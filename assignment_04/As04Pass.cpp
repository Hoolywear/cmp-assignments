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
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include <iostream>
#include <algorithm>
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG 3

#define D1_COLOR raw_ostream::BRIGHT_YELLOW
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

//     D1( "\tCurrent instruction: " << I)

//     if ( !isa<BranchInst>(I) ){

//       for (auto useIt = I.use_begin(); useIt != I.use_end(); ++useIt) {
//         User *use = useIt->getUser();
//         Instruction *inst = dyn_cast<Instruction>(use);

//         D3("\tChecking " << I << "'s use: " << *inst )

//         if ( L.contains(inst) ){
//           D1("\tInstruction has use inside second loop")
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
    D3( "\t(checkGuardCondition) Guarded loops don't have identical conditions " )
    return false;
  }
  D3( "\t(checkGuardCondition) Guarded loops have identical conditions " )
  return true;
}


/*
* The function checks if loops are both guarded or not and if there are statements between the loops
*/
bool areAdjacentLoops(Loop &l1, Loop &l2) {
  D2("--- START ADJACENCY CHECK ---")
  // Try to retrieve both guard branch instructions
  BranchInst *guardBranch1 = l1.getLoopGuardBranch();
  BranchInst *guardBranch2 = l2.getLoopGuardBranch();

  // Logic to retrieve correct "adjacent BB candidates" depending if loops are both guarded or not
  if (guardBranch1 && guardBranch2 && checkGuardCondition(guardBranch1, guardBranch2)) {
    D2( "\tLoops are both guarded and with identical conditions" )
    // iterate on the two successor BBs and verify if one exit of the first loop
    // is the guard BB of the second loop
    for (auto S: guardBranch1->successors()) {
      if ( (guardBranch2->getParent()) == S ) {
        D3("\tFirst loop's guard exit is the second loop's guard BB " << *S)
    
        // Check if first BB instruction is the branch condition
        if (dyn_cast<Instruction>(S->begin()) != dyn_cast<Instruction>(guardBranch2->getCondition())) {
          D2("\tLoops are not adjacent - EXIT CHECK WITH FALSE")
          return false;
        }
      }
    }
    D1("\tALL GUARD CHECKS WENT GOOD, BUT GUARDED LOOP FUSION IS NOT (YET) SUPPORTED - EXIT CHECK WITH FALSE")
    return false;
  } else if (!guardBranch1 && !guardBranch2) {
    
    D2( "\tBoth loops are not guarded" )
    
    // The first loop must have a single exit block
    if ( !l1.getExitBlock() ){
      D2("\tMore than one exit block for the loop - EXIT CHECK WITH FALSE")
      return false;
    }

    // normal form loops (only case we consider) always have the preheader
    if ( l1.getExitBlock() == l2.getLoopPreheader() ){
      D3( "\tThe exit block of the first loop is the preheader of the second loop " )
      // check if there are no statements between loops (the first instruction is a branch)
      if( !isa<BranchInst>(l1.getExitBlock()->begin()) ){
        D2 ( "\tLoops are not adjacent - EXIT CHECK WITH FALSE" )
        return false;
      }
    }
  } else {
    // Loops are not both guarded or both not guarded (acts as a fallback exit condition)
    D2("\tNot both guarded or not guarded - EXIT CHECK WITH FALSE")
    return false;
  }
  
  // All checks passed
  D2 ( "\tLoops are adjacent - EXIT CHECK WITH TRUE" )
  return true;
}
      
      
/*
* Function that checks if the two loops are control-flow equivalent.
* - if the loops are both guarded we need to check if the l1 guard dominates l2 guard AND l2 gaurd postdominates l1 guard
* - if the loops are bot not guarded we need to check if l1 preheader dominates l2 header AND if l2 preheader postdominates l1 header
*/
bool areControlFlowEq(Loop &l1, Loop &l2, DominatorTree &DT, PostDominatorTree &PDT) {
  D2("--- START CTRL FLOW EQUIVALENCY CHECK ---")
  if ( l1.isGuarded() && l2.isGuarded() ) {
    if ( DT.dominates(getGuardBlock(l1), getGuardBlock(l2)) && PDT.dominates( getGuardBlock(l2), getGuardBlock(l1) ) ) {
      D2( "\tGuarded loops are control flow equivalent - EXIT CHECK WITH TRUE" )
      return true;
    }
  } else if ( !l1.isGuarded() && !l2.isGuarded() ) { 
    if ( DT.dominates(l1.getLoopPreheader(), l2.getLoopPreheader()) && PDT.dominates( l2.getLoopPreheader(), l1.getLoopPreheader() ) ) {
      D2( "\tLoops are control flow equivalent - EXIT CHECK WITH TRUE" )
      return true;
    } 
  }
  
  D2( "\tLoops are not control flow equivalent - EXIT CHECK WITH FALSE" )
  return false;
}

