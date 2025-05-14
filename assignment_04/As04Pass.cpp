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
#include "llvm/IR/Dominators.h"
#include <iostream>
#include <algorithm>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG 3

#if DEBUG == 0
#define D1(x)
#define D2(x)
#define D3(x)
#elif DEBUG == 1
#define D1(x) llvm::outs() << x << '\n';
#define D2(x)
#define D3(x)
#elif DEBUG == 2
#define D1(x) llvm::outs() << x << '\n';
#define D2(x) D1(x)
#define D3(x)
#elif DEBUG == 3
#define D1(x) llvm::outs() << x << '\n';
#define D2(x) D1(x)
#define D3(x) D1(x)
#endif

/*
* The function verifies if there is AT LEAST ONE instruction in the second loop preheader (which is also 
* the exit block of the precedent loop) that has uses inside the second loop.
* Note: we check this condition because if none of the instructions beetween the loop (outside)
* are used we can join loops even if not "directly" adjacent (the instructions then have to be moved after the second loop).
*/
// bool hasUsesInsideLoop(Loop &L){

//   BasicBlock *preHeader = L.getLoopPreheader();

//   for ( auto &I: *preHeader ){

//     D1( "\t Current instruction: " << I)

//     if ( !isa<BranchInst>(I) ){

//       for (auto useIt = I.use_begin(); useIt != I.use_end(); ++useIt) {
//         User *use = useIt->getUser();
//         Instruction *inst = dyn_cast<Instruction>(use);

//         D3("\tChecking " << I << "'s use: " << *inst )

//         if ( L.contains(inst) ){
//           D1("\t Instruction has use inside second loop")
//           return true;
//         }
//       }
//     }
//   }

//   return false;
// }



bool adjacentLoops(Loop &l1, Loop &l2){

  // get loop 1 exit block
  BasicBlock *exitBBl1 = l1.getExitBlock();
  // get loop 2 preheader block
  BasicBlock *preHeaderBBl2 = l2.getLoopPreheader();


  // guarded check
  if ( l1.isGuarded() ){



    D1( "\t Found guarde loop" )

  } else {
    // if more than one exit block on l1 return nullptr
    if ( !exitBBl1 ){
      D1("\t More than one exit block for the loop")
      return false;
    }

    D1("\t Loop1 exit block: " << *exitBBl1 )
    D1("\t Loop2 preheader block: " << *preHeaderBBl2)

    if ( exitBBl1 != preHeaderBBl2 ){
      return false;
    } else if ( hasUsesInsideNextLoop(l2) ){
      return false;
    }
  }
  
  D2( "\t Found adjacent loops! " )
  return true;

}



//-----------------------------------------------------------------------------
// TestPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

// New PM implementation
struct As04Pass: PassInfoMixin<As04Pass> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {

    // Get loop info from function
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    // dominators
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    // small vector of loops
    // SmallVector<Loop *> Worklist;

    /*
    * momentaneamente lavoriamo sui loop più esterni, bisogna verificare se ogni loop 
    * contiene altri loop e nel caso eseguire anche per gli interni dove possibile
    */

    // reverse iterate over loops beacuse the first one is the last in the program 
    for ( auto it = LI.rbegin() ; it != LI.rend()-1; ++it ){
      Loop *loop1 = *it;
      Loop *loop2 = *(it+1);

      D1("Loop1 header: " << *loop1->getHeader());
      D1("Loop2 header: " << *loop2->getHeader());



      // First check: loop are adjacent
      adjacentLoops(*loop1, *loop2);




      // REMEMBER: move instructions with no uses inside the "second" loop to begin of exit BB of second loop 

    
    
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
  return {LLVM_PLUGIN_API_VERSION, "As04Pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "lf-pass") {
                    FPM.addPass(As04Pass());
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
