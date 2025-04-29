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
#include <iostream>
#include <vector>

using namespace llvm;
using namespace std;


void InitializeLoopInst(std::vector<Instruction*> &LoopInst, Loop *L){
  // Per ogni basic block nel loop
  for (Loop::block_iterator BI = L->block_begin(); BI != L->block_end(); ++BI){
    BasicBlock *B = *BI;
    // Per ogni istruzione nel basic block
    for (auto &I : *B){
      LoopInst.push_back(&I);  // Aggiungi puntatore all'istruzione
    }
  }
}


/*
* The function find the Loop Invariant Instructions
* and iterate until convergent
* Return the vector with the Loop Invariant Instructions
*/
vector<Instruction*> FindLoopInv(Loop &L) {
  vector<Instruction*> LoopInst;
  vector<Instruction*> NonLoopInv_inst;
  vector<Instruction*> LoopInv_inst;

  InitializeLoopInst(LoopInst, &L);

  // Qui dovrebbe esserci la logica per trovare le istruzioni invarianti

  return LoopInv_inst;
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
  
  /* APPUNTI
   *	andiamo ad estendere questa funzione
   */
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    // Get loop info
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    vector<Instruction*> LoopInv_inst;
    // loop iterator
    for (auto &L : LI) {
      LoopInv_inst = FindLoopInv(*L);
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