/*
* Function that checks if two loops iterate the same amount of times
*/
bool iterateEqualTimes(Loop &l1, Loop &l2, ScalarEvolution &SE) {
  D2("--- START ITERATION NUMBER CHECK ---")
  const SCEV *itTimes1 = SE.getBackedgeTakenCount(&l1);
  const SCEV *itTimes2 = SE.getBackedgeTakenCount(&l2);
  
  if (isa<SCEVCouldNotCompute>(itTimes1) || isa<SCEVCouldNotCompute>(itTimes2)) {
    D2("\tCannot determine iteration count for one of the loops - EXIT CHECK WITH FALSE")
    return false;
  }
  D2( "\tBack-edge count of L1: " << *itTimes1 << "\tSCEV type: " << *itTimes1->getType() )
  D2( "\tBack-edge count of L2: " << *itTimes2 << "\tSCEV type: " << *itTimes2->getType() )

  D2("\tminus: " << SE.getMinusSCEV(itTimes1,itTimes2)->isZero())

  if (itTimes1 == itTimes2) {
    D2("\tLoops iterate the same amount of times - EXIT CHECK WITH TRUE")
    return true;
  }
  
  D2("\tLoops do not iterate the same amount of times - EXIT CHECK WITH FALSE")
  return false;
}

/*
* Function that returns load/store instructions of the loop
* if isLoad is true return Load instructions, otherwise return store instructions
*/
vector<Instruction*> getMemInst(Loop &l, bool isLoad){
  vector<Instruction*> memInsts;
  // iterate over basic blocks of loops
  for ( Loop::block_iterator BI = l.block_begin(); BI != l.block_end(); ++BI ) {
    BasicBlock *B = *BI;
    // iterate over instructions in the basic block
    for ( auto &I: *B ){
      // check if the instruction is a load or a store
      if ( isLoad && isa<LoadInst>(I)){
        D3("\t\tFound a load instruction: " << I );
        memInsts.push_back(&I);
      }else if( !isLoad && isa<StoreInst>(I)){
        D3("\t\tFound a store instruction: " << I );
        memInsts.push_back(&I);
      }
    }
  }
  return memInsts;
}

/*
* function that checks if the loops have negative distance dependencies
*/

const SCEV *getSCEVwithNoPtr(Value *memPtr, Loop &L, ScalarEvolution &SE) {
  if (const SCEV *S = SE.removePointerBase(SE.getSCEVAtScope(memPtr, &L))) {
    return S;
  }
  return nullptr;
}



