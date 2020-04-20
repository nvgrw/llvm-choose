//===-- ChooseLowering.cpp - Choose translation ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that lowers choose statements to switch in lieu
// of a choose-specific code generator and in order to apply optimizations
// specific to the switch instruction without duplication.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/PassSupport.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

namespace {

/// ChooseLowering - This pass rewrites occurrences of the choose instruction,
/// replacing them with equivalent switch statements.
class ChooseLowering : public FunctionPass {

public:
  static char ID;

  ChooseLowering();
  StringRef getPassName() const override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool doInitialization(Module &M) override;
  bool runOnFunction(Function &F) override;
};

}

// -----------------------------------------------------------------------------

INITIALIZE_PASS_BEGIN(ChooseLowering, "choose-lowering", "Choose Lowering", false, false)
INITIALIZE_PASS_END(ChooseLowering, "choose-lowering", "Choose Lowering", false, false)

FunctionPass *llvm::createChooseLoweringPass() { return new ChooseLowering(); }

char ChooseLowering::ID = 0;

ChooseLowering::ChooseLowering() : FunctionPass(ID) {
  initializeChooseLoweringPass(*PassRegistry::getPassRegistry());
}

StringRef ChooseLowering::getPassName() const {
  return "Lower Choose Instructions";
}

void ChooseLowering::getAnalysisUsage(AnalysisUsage &AU) const {
  FunctionPass::getAnalysisUsage(AU);
  // TODO(nvgrw): fill with dependencies
}

bool ChooseLowering::doInitialization(Module &M) {
  return false;
}

bool ChooseLowering::runOnFunction(Function &F) {
  bool MadeChange = false;

  for (BasicBlock &BB : F) { // for each basic block
    for (BasicBlock::iterator II = BB.begin(), E = BB.end(); II != E;) {
      // for each instruction
      ChooseInst *CI = dyn_cast<ChooseInst>(II++);
      if (!CI)
        continue;

      uint64_t RandomUpperBound = 0;
      for (auto &Choice : CI->choices()) {
        RandomUpperBound += Choice.getChoiceWeight()->getZExtValue();
      }

      IRBuilder<> Builder(CI);
      const std::vector<Value *> args = {Builder.getInt64(RandomUpperBound)};
      const std::vector<Type *> args_type;
      Function *rand_uniform = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::rand_uniform, args_type);
      SwitchInst *SI =
          Builder.CreateSwitch(Builder.CreateCall(rand_uniform, args),
                               CI->getDefaultDest(), CI->getNumChoices());

      uint64_t WeightCount = 0;
      for (auto &Choice : CI->choices()) {
        unsigned Weight = Choice.getChoiceWeight()->getZExtValue();
        if (Choice.getChoiceIndex() == 0) { // default (first) case
          WeightCount += Weight;
          continue;
        }

        for (unsigned i = WeightCount; i < WeightCount + Weight; i++) {
          SI->addCase(Builder.getInt64(i), Choice.getChoiceSuccessor());
        }
        WeightCount += Weight;
      }

      CI->replaceAllUsesWith(SI);
      CI->eraseFromParent();
      MadeChange = true;
    }
  }

  return MadeChange;
}

