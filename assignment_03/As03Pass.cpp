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
#include <vector>

using namespace llvm;
using namespace std;

#define DEBUG

#ifdef DEBUG
#define D(x) llvm::outs() << x << '\n';
#else
#define D(x)
#endif

/*
* LOOP INVARIANCE FUNCTIONS
*/

bool isLoopInvInstr(Instruction &I, vector<Instruction*> &linvInstr, Loop &L);

/*
* Function that checks wether an operand from a BinaryOp is considered loop-invariant
*/
bool isLoopInvOp(Value *OP, vector<Instruction*> &linvInstr, Loop &L) {
  D("\t\tEntered isLoopInvOp for " << *OP);


  if ( isa<ConstantInt>(OP) ) {
    D("\t\tOperand is constant -> labeling as linv");
    return true;
  }else if ( isa<Argument>(OP) ){
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

/*
* Function that checks wether a BinaryOp from a loop is considered loop-invariant
*/
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

/*
* Function to retrieve loop-invariant instructions from a specific loop
*/
void getLoopInvInstructions(vector<Instruction*> &linvInstr, Loop &L) {
  // TODO implement logic to iterate over nested loops

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

/*
* CODE MOTION CANDIDATE SEARCH FUNCTIONS
*/

/*
 * function that gets the basic blocks for the uses of I and check if I dominates then all 
 */
bool domAllUses( Instruction *I, DominatorTree &DT, Loop &L){
  // get the BB of the instruction
  BasicBlock *defBlock = I->getParent();

  // For all the uses of the instruction
  for (auto useIt = I->use_begin(); useIt != I->use_end(); ++useIt) {
    User *use = useIt->getUser();

    if (dyn_cast<Instruction>(use)) {
      Instruction *useInst = dyn_cast<Instruction>(use);
      // get the BB of the use
      BasicBlock *useBB = useInst->getParent();

      // if the BB of the use is not inside the loop and it doesn't dominate the instruction BB, then return false
      if ( L.contains(useBB) && !DT.dominates(defBlock, useBB) ) {
        D( "Return false: " << *I <<  " does not dominate all its uses in the loop \n" );
        D( "PROBLEMATIC USE: " << *useInst )
        return false;
      }
    }
  }
  return true;
}

/*
* function that checks if I has any use in PHI node
*/
bool hasMultipleDef(Instruction *I, Loop &L){
  // For all the uses of the instruction
  for (auto useIt = I->use_begin(); useIt != I->use_end(); ++useIt) {
    User *use = useIt->getUser();

    if (dyn_cast<Instruction>(use)) {
      Instruction *useInst = dyn_cast<Instruction>(use);
    
      if ( L.contains(useInst->getParent()) && isa<PHINode>(useInst) ){
        D( "The use is a PHI node inside the loop!" )
        return true;
      }
    }
  }
  return false;
}

// /*
// * function checks if variable dominates all exits where it is alive (used)
// */
bool isAliveOutsideLoop(Instruction *I, Loop &L, DominatorTree &DT, BasicBlock *exitBlock) {
  
  BasicBlock *useBB;
  
  // For all the uses of the instruction
  for (auto useIt = I->use_begin(); useIt != I->use_end(); ++useIt) {
    User *use = useIt->getUser();
    
    if (dyn_cast<Instruction>(use)) {
      Instruction *useInst = dyn_cast<Instruction>(use);
      useBB = useInst->getParent();
      
      if ( !L.contains(useBB) && DT.dominates(exitBlock, useBB) ){
        D("The use " << *useInst << " of " << *I << " is outside the loop and alive in the current exit block");
        return false;
      }
    }
  }
  return true;
}

/*
* function to check wether a code motion candidate instruction dominates all exit BBs after whose it's still alive
*/
bool domsAllLivePaths(Instruction *I, Loop &L, DominatorTree &DT, SmallVector<BasicBlock*> &exitBBs) {
  // get the BB of the loop invariant instruction
  BasicBlock *BBInst = I->getParent();

  // check if the BB dominates the loop exit AND if the the BB dominates all blocks that use the variable
  for (auto &exitBlock : exitBBs) {
    if ( !DT.dominates(BBInst, exitBlock) && !isAliveOutsideLoop(I,L,DT,exitBlock)) {
      return false;
    }
  }

  return true;
}

void findCodeMotionCandidates(vector<Instruction*> &loopInvInstr, DominatorTree &DT, Loop &L){

/*
* function that finds the code motion candidates
*/
  SmallVector<BasicBlock*> exitBBs; 
  L.getExitBlocks(exitBBs);
  
  if ( exitBBs.size() == 0 ){
    D(" Loop has no exit blocks -> deleting all instructions from loopInvInstr")
    loopInvInstr.clear();
    return;
  }

  #ifdef DEBUG
  D("======\nLoop EXIT BLOCKS:\n======");
  for (auto E: exitBBs)
    D(*E);
  #endif

  for (auto it = loopInvInstr.begin(); it != loopInvInstr.end();) {
    // get instruction from the iterator
    Instruction *I = *it;
    
    if( hasMultipleDef(I, L) ) {
      D("Erasing " << *I << " from loopInvInstr because has multiple definitions inside the loop ");
      loopInvInstr.erase(it);
    } else if (!domAllUses(I, DT, L) ) {
      D("Erasing " << *I << " from loopInvInstr because it doesn't dominate all uses");
      loopInvInstr.erase(it);
    } else if ( !domsAllLivePaths(I, L, DT, exitBBs) ) { 
      D("Erasing " << *I << " from loopInvInstr because it doesn't dominate all loop exit blocks where is alive ");
      loopInvInstr.erase(it);
    } else { // increase the iterator only if element not deleted
      ++it;
    }
  }

  #ifdef DEBUG
  D("Candidates after dominator checks: \n");
  for (auto I: loopInvInstr)
    D(*I);
  #endif
}


/*
* CODE MOTION FUNCTIONS
*/

/*
* function that checks if the instruction is movable in the preheader
*/
bool isMovable(Value *op, vector<Instruction*> &loopInvInstr) {
  if(isa<ConstantInt>(op) || isa<Argument>(op) || find(loopInvInstr.begin(), loopInvInstr.end(), op) == loopInvInstr.end()) {
    D("Instruction is a constant or not loop invariant -> movable")
    return true;
  }
  D("FALLBACK EXIT: Operand " << *op << " has to be moved first");
  return false;
}

/*
* function that moves the instruction to the preheader block
*/
void Move(Instruction *I, vector<Instruction*> &loopInvInstr, BasicBlock *phBB) {
  Value *op1 = I->getOperand(0);
  Value *op2 = I->getOperand(1);

  if (!isMovable(op1, loopInvInstr)) {
    Move(dyn_cast<Instruction>(op1), loopInvInstr, phBB);
  }
  
  if(!isMovable(op2, loopInvInstr)) {
    Move(dyn_cast<Instruction>(op2), loopInvInstr, phBB);
  }

  // Move the instruction to the preheader block
  I->moveBefore(phBB->getTerminator());
  D("Moved instruction " << *I << " to preheader block " << *phBB);
}

/*
* function that performs the code motion
*/
void codeMotion(vector<Instruction*> &loopInvInstr, Loop &L) {
  D("======\nCode Motion:\n======");

  if (!L.getLoopPreheader()) { // even though we work on loops in normal form, we should keep this test if all previous ones fail
    D("No preheader for the loop -> cannot perform code motion")
    return;
  } else {
    BasicBlock* phBB = L.getLoopPreheader();
    D("Preheader found: " << *phBB); 
    Instruction* lastMoved = phBB->getTerminator(); 
    while(!loopInvInstr.empty()){
      Instruction *I = loopInvInstr.front(); // get the last instruction in the vector
      D("Moving instruction " << *I << " to preheader block " << *phBB);
      Move(I, loopInvInstr, phBB); // move the instruction to the preheader block
      loopInvInstr.erase(loopInvInstr.begin()); // remove from the list of loop invariant instructions
    }
  }

  D("Instructions after code motion: \n");
  #ifdef DEBUG
  for (auto BB: L) {
    for (auto I: *BB) {
      D(*I);
    }
  }
  #endif
  
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

    // dominators
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);

    #ifdef DEBUG
    D("======\nDominance tree in deep-first:\n======");
      for (auto *DTN : depth_first(DT.getRootNode())) {
        D(DTN);
      }
    #endif

    // instruction vectors to be reused for each loop
    vector<Instruction*> loopInvInstr;

    // iterate on all TOP-LEVEL loops from function
    for ( auto &L: LI ) {
      SmallVector<Loop*> nestVect = L->getLoopsInPreorder();
      for ( auto &NL: nestVect){
        D(*NL);
        // retrieve loop invariant instructions for current loop
        getLoopInvInstructions(loopInvInstr, *NL);
        #ifdef DEBUG
        D("======\nLoop-invariant instructions:\n======");
        for (auto I: loopInvInstr)
          D(*I);
        #endif


        // code motion candidates
        findCodeMotionCandidates(loopInvInstr, DT, *NL);

        // code motion
        codeMotion(loopInvInstr, *NL);

        loopInvInstr.clear();
      }
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
