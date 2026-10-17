#include <vector>
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Constants.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

namespace {


class DependenceGraph {
public:

  DependenceGraph() {
  }

  llvm::DenseSet<llvm::Value*> compute_sideffectful_values() {
    llvm::DenseSet<llvm::Value*> visited;

    for(llvm::Value* return_node : return_nodes) {
      DFS(return_node, visited);
    }

    for(llvm::Value* memory_address : memory_adresses) {
      DFS(memory_address, visited);
    }

    return visited;
  }

  void DFS(llvm::Value* vertex, llvm::DenseSet<llvm::Value*>& visited) {
    if(visited.contains(vertex)) return;

    visited.insert(vertex);
    for(llvm::Value* neighbor : graph[vertex]) {
        DFS(neighbor, visited);
    }
  }


  void add_edge(llvm::Value* source_vertex, llvm::Value* destination_vertex) {
    if(!graph.contains(source_vertex)) {
      graph.insert({source_vertex, {}});
    }

    
    if(!graph.contains(destination_vertex)) {
      graph.insert({destination_vertex, {}});
    }

    graph[source_vertex].insert(destination_vertex);
  }

  void set_return_node(llvm::Value* return_node) {
    return_nodes.insert(return_node);
  }

  void set_memory_address(llvm::Value* memory_address) {
    memory_adresses.insert(memory_address);
  }


void print() const {
    llvm::errs() << "=== Dependence Graph ===\n";

    for (const auto &[source, destinations] : graph) {
        llvm::errs() << "Source: ";
        source->print(llvm::errs());
        llvm::errs() << "\n";

        for (llvm::Value *destination : destinations) {
            llvm::errs() << "  -> ";
            destination->print(llvm::errs());
            llvm::errs() << "\n";
        }
    }

    llvm::errs() << "\n=== Return Nodes ===\n";

    for (llvm::Value *return_node : return_nodes) {
        llvm::errs() << "  ";
        return_node->print(llvm::errs());
        llvm::errs() << "\n";
    }

    llvm::errs() << "\n=== Memory Addresses ===\n";

    for (llvm::Value *memory_address : memory_adresses) {
        llvm::errs() << "  ";
        memory_address->print(llvm::errs());
        llvm::errs() << "\n";
    }
}

private:
  llvm::DenseMap<llvm::Value*, llvm::DenseSet<llvm::Value*>> graph;
  llvm::DenseSet<llvm::Value*> return_nodes;
  llvm::DenseSet<llvm::Value*> memory_adresses;
};

class MyPass : public llvm::detail::PassInfoMixin<MyPass> {
public:
  static bool isRequired() { return true; }

  bool is_arithmetic_instruction(llvm::Instruction &instruction) {
    return (instruction.isBinaryOp() && !instruction.mayReadOrWriteMemory()) || 
    (llvm::isa<llvm::ICmpInst>(instruction));
  }

  void process_arithmetic_instruction(llvm::Instruction &instruction, DependenceGraph &graph) {
    llvm::Value* operand1 = instruction.getOperand(0);
    llvm::Value* operand2 = instruction.getOperand(1);

    llvm::Value* result = &instruction;

    if(!llvm::isa<llvm::Constant>(operand1)) graph.add_edge(result, operand1);
    if(!llvm::isa<llvm::Constant>(operand2)) graph.add_edge(result, operand2);
  }

  void process_return_instruction(llvm::ReturnInst &instruction, DependenceGraph &graph) {
    llvm::Value* return_value = instruction.getReturnValue();
    if(!llvm::isa<llvm::Constant>(return_value)) graph.add_edge(&instruction, return_value);

    graph.set_return_node(&instruction);
  }

  void process_load_instruction(llvm::LoadInst &instruction, DependenceGraph &graph) {
    llvm::Value* memory_adress = instruction.getPointerOperand();
    
    graph.set_memory_address(memory_adress);
    graph.add_edge(&instruction, memory_adress);
  }

  void process_store_instruction(llvm::StoreInst &instruction, DependenceGraph &graph) {
    llvm::Value* memory_adress = instruction.getPointerOperand();
    llvm::Value* store_value = instruction.getValueOperand();

    graph.set_memory_address(memory_adress);
    graph.add_edge(memory_adress, store_value);
  }

