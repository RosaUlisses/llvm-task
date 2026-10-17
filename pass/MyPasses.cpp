#include <vector>
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/PassManager.h"
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
    return_nodes.push_back(return_node);
  }

  #include "llvm/Support/raw_ostream.h"

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
}

private:
  llvm::DenseMap<llvm::Value*, llvm::DenseSet<llvm::Value*>> graph;
  std::vector<llvm::Value*> return_nodes;
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

        // TODO: deal with return instruction 
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

  void build_dependece_graph(Function& function) {
    DependenceGraph graph;
    for(llvm::BasicBlock &basic_block : function) {
      process_basic_block(basic_block, graph);
    }

    graph.print();
  }

  void apply_dead_code_elimination(Function& function) {
    build_dependece_graph(function);
    // 1- build dependece graph
    // 2- perform DFS
    // 3- remove useless instructions
  }
  
  PreservedAnalyses run(Function &F, FunctionAnalysisManager & manager) {
    //errs() << "MyPass executado na funcao: " << F.getName() << "\n";
    apply_dead_code_elimination(F);

    return PreservedAnalyses::all();
  }
};

} // namespace

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