bool haveNegativeDistance(Loop &l1, Loop &l2, ScalarEvolution &SE){

  vector<Instruction*> storeInsts1 = getMemInst(l1, false); // get stores of loop1
  vector<Instruction*> loadInsts2 = getMemInst(l2, true);  // get loads of loop2  

  // iterate over the first loop memory instructions
  for ( auto I1: storeInsts1 ) {
    Value *getPtrInstr1 = dyn_cast<StoreInst>(I1)->getPointerOperand();
    Value *getBasePtr1 = dyn_cast<GetElementPtrInst>(getPtrInstr1)->getPointerOperand();
    
    D1("\tPointer operand 1: " << *getBasePtr1 );

    for ( auto I2: loadInsts2 ) {
      Value *getPtrInstr2 = dyn_cast<LoadInst>(I2)->getPointerOperand();
      Value *getBasePtr2 = dyn_cast<GetElementPtrInst>(getPtrInstr2)->getPointerOperand();
      D1("\tPointer operand 2: " << *getBasePtr2 );

      if ( getBasePtr1 != getBasePtr2 ){
        D2( "\tLoad and store working on different arrays " )
        continue;
      }

      const SCEV *SCEVl1 = SE.getSCEVAtScope(getPtrInstr1, &l1);
      const SCEV *SCEVl2 = SE.getSCEVAtScope(getPtrInstr2, &l2);
      
      // if (SCEVl1 && SCEVl2) {
      //   D2( "\tSCEV 1: " << *SCEVl1 );
      //   D2( "\tSCEV 2: " << *SCEVl2 );
      // } else {
      //   D2("\tINVALID SCEV: aborting")
      //   return true;
      // }

      // const SCEV *SCEVNoPtr1 = SE.removePointerBase(SCEVl1);
      // const SCEV *SCEVNoPtr2 = SE.removePointerBase(SCEVl2);

      // Instruction *ptrOffsetInst1 = dyn_cast<Instruction>(dyn_cast<Instruction>(dyn_cast<Instruction>(getPtrInstr1)->getOperand(1))->getOperand(0));
      // Instruction *ptrOffsetInst2 = dyn_cast<Instruction>(dyn_cast<Instruction>(dyn_cast<Instruction>(getPtrInstr2)->getOperand(1))->getOperand(0));

      // const SCEVAddRecExpr *Trip0 = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(ptrOffsetInst1));
      // const SCEVAddRecExpr *Trip1 = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(ptrOffsetInst2));
      // if (Trip0 && Trip1) {
      //   const SCEV *Start0 = Trip0->getStart();
      //   const SCEV *Start1 = Trip1->getStart();
      //   D2("SCEV 1: " << *Trip0)
      //   D2("SCEV 2: " << *Trip1)

      //   D2("SCEV 1 start: " << *Start0)
      //   D2("SCEV 2 start: " << *Start0)
      // }

      // const SCEV *offsetSCEV1 = SE.getSCEVAtScope(ptrOffsetInst1, &l1);
      // const SCEV *offsetSCEV2 = SE.getSCEVAtScope(ptrOffsetInst2, &l2);

      // const SCEV *SCEVNoPtr1 = getSCEVwithNoPtr(getPtrInstr1, l1, SE);
      // const SCEV *SCEVNoPtr2 = getSCEVwithNoPtr(getPtrInstr2, l2, SE);




      // D2(SE.containsAddRecurrence(SCEVl1))
      // D2(SE.containsAddRecurrence(SCEVl2))

      // D2(SE.containsAddRecurrence(SCEVNoPtr1))
      // D2(SE.containsAddRecurrence(SCEVNoPtr2))


      // for (auto scev: SCEVNoPtr1->operands()) {
      //   D2("PTR1 op: " << *scev << " is constant? " << isa<SCEVConstant>(scev))
      // }
      // for (auto scev: SCEVNoPtr2->operands()) {
      //   D2("PTR2 op: " << *scev << " is constant? " << isa<SCEVConstant>(scev))
      // }
      

      // if (auto *C = dyn_cast<SCEVConstant>(SCEVNoPtr1)) {
      //   const APInt &V = C->getAPInt();
      //   D2(V)
      // } else {D2('A')}

      // if (auto *C = dyn_cast<SCEVConstant>(SCEVNoPtr2)) {
      //   const APInt &V = C->getAPInt();
      //   D2(V)
      // } else {D2('A')}
      
      // const SCEVAddRecExpr *SCEVl1 = dyn_cast<SCEVAddRecExpr>(SE.getSCEVAtScope(getBasePtr1, &l1));
      // const SCEVAddRecExpr *SCEVl2 = dyn_cast<SCEVAddRecExpr>(SE.getSCEVAtScope(getBasePtr2, &l2));

      const SCEV *minusSCEV = SE.getMinusSCEV(SCEVl1, SCEVl2);
      D2( "\tMinus SCEV: " << *minusSCEV );

      D2("È negativa? " << SE.isKnownNegative(minusSCEV))

      const SCEV *temp = minusSCEV;
      const SCEVConstant *ConstDiff = dyn_cast<SCEVConstant>(temp);
      D2( " \tConst Diff: " << ConstDiff )

      while(!ConstDiff){
        temp = temp->operands()[0]; 
        ConstDiff = dyn_cast<SCEVConstant>(temp);
      }
      
      int offset = ConstDiff->getValue()->getSExtValue();
      outs() << "   Offset: " << offset << "\n";


    }
  }

  return false;
}

PHINode *getPHIFromHeader(Loop &L) {
  for (auto &I: *L.getHeader()) {
    if (isa<PHINode>(I)) {
      return dyn_cast<PHINode>(&I);
    }
  }

  return nullptr;
}