  void processs_conditional_branch_instruction(llvm::CondBrInst &instruction, DependenceGraph &graph) {
    llvm::Value* condition = instruction.getCondition();

    for(llvm::BasicBlock* jumping_block : instruction.successors()) {

      for(llvm::Instruction &jumping_block_instruction : *jumping_block) {
        if(is_arithmetic_instruction(jumping_block_instruction)) {
          graph.add_edge(&jumping_block_instruction, condition);
        }

        // TODO: change the names here, they are very bad.
        else if(auto *branch_instruction = llvm::dyn_cast<llvm::CondBrInst>(&jumping_block_instruction)) {
          graph.add_edge(branch_instruction->getCondition(), condition);
        }

        else if(auto *load_instruction = llvm::dyn_cast<llvm::LoadInst>(&jumping_block_instruction)) {
          graph.add_edge(load_instruction, condition);
        }

        else if(auto *store_instruction = llvm::dyn_cast<llvm::StoreInst>(&jumping_block_instruction)) {
          graph.add_edge(store_instruction->getPointerOperand(), condition);
        }

        else if(auto* return_instruction = llvm::dyn_cast<llvm::ReturnInst>(&jumping_block_instruction)) {
          graph.add_edge(return_instruction, condition);
        }
      }
    }
  }

  void process_instruction(llvm::Instruction &instruction, DependenceGraph &graph) {
    if(is_arithmetic_instruction(instruction)) {
      process_arithmetic_instruction(instruction, graph);
    }

    else if(auto *branch_instruction = llvm::dyn_cast<llvm::CondBrInst>(&instruction)) {
      processs_conditional_branch_instruction(*branch_instruction, graph);
    }

    else if(auto *load_instruction = llvm::dyn_cast<llvm::LoadInst>(&instruction)) {
      process_load_instruction(*load_instruction, graph);
    }

    else if(auto *store_instruction = llvm::dyn_cast<llvm::StoreInst>(&instruction)) {
      process_store_instruction(*store_instruction, graph);
    }

    else if(auto* return_instruction = llvm::dyn_cast<llvm::ReturnInst>(&instruction)) {
      llvm::Value* return_value = return_instruction->getReturnValue();
      process_return_instruction(*return_instruction, graph);
    }
  }

  void process_basic_block(llvm::BasicBlock &basic_block, DependenceGraph &graph) {
    for(llvm::Instruction &instruction : basic_block) {
      process_instruction(instruction, graph);
    }
  }

  DependenceGraph build_dependece_graph(Function& function) {
    DependenceGraph graph;
    for(llvm::BasicBlock &basic_block : function) {
      process_basic_block(basic_block, graph);
    }
 
    return graph;
  }

  std::vector<llvm::Instruction*> get_dead_instructions(llvm::Function &function, llvm::DenseSet<llvm::Value*> &sideffectful_values) {
    std::vector<llvm::Instruction*> dead_instructions;

    for(llvm::Instruction &instruction : llvm::instructions(function)) {
      if(is_arithmetic_instruction(instruction)) {
        if(!sideffectful_values.contains(&instruction)) dead_instructions.push_back(&instruction);
      }

      else if(auto* load_instruction = llvm::dyn_cast<llvm::LoadInst>(&instruction)) {
        if(!sideffectful_values.contains(&instruction)) dead_instructions.push_back(&instruction);
      }
    }

    return dead_instructions;
  }
    

  void apply_dead_code_elimination(Function& function) {
    DependenceGraph graph = build_dependece_graph(function);
    graph.print();
    
    llvm::DenseSet<llvm::Value*> sideffectful_values = graph.compute_sideffectful_values();

    
    std::vector<llvm::Instruction*> dead_instructions = get_dead_instructions(function, sideffectful_values);

    for (llvm::Instruction *I : dead_instructions) {
        if (!I->getType()->isVoidTy()) {
            I->replaceAllUsesWith(llvm::PoisonValue::get(I->getType()));
        }
    }

    for (llvm::Instruction *I : dead_instructions) {
        I->eraseFromParent();
    }
  }
  
  PreservedAnalyses run(Function &F, FunctionAnalysisManager & manager) {
    //errs() << "MyPass executado na funcao: " << F.getName() << "\n";
    apply_dead_code_elimination(F);

    return PreservedAnalyses::all();
  }
};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION,
      "MyPasses",
      LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name != "my-pass")
                return false;

              FPM.addPass(MyPass());
              return true;
            });
      }};
}
