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
#include "llvm/IR/CFG.h"
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
* Function that checks if the guard blocks of the guarded loops have identical conditions
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
* Function that checks if loops are both guarded or not and if there are statements between the loops
*/
bool areAdjacentLoops(Loop &l1, Loop &l2) {
  D2("--- START ADJACENCY CHECK ---")
  // Try to retrieve both guard branch instructions
  BranchInst *guardBranch1 = l1.getLoopGuardBranch();
  BranchInst *guardBranch2 = l2.getLoopGuardBranch();

  // Logic to retrieve correct "adjacent BB candidates" depending if loops are both guarded or not
  if (guardBranch1 && guardBranch2 && checkGuardCondition(guardBranch1, guardBranch2)) {
    D2( "\tLoops are both guarded and with identical conditions" )
    // iterate on the two successor BBs and verify if one exit of the first loop is the guard BB of the second loop
    for (auto S: guardBranch1->successors()) {
      if ( (guardBranch2->getParent()) == S ) {
        D3("\tFirst loop's guard exit is the second loop's guard BB " << *S)
    
        // check if first BB instruction is the branch condition
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
    
    // the first loop must have a single exit block
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
    // loops are not both guarded or both not guarded (acts as a fallback exit condition)
    D2("\tNot both guarded or not guarded - EXIT CHECK WITH FALSE")
    return false;
  }
  
  // all checks passed
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
    if ( DT.dominates(getGuardBlock(l1), getGuardBlock(l2)) && PDT.dominates(getGuardBlock(l2), getGuardBlock(l1)) ) {
      D2( "\tGuarded loops are control flow equivalent - EXIT CHECK WITH TRUE" )
      return true;
    }
  } else if ( !l1.isGuarded() && !l2.isGuarded() ) { 
    if ( DT.dominates(l1.getLoopPreheader(), l2.getLoopPreheader()) && PDT.dominates(l2.getLoopPreheader(), l1.getLoopPreheader()) ) {
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
* if isLoad is true, return load instructions, otherwise return store instructions
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
* Get a APInt from a MinusSCEV Expression (recursive version)
* Return an APInt that is the evaluation of MinusSCEV constants
* If the result is < 0, then loops have negative distance dependencies
*/ 
APInt evaluateMinusSCEV(const SCEV *scev) {
  // Get current SCEV type bitwidth, to be used for APInt instances
  unsigned bitwidth = scev->getType()->getIntegerBitWidth();
  
  // BASE CASE: SCEV is constant => return the value
  if (const auto *C = dyn_cast<SCEVConstant>(scev)) {
    // First retrieve the SCEV ConstantInt, and then the corresponding APInt value
    const APInt &Val = C->getValue()->getValue();
    D1("\t\tConstant found: " << Val);
    return Val;
  }
  
  // SCEVAddRecExpr represents a recursive formula tied to a loop: {start,+,step}
  // if a SCEVAddRecExpr if found => {start,+,step} case => evaluate this with recursive call
  if (const auto *AddRec = dyn_cast<SCEVAddRecExpr>(scev)) {
    D2("\t\tAddRec found: " << *scev << ", evaluating the start operand");
    // Per una ricorrenza, ritorna il valore iniziale
    return evaluateMinusSCEV(AddRec->getStart());
  }
  
  // The following conditions check the different types of operations we need to handle
  
  // check for an Add in the SCEV
  if (const auto *AddExpr = dyn_cast<SCEVAddExpr>(scev)) {
    D2("\t\tAddExpr: " << *scev);
    APInt sum(bitwidth, 0);
    for (const SCEV *Op : AddExpr->operands()) {
      APInt opVal = evaluateMinusSCEV(Op);
      sum += opVal;
    }
    return sum;
  }
  
  // check a Mul in the SCEV
  if (const auto *MulExpr = dyn_cast<SCEVMulExpr>(scev)) {
    D2("\t\tMulExpr: " << *scev);
    APInt prod(bitwidth, 1);
    for (const SCEV *Op : MulExpr->operands()) {
      APInt opVal = evaluateMinusSCEV(Op);
      prod *= opVal;
    }
    return prod;
  }
  
  // checks for Sext expression
  if (const auto *Sext = dyn_cast<SCEVSignExtendExpr>(scev)) {
    D2("\t\tSignExtend: " << *scev);
    APInt opVal = evaluateMinusSCEV(Sext->getOperand());
    return opVal.sext(bitwidth);
  }
  
  D2(" \t\tUnable to handle SCEV type - RETURN 0");
  return APInt(bitwidth, 0);
}

/*
* Function that checks if there are negative distance dependencies between two loops
*/
bool haveNegativeDistanceDiff(vector<Instruction*> storeInsts, vector<Instruction*> loadInsts, Loop &storeLoop, Loop &loadLoop, ScalarEvolution &SE, bool invert) {
  for (auto *I1 : storeInsts) {
    auto *store = dyn_cast<StoreInst>(I1);
    
    Value *storePtr = store->getPointerOperand();
    auto *gepStore = dyn_cast<GetElementPtrInst>(storePtr);
    
    Value *storeBasePtr = gepStore->getPointerOperand();
    D1("\tPointer operand 1: " << *storeBasePtr);
    
    for (auto *I2 : loadInsts) {
      auto *load = dyn_cast<LoadInst>(I2);
      
      Value *loadPtr = load->getPointerOperand();
      auto *gepLoad = dyn_cast<GetElementPtrInst>(loadPtr);
      
      Value *loadBasePtr = gepLoad->getPointerOperand();
      D1("\tPointer operand 2: " << *loadBasePtr);
      
      // not working on the same array
      if (storeBasePtr != loadBasePtr) {
        D2("\tLoad and store working on different arrays");
        continue;
      }
      
      // Scalar evolution on pointer load and store instructions 
      const SCEV *storeScev = SE.getSCEVAtScope(storePtr, &storeLoop);
      const SCEV *loadScev = SE.getSCEVAtScope(loadPtr, &loadLoop);

      // When working on the second possible negdep situation, operands in the MinusSCEV need to be inverted
      // (we are checking stores from l2 and loads from l1, so the distances are inverted)
      const SCEV *diff = !invert ? SE.getMinusSCEV(storeScev, loadScev) : SE.getMinusSCEV(loadScev, storeScev);

      D2("\tMinus SCEV: " << *diff);
      
      // Check negative distance (works for constants)
      if (SE.isKnownNegative(diff)) {
        D2("\tKnown negative distance found");
        return true;
      }
      
      // Evaluate the MinusSCEV to get the distance
      APInt minusConst = evaluateMinusSCEV(diff);
      D2("\tValue of the MinusSCEV: " << minusConst);
      
      // If the distance is negative, then there is a negative distance dependency
      if ( minusConst.isNegative() ) {
        D2 ("\tThe loops have negative distance dependencies")
        return true;
      }
      
    }
  }
  D2("\tVectors are empty or there are no dependencies between passed vectors")
  return false;
}

/*
* Function that checks if the loops have negative distance dependencies
*/
bool haveNoNegativeDistance(Loop &l1, Loop &l2, ScalarEvolution &SE) {
  // Retrieve vectors
  vector<Instruction*> storeInsts1 = getMemInst(l1, false); // get stores of loop1
  vector<Instruction*> storeInsts2 = getMemInst(l2, false); // get stores of loop2
  vector<Instruction*> loadInsts1 = getMemInst(l1, true);   // get loads of loop1  
  vector<Instruction*> loadInsts2 = getMemInst(l2, true);   // get loads of loop2  

  // Checks if the loops have negative distance dependencies
  if (haveNegativeDistanceDiff(storeInsts1, loadInsts2, l1, l2, SE, false) || haveNegativeDistanceDiff(storeInsts2, loadInsts1, l2, l1, SE, true)) {
    D2("\tLoops have negative distance dependencies - EXIT WITH FALSE")
    return false;
  }

  D2("\tLoops have no negative distance dependencies - EXIT WITH TRUE")
  return true;
}

/*
* Get the PHI node from the header basic block
*/
PHINode *getPHIFromHeader(Loop &L) {
  for (auto &I: *L.getHeader()) {
    if (isa<PHINode>(I)) {
      return dyn_cast<PHINode>(&I);
    }
  }

  return nullptr;
}


vector<BasicBlock*> getLatchPreds( BasicBlock *latch ) {
  vector<BasicBlock*> preds;
  for (auto it = pred_begin(latch), et = pred_end(latch); it != et; ++it) {
    preds.push_back(*it);
  }
  return preds;
}


/*
* Function that fuse two loops
*/
bool fuseLoops(Loop &l1, Loop &l2, ScalarEvolution &SE) {
  // Retrieve phi nodes from loop headers
  PHINode *phi1 = getPHIFromHeader(l1);
  PHINode *phi2 = getPHIFromHeader(l2);

  // Check if both loops have a PHI node in the header
  if (!phi1 || !phi2) {
    D2("Couldn't find one of the induction variable PHI nodes - aborting fusion")
    return false;
  } else {
    D2("PHI 1: " << *phi1)
    D2("PHI 2: " << *phi2)
  }

  // Get BBs needed to link the exiting edge from first loop to the BB after the second loop
  BasicBlock *exitingBlock1 = l1.getExitingBlock();
  BasicBlock *exitBlock2 = l2.getExitBlock();

  // Get the branch from exiting block, to then change its destination
  BranchInst *exitingBranch1 = dyn_cast<BranchInst>(--(exitingBlock1->end()));

  // May be redundant
  if (!exitingBranch1) {
    D2("No exiting branch found - could not fuse loops")
    return false;
  }

  if (!l1.contains(exitingBranch1->getSuccessor(0))) {
    exitingBranch1->setSuccessor(0, exitBlock2);
  } else {
    exitingBranch1->setSuccessor(1, exitBlock2);
  }

  // Get both latches
  BasicBlock *latch1 = l1.getLoopLatch();
  BasicBlock *latch2 = l2.getLoopLatch();

  // stiamo cercando di collegare direttamente il secondo latch al primo latch. in questo modo ci evitiamo lo smeno di spostare istruzioni che potrebbero essere non legate alla induction variable ma che comunque si trovano nel latch, in un altro blocco precedente.
  // dunque, bisogna implementare un controllo che verifica che, oltre al salto ed all'incremento della vecchia induction variable, non ci siano altre istruzioni
  // o meglio: possiamo direttamente rimuovere tutte le istruzioni relative all'induction variable vecchia (solo l'incremento?)

  // inoltre, bisogna anche spostare le istruzioni non legate alla induction v. del loop1 che si trovano nel latch 1: altrimenti eseguiranno dopo il body del loop2 inserito in mezzo
  // guarda metodi per splittare i basic block eventualmente


  // replace the uses of the second induction var.
  phi2->replaceAllUsesWith(phi1);
  // erase the PHI instruction from the second loop header
  phi2->eraseFromParent();

  // link the last body blocks of loop1 with the header of the first loop
  vector<BasicBlock*> predsLatch1 = getLatchPreds(latch1);  // loop1 latch predecessors
  
  BasicBlock* L2LinkBB = l2.getHeader();

  // if the header of the second loop is the exiting block, then is not rotated and we can
  // link the only one internal successor as link block for loop 1 latch predecessors
  if ( l2.isLoopExiting(L2LinkBB) ) {

    BranchInst *HeaderBranch = dyn_cast<BranchInst>(--(L2LinkBB->end()));

    // get the correct BB to link with
    L2LinkBB = l2.contains(HeaderBranch->getSuccessor(0)) ? HeaderBranch->getSuccessor(0) : HeaderBranch->getSuccessor(1);

    // erase the loop header if the loop is not rotated (not used anymore)
    l2.getHeader()->eraseFromParent();

  }
  
  // link first loop latch precedessors with single successor of second loop header
  for (auto it = predsLatch1.begin(); it != predsLatch1.end(); ++it) {
    BasicBlock* Pred = *it;
    // BranchInst *branch = dyn_cast<BranchInst>(Pred->getTerminator());
    BranchInst *branch = dyn_cast<BranchInst>(--(Pred->end()));
    // May be redundant
    if (!branch) {
      D2("No exiting branch found - could not fuse loops")
      return false;
    }
    branch->setSuccessor(0, L2LinkBB);
  }

  // iterate on latch2 predecessors to change the branch to latch1
  vector<BasicBlock*> predsLatch2 = getLatchPreds(latch2);
  // iterate over the preds vector and set the successor to first loop latch
  for (auto it = predsLatch2.begin(); it != predsLatch2.end(); ++it) {
    BasicBlock* Pred = *it;
    // BranchInst *branch = dyn_cast<BranchInst>(Pred->getTerminator());
    BranchInst *branch = dyn_cast<BranchInst>(--(Pred->end()));
    // May be redundant
    if (!branch) {
      D2("No exiting branch found - could not fuse loops")
      return false;
    }
    branch->setSuccessor(0,latch1);
  }

  // Beautify IR code
  latch1->moveBefore(latch2);

  // erase the second loop preheader (already without predecessors)
  l2.getLoopPreheader()->eraseFromParent();

  // TODO - check instructions

  
  // erase latch of the second loop (not used anymore)
  latch2->eraseFromParent();

  return true;
}

/*
* Function that fuses all loops in a given level of the loop nest
*/
void fuseLevelNLoops(vector<Loop*> currentLevelLoops, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE, LoopInfo &LI, Function &F, FunctionAnalysisManager &AM) {
  // Barrier check for empty vector
  if (currentLevelLoops.empty()) {
    D1("Fusion function received empty vector - exiting")
    return;
  }

  // Vector to store loops after fusion
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
    if (areAdjacentLoops(*loop1, *loop2) && areControlFlowEq(*loop1, *loop2, DT, PDT) && iterateEqualTimes(*loop1, *loop2, SE) && haveNoNegativeDistance(*loop1,*loop2,SE)) {
      D1("ALL CHECKS GOOD: PROCEED WITH LOOP FUSION, REMOVE LOOP2 FROM ARRAY, BREAK AND REPEAT")
      D1("=== LOOP FUSION ===")
      // fuse the loops
      if (fuseLoops(*loop1, *loop2, SE)) {
        // 1. Ricalcola DominatorTree
DT.recalculate(F);

// 2. Ricalcola PostDominatorTree
PDT.recalculate(F);

// 3. Ricalcola LoopInfo (richiede DT aggiornato)
LI.releaseMemory();
LI.analyze(DT);

      }
      // Remove second loop from vector because it has been fused with the first one
      currentLevelLoops.erase( loopIt+1 );
    } else {
      D1("LOOPS CANNOT BE FUSED, REMOVE LOOP1 FROM ARRAY AND CONTINUE ITERATING")
      // Remove first loop from vector because it cannot be fused with the second one
      currentLevelLoops.erase( loopIt );
    }
    // In both cases add first loop to fusion results vector, if not already present because it is either fused with the second one or it is the last loop in the vector
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
    // vector to store subloops
    vector<Loop*> subLoops = L->getSubLoopsVector();
    #ifdef DEBUG
    D2("RECURSIVE CALL ON SUBLOOPS FROM:")
    L->getHeader()->printAsOperand(errs(),false);
    errs() << '\n';
    #endif
    // If there are subloops, call the function recursively
    fuseLevelNLoops(subLoops, DT, PDT, SE, LI, F, AM);
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

    // call to the function that fuses loops
    fuseLevelNLoops(functionLoops, DT, PDT, SE, LI, F, AM);

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
