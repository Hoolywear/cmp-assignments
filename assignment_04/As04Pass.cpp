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
#include "llvm/Analysis/PostDominators.h"
#include <iostream>
#include <algorithm>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG 3

#define D1_COLOR raw_ostream::WHITE
#define D2_COLOR raw_ostream::BLUE
#define D3_COLOR raw_ostream::RED

#if DEBUG == 0
#define D1(x)
#define D2(x)
#define D3(x)
#elif DEBUG == 1
#define D1(x) llvm::outs().changeColor(D1_COLOR) << x << '\n'; outs().resetColor();
#define D2(x)
#define D3(x)
#elif DEBUG == 2
#define D1(x) llvm::outs().changeColor(D1_COLOR) << x << '\n'; outs().resetColor();
#define D2(x) llvm::outs().changeColor(D2_COLOR) << x << '\n'; outs().resetColor();
#define D3(x)
#elif DEBUG == 3
#define D1(x) llvm::outs().changeColor(D1_COLOR) << x << '\n'; outs().resetColor();
#define D2(x) llvm::outs().changeColor(D2_COLOR) << x << '\n'; outs().resetColor();
#define D3(x) llvm::outs().changeColor(D3_COLOR) << x << '\n'; outs().resetColor();
#endif

/*
* Function that retrieves the guard BB from a loop, if present.
*/

BasicBlock *getGuardBlock(Loop &L) {
  if ( L.isGuarded() ){
    D3( "\t(getGuardBlock) Loop is guarded, retrieving its guard BB" )
    // retrieve guard branch
    return L.getLoopGuardBranch()->getParent();
  }
  D3("\t(getGuardBlock) Loop is not guarded")
  return nullptr;
}

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

/*
* The function checks if the guard blocks of the guarded loops have identical conditions
*/
bool checkGuardCondition(BranchInst *branch1, BranchInst *branch2) {
  // get compare from branch instruction
  Instruction *compInst1 = dyn_cast<Instruction>(branch1->getCondition());
  Instruction *compInst2 = dyn_cast<Instruction>(branch2->getCondition());

  D3( "\t\tCompInst 1: " << *compInst1 )
  D3( "\t\tCompInst 2: " << *compInst2 )

  if ( !compInst1->isIdenticalTo(compInst2) ){
    D2( "\t(checkGuardCondition) Guarded loops don't have identical conditions " )
    return false;
  }
  D2( "\t(checkGuardCondition) Guarded loops have identical conditions " )
  return true;
}


/*
* The function checks if loops are both guarded or not and if there are statements between the loops
*/
bool areAdjacentLoops(Loop &l1, Loop &l2) {

  // Loops need to be both guarded or not
  BasicBlock *guard1 = getGuardBlock(l1);
  BasicBlock *guard2 = getGuardBlock(l2);

  // if one loop is guarded and the other isn't return false
  if ((guard1 && !guard2) || (!guard1 && guard2)) {
    D2("Not both guarded or not guarded")
      return false;
  }

  // if both guarded
  if ( guard1 && guard2 ){

    D1( "\t Loops are both guarded" )
    D2( "\t Guard block of the first loop" << *guard1 )
    D2( "\t Guard block of the Second loop" << *guard2 )

    // check if loop guard block have identical condition
    if ( !checkGuardCondition(l1.getLoopGuardBranch(), l2.getLoopGuardBranch()) ){
      D1( "\t Guarded loops don't have identical conditions " )
      return false;
    }
    D1( "\t Guarded loops have identical conditions " )

    BranchInst *guardBranch = l1.getLoopGuardBranch();

    // iterate on the two successor BBs and verify if the exit of the first loop
    // is the guard BB of the secondo loop
    for (auto S: guardBranch->successors()) {
      if ( (l2.getLoopGuardBranch()->getParent()) == S ) {
        D2("\t One of the successor of the first guard branch is the second loop guard BB " << *S)
        
        
        for ( auto &I: *S ){
          /*
          * checks if there are instructions between loop.
          * NOTE TO CHECK: we assume that a guarded form loop always has an if before the loop, so we check
          * if there are instructions different from branches or compares
          */
          if ( !isa<BranchInst>(I)  && !isa<CmpInst>(I) ){
            D2("\t => return false because guaded loops are not adjacent ")
            return false;
          }
        }
      }
    }
  } else { // if they are both not guarded

    D1( "\t Both loops are not guarded " )
    
    // if more than one exit block on l1 return nullptr
    if ( !l1.getExitBlock() ){
      D1("\t More than one exit block for the loop ")
      return false;
    }

    // normal form loops (only case we consider) always have the preheader
    if ( l1.getExitBlock() == l2.getLoopPreheader() ){

      D2( " \t The exit block of the first loop is the preheader of the second loop " )

      // check if there are no statements between loops (the first instruction is a branch)
      if( !isa<BranchInst>(l1.getExitBlock()->begin()) ){
      D2 ( " \t => return false beacuse loops are not adjacent " )
        return false;
      }
    }
  }
  return true;
}


