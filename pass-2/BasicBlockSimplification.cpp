#include <vector>
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Constants.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

namespace {

class BasicBlockSimplification : public llvm::detail::PassInfoMixin<BasicBlockSimplification> {
public:
  static bool isRequired() { return true; }

  bool is_trivial_jump(llvm::BasicBlock &block) {
    return block.size() == 1 && llvm::dyn_cast<llvm::UncondBrInst>(&block.front()) && &block != block.getTerminator()->getSuccessor(0);
  }

  void remove_jumps_to_dead_block(llvm::BasicBlock &dead_block) {
    llvm::UncondBrInst* branch_instruction = llvm::dyn_cast<llvm::UncondBrInst>(&dead_block.front());
    llvm::BasicBlock& goto_block = *branch_instruction->getSuccessor();

    std::vector<llvm::Instruction*> terminator_instructions;
    for(llvm::BasicBlock* predecessor_block : llvm::predecessors(&dead_block)) {
      terminator_instructions.push_back(predecessor_block->getTerminator());
    }

    for(llvm::Instruction* instruction : terminator_instructions) {
      instruction->replaceSuccessorWith(&dead_block, &goto_block);
    }
  }

  void remove_dead_basic_blocks(llvm::Function &function) {
    std::vector<BasicBlock*> dead_blocks;

    for(llvm::BasicBlock &block : function) {
      if(&block == &function.getEntryBlock()) continue;

      if(is_trivial_jump(block)) {
        remove_jumps_to_dead_block(block);
        dead_blocks.push_back(&block);
      }
    }

    for(llvm::BasicBlock* block : dead_blocks) {
      block->eraseFromParent();
    }
  }

  bool all_successors_are_equal(llvm::Instruction& branch_instruction) {
    llvm::BasicBlock* first_successor = branch_instruction.getSuccessor(0);

    for(int i = 0; i < branch_instruction.getNumSuccessors(); i++) {
      if(branch_instruction.getSuccessor(i) != first_successor) return false;
    }

    return true;
  }

  void remove_redundant_conditional_branchings_from_basic_block(llvm::BasicBlock& basic_block) {
    std::vector<Instruction*> redundant_branchings;
    for(llvm::Instruction& instruction : basic_block) {
      if(llvm::isa<llvm::CondBrInst, SwitchInst>(instruction)) {
        if(all_successors_are_equal(instruction)) {
          redundant_branchings.push_back(&instruction);
        }
      }
    }

    for(llvm::Instruction* instruction : redundant_branchings) {
      llvm::BasicBlock& go_to_block = *instruction->getSuccessor(0);
      llvm::UncondBrInst* unconditional_jump = llvm::UncondBrInst::Create(&go_to_block);

      llvm::ReplaceInstWithInst(instruction, unconditional_jump);
    }
  }

  void simplify_redundant_conditional_branchings(llvm::Function& function) {
    for(llvm::BasicBlock& basic_block : function) {
      remove_redundant_conditional_branchings_from_basic_block(basic_block);
    }
  }

  void apply_basic_block_simplification(llvm::Function &function) {
    remove_dead_basic_blocks(function);
    simplify_redundant_conditional_branchings(function);
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager & manager) {
    apply_basic_block_simplification(F);

    return PreservedAnalyses::all();
  }
};

}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION,
      "BasicBlockSimplification",
      LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name != "BasicBlockSimplification")
                return false;

              FPM.addPass(BasicBlockSimplification());
              return true;
            });
      }};
}