/*
* The function fuse two loops
*/
bool fuseLoops(Loop &l1, Loop &l2, ScalarEvolution &SE) {

  // BasicBlock *h1 = l1.getHeader();
  // BasicBlock *h2 = l2.getHeader();

  // D3("Loop1 header: " << *h1)
  // D3("Loop2 header: " << *h2)

  PHINode *phi1 = getPHIFromHeader(l1);
  PHINode *phi2 = getPHIFromHeader(l2);

  if (!phi1 || !phi2) {
    D2("Couldn't find one of the induction variable PHI nodes - aborting fusion")
    return false;
  } else {
    D2("PHI 1: " << *phi1)
    D2("PHI 2: " << *phi2)
  }

  phi2->replaceAllUsesWith(phi1);

  // Move body2 before body1

  // Link first loop's exiting block (and eventually guard) 
  // to second loop's exit block

  // Unlink second loop's other blocks from cfg



  return true;
}


void fuseLevelNLoops(vector<Loop*> currentLevelLoops, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE) {

  // Barrier check for empty vector
  if (currentLevelLoops.empty()) {
    D1("Fusion function received empty vector - exiting")
    return;
  }

  vector<Loop*> loopsAfterFusion;

  // Iterate over current level loops (in current loop nest)
  auto loopIt = currentLevelLoops.begin();

  // Skip checks if only one element in vector
  while (loopIt != currentLevelLoops.end()-1) {
    D1("=== ENTERING LOOP PAIR ANALYSIS ITERATION ===")
    // Get first two adjacent loops in vector
    Loop *loop1 = *loopIt;
    Loop *loop2 = *(loopIt + 1);
    
    #ifdef DEBUG
    D1("Loop1 header: "); loop1->getHeader()->printAsOperand(errs(), false); errs() << '\n';
    D1("Loop2 header: "); loop2->getHeader()->printAsOperand(errs(), false); errs() << '\n';
    #endif
    
    // Checks for loop fusion
    if (areAdjacentLoops(*loop1, *loop2) && areControlFlowEq(*loop1, *loop2, DT, PDT) && iterateEqualTimes(*loop1, *loop2, SE) && !haveNegativeDistance(*loop1,*loop2,SE)) {
      D1("ALL CHECKS GOOD: PROCEED WITH LOOP FUSION, REMOVE LOOP2 FROM ARRAY, BREAK AND REPEAT")
      // fuse the loops
      fuseLoops(*loop1, *loop2, SE);
      currentLevelLoops.erase( loopIt+1 );
    } else {
      D1("LOOPS CANNOT BE FUSED, REMOVE LOOP1 FROM ARRAY AND CONTINUE ITERATING")
      currentLevelLoops.erase( loopIt );
    }
    // In both cases add first loop to fusion results vector, if not already present
    if (find(loopsAfterFusion.begin(), loopsAfterFusion.end(), loop1) == loopsAfterFusion.end()) {
      loopsAfterFusion.push_back(loop1);
    }
    D1("=== END OF ANALYSIS ITERATION ===")
  }
  // Last element in vector (also if parameter already came with a single loop inside)
  D1("=== ONE LOOP LEFT - STOP ANALYZING ===")
  if (find(loopsAfterFusion.begin(), loopsAfterFusion.end(), *loopIt) == loopsAfterFusion.end()) {
    loopsAfterFusion.push_back(*loopIt);
  }
  
  // Recursive calls on each next loop nest
  for( auto L: loopsAfterFusion) {
    vector<Loop*> subLoops = L->getSubLoopsVector();
    #ifdef DEBUG
    D2("RECURSIVE CALL ON SUBLOOPS FROM:")
    L->getHeader()->printAsOperand(errs(),false);
    errs() << '\n';
    #endif
    fuseLevelNLoops(subLoops, DT, PDT, SE);
  }
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
    // Dominators tree
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    // Post-dominator tree
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    // Scalar Evolution Analysis
    ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

    // Small vector of loops
    vector<Loop*> functionLoops = LI.getTopLevelLoops();
    // Get proper loop ordering
    reverse(functionLoops.begin(), functionLoops.end());

    #ifdef DEBUG
    D1("Print Top-level loops")
    for ( auto L: functionLoops) {
      D1( *L)
    }
    #endif

    fuseLevelNLoops(functionLoops, DT, PDT, SE);

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