/*
* Function that checks if the two loops are control-flow equivalent.
* - if the loops are both guarded we need to check if the l1 guard dominates l2 guard AND l2 gaurd postdominates l1 guard
* - if the loops are bot not guarded we need to check if l1 preheader dominates l2 header AND if l2 preheader postdominates l1 header
*/
bool areControlFlowEq(Loop &l1, Loop &l2, DominatorTree &DT, PostDominatorTree &PDT) {

  if ( l1.isGuarded() && l2.isGuarded() ) {
    if ( DT.dominates(getGuardBlock(l1), getGuardBlock(l2)) && PDT.dominates( getGuardBlock(l2), getGuardBlock(l1) ) ) {
      D2( "\tGuarded loops are control flow equivalent " )
      return true;
    }
  } else if ( !l1.isGuarded() && !l2.isGuarded() ) { 
    
    if ( DT.dominates(l1.getLoopPreheader(), l2.getLoopPreheader()) && PDT.dominates( l2.getLoopPreheader(), l1.getLoopPreheader() ) ) {
      D2( "\tLoops are control flow equivalent " )
      return true;
    } 

  }


  return false;
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
    // dominators and postdominators
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);

    // small vector of loops
    // SmallVector<Loop *> Worklist;

    /*
    * momentaneamente lavoriamo sui loop più esterni, bisogna verificare se ogni loop 
    * contiene altri loop e nel caso eseguire anche per gli interni dove possibile
    */

    // reverse iterate over loops beacuse the first one is the last in the program 
    for ( auto it = LI.rbegin() ; it != LI.rend()-1; ++it ){
      D1("--> ENTERING LOOP PAIR ANALYSIS <--")
      Loop *loop1 = *it;
      Loop *loop2 = *(it+1);

      D1("Loop1 header: " << *loop1->getHeader());
      D1("Loop2 header: " << *loop2->getHeader());
      D2("┌───────┬────┬────┐")
      D2("│\t│ L1 │ L2 │")
      D2("├───────┼────┼────┤")
      D2("│GUARDED│ " << loop1->isGuarded() << "  │ " << loop2->isGuarded() << "  │")
      D2("│ROTATED│ " << loop1->isRotatedForm() << "  │ " << loop2->isRotatedForm() << "  │")
      D2("└───────┴────┴────┘")
      
      // First check: loops are adjacent
      if (!areAdjacentLoops(*loop1, *loop2)) {
        D1("LOOPS ARE NOT ADJACENT - CONTINUE WITH NEXT LOOP PAIR\n=============================================")
        continue;
      }
      if (!areControlFlowEq(*loop1, *loop2, DT, PDT)) {
        D1("LOOPS ARE NOT CONTROL FLOW EQUIVALENT - CONTINUE WITH NEXT LOOP PAIR\n=============================================")
        continue;
      }
      D1("LOOPS PASSED ALL CHECKS - CONTINUE WITH NEXT LOOP PAIR\n=============================================")
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
