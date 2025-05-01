//=============================================================================
// FILE:
//    TestPass.cpp
//
// DESCRIPTION:
//    Visits all functions in a module and prints their names. Strictly speaking, 
//    this is an analysis pass (i.e. //    the functions are not modified). However, 
//    in order to keep things simple there's no 'print' method here (every analysis 
//    pass should implement it).
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=<path-to>libTestPass.so -passes="test-pass" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG

#ifdef DEBUG
#define D(x) llvm::outs() << x << '\n';
#else
#define D(x)
#endif

bool isLoopInvInstr(Instruction &I, vector<Instruction*> &linvInstr, Loop &L);

// Function that checks wether an operand from a BinaryOp is considered loop-invariant
bool isLoopInvOp(Value *OP, vector<Instruction*> &linvInstr, Loop &L) {
  D("\t\tEntered isLoopInvOp for " << *OP);


  if ( isa<ConstantInt>(OP) ) {
    D("\t\tOperand is constant -> labeling as linv");
    return true;
  }else if ( dyn_cast<Argument>(OP) ){
    D("\t\tOperand is a function argument -> labeling as linv")
    return true;
  }

  // Cast from Value to Instruction to perform checks
  if (dyn_cast<Instruction>(OP)  ) {
    Instruction *OpInst = dyn_cast<Instruction>(OP);

    if ( find(linvInstr.begin(), linvInstr.end(), OpInst) != linvInstr.end() ) {
      D("\t\tOperand already labeled as loop-invariant");
      return true;
    } else if (!L.contains(OpInst)) {
      D("\t\tOperand defined outside the loop -> labeling as linv");
      return true;
    } else if (!OpInst->isBinaryOp()) { // If inside loop, not already linv, not constant, and not BinaryOp, then is not linv
      D("\t\tOperand defined inside the loop and not a BinaryOp -> not linv");
      return false;
    } else if (isLoopInvInstr(*OpInst, linvInstr, L)) { // Cannot determine wether the operand is linv or not: recursive call to isLoopInvInstr(OPERATOR)
      return true;
    }
  }

  D("FALLBACK EXIT");
  return false;
}

// Function that checks wether a BinaryOp from a loop is considered loop-invariant
bool isLoopInvInstr(Instruction &I, vector<Instruction*> &linvInstr, Loop &L) {
  // Retrieve operands
  Value *op1 = I.getOperand(0);
  Value *op2 = I.getOperand(1);

  D("\tBINARY OP:");
  D("\tOperand 1: " << *op1 );
  D("\tOperand 2: " << *op2 );

  // Check if both are loop-invariant; if so, the instruction itself can be considered loop-invariant
  if (isLoopInvOp(op1, linvInstr, L) && isLoopInvOp(op2, linvInstr, L)) {
    D("\tBoth operands are linv -> label as linv")
    return true;
  }

  return false;
}

// Function to retrieve loop-invariant instructions from a specific loop
void getLoopInvInstructions(vector<Instruction*> &linvInstr, Loop &L) {
  // TODO: implement logic to iterate over nested loops

  // Iterate over loop instructions (via its BBs first)
  for (Loop::block_iterator BI = L.block_begin(); BI != L.block_end(); ++BI) {
    BasicBlock *B = *BI;
    // Iterate over BB instructions
    for (auto &I: *B) {
      D(I);
      // Check for loop invariance applies only to BinaryOp instructions
      if (I.isBinaryOp() && isLoopInvInstr(I, linvInstr, L)) {
        D("\t^ IS LOOP INVARIANT OP ^");
        linvInstr.push_back(&I);
      }
    }
  }

  return;
}

//-----------------------------------------------------------------------------
// TestPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

// New PM implementation
struct As03Pass: PassInfoMixin<As03Pass> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    // Get loop info from function
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    // Instruction vectors to be reused for each loop
    vector<Instruction*> loopInvInstr;
    
    // Iterate on all TOP-LEVEL loops from function
    for (auto &L: LI) {
      getLoopInvInstructions(loopInvInstr, *L);


      #ifdef DEBUG
      D("======\nLoop-independent instructions:\n======")
      for (auto I: loopInvInstr)
        D(*I);
      #endif
    }

  	return PreservedAnalyses::all();
}


  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "As03Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "licm-pass") {
                    FPM.addPass(As03Pass());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize TestPass when added to the pass pipeline on the
// command line, i.e. via '-passes=test-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getTestPassPluginInfo();
}
