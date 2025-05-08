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
* LOOP INVARIANCE FUNCTIONS
*/

bool isLoopInvInstr(Instruction &I, vector<Instruction*> &loopInvInstr, Loop &L);

/*
* Function that checks wether an operand from a BinaryOp is considered loop-invariant
*/
bool isLoopInvOp(Value *OP, vector<Instruction*> &loopInvInstr, Loop &L) {

  if ( isa<ConstantInt>(OP) ) {
    D2("\t\tOperand is constant -> labeling as linv");
    return true;
  }else if ( isa<Argument>(OP) ){
    D2("\t\tOperand is a function argument -> labeling as linv")
    return true;
  }

  // Cast from Value to Instruction to perform checks
  if ( dyn_cast<Instruction>(OP) ) {
    Instruction *OpInst = dyn_cast<Instruction>(OP);

    if ( find(loopInvInstr.begin(), loopInvInstr.end(), OpInst) != loopInvInstr.end() ) {
      D2("\t\tOperand already labeled as loop-invariant");
      return true;
    } else if (!L.contains(OpInst)) {
      D2("\t\tOperand defined outside the loop -> labeling as linv");
      return true;
    } else if (!OpInst->isBinaryOp()) { // If inside loop, not already linv, not constant, and not BinaryOp, then is not linv
      D2("\t\tOperand defined inside the loop and not a BinaryOp -> not linv");
      return false;
    } else if (isLoopInvInstr(*OpInst, loopInvInstr, L)) { // Cannot determine wether the operand is linv or not: recursive call to isLoopInvInstr(OPERATOR)
      return true;
    }
  }

  D2("OPERAND DID NOT PASS ANY TEST");
  return false;
}

/*
* Function that checks wether a BinaryOp from a loop is considered loop-invariant
*/
bool isLoopInvInstr(Instruction &I, vector<Instruction*> &loopInvInstr, Loop &L) {
  // Retrieve operands
  Value *op1 = I.getOperand(0);
  Value *op2 = I.getOperand(1);

  D2("\tBINARY OP:");
  D2("\tOperand 1: " << *op1 );
  D2("\tOperand 2: " << *op2 );

  // Check if both are loop-invariant; if so, the instruction itself can be considered loop-invariant
  if (isLoopInvOp(op1, loopInvInstr, L) && isLoopInvOp(op2, loopInvInstr, L)) {
    D2("\tBoth operands are linv -> label as linv")
    return true;
  }

  return false;
}

/*
* Function to retrieve loop-invariant instructions from a specific loop
*/
void getLoopInvInstructions(vector<Instruction*> &loopInvInstr, Loop &L) {
  // TODO implement logic to iterate over nested loops

  // Iterate over loop instructions (via its BBs first)
  for (Loop::block_iterator BI = L.block_begin(); BI != L.block_end(); ++BI) {
    BasicBlock *B = *BI;
    // Iterate over BB instructions
    for (auto &I: *B) {
      D2(I);
      // Check for loop invariance applies only to BinaryOp instructions
      if (I.isBinaryOp() && isLoopInvInstr(I, loopInvInstr, L)) {
        D2("\tIS LOOP INVARIANT OP");
        loopInvInstr.push_back(&I);
      }
    }
  }
  return;
}

/*
* CODE MOTION CANDIDATE SEARCH FUNCTIONS
*/

/*
* function that checks if I has any use in PHI nodes internal to the loop,
* which means there are multiple definitions of the same variable
*/
bool hasMultipleDef(Instruction *I, Loop &L){
  D2("\tChecking if definition has multiple definitions")

  // For all the uses of the instruction
  for (auto useIt = I->use_begin(); useIt != I->use_end(); ++useIt) {
    User *use = useIt->getUser();
    D3("\tChecking " << *I << "'s use " << *use)

    // Do additional checks only if the use is a PHINode
    if (isa<PHINode>(use)) {
      PHINode *usePHI = dyn_cast<PHINode>(use);
    
      // Check if PHI is inside the loop
      if ( L.contains(usePHI) ){
        D2( "\tFound a PHI node inside the loop, checking its arguments... (" << *usePHI << " )" )

        // Iterate over PHI incoming BBs, and check if there's at least another one from inside the loop apart from I's
        for (int i = 0; i < usePHI->getNumIncomingValues(); i++) {
          BasicBlock *incomingBB = usePHI->getIncomingBlock(i);
        
          D2("\tCurrent incoming BB: " << *incomingBB)
          if ( L.contains(incomingBB) && I != dyn_cast<Instruction>(usePHI->getIncomingValue(i))) {
            D2("\tThe variable has another definition from inside the loop!")
            return true;
          }

          #ifdef DEBUG
          else {
            D2("\tThe incoming use is the same, or defined outside the loop")
          }
          #endif
          
        }
      }
    } else {
      D3("\tSkipping use because not a PHI node")
    }
  }
  D2("\t\tOK")
  return false;
}

// /*
// * function checks if variable is dead outside the loop 
// */
bool isDeadOutsideLoop(Instruction *I, Loop &L) {
  D2("\t\tChecking if definition is alive outside loop")

  BasicBlock *useBB;
  
  // For all the uses of the instruction
  for (auto useIt = I->use_begin(); useIt != I->use_end(); ++useIt) {
    User *use = useIt->getUser();
    
    // check if the use is outside the loop
    if (isa<Instruction>(use)) {
      Instruction *useInst = dyn_cast<Instruction>(use);
      if ( !L.contains(useInst) ){
        D2("\tThe use " << *useInst << " of " << *I << " is outside the loop -> NOT DEAD");
        return false;
      }
      // if the use is a PHINode from inside, check for its uses outside the loop as well
      else if (isa<PHINode>(useInst)) {
        return isDeadOutsideLoop(useInst,L);
      }
    }
  }
  D2( "\t\tOK" )
  return true;
}


/*
* function to check wether a code motion candidate instruction dominates all exit BBs or, if it doesn't, if it's dead outside the loop
*/
bool domsAllLivePaths(Instruction *I, Loop &L, DominatorTree &DT, SmallVector<BasicBlock*> &exitBBs) {
  D2("\tChecking if definition dominates all paths where is not dead")
  // get the BB of the loop invariant instruction
  BasicBlock *BBInst = I->getParent();

  // check if the BB dominates all the loop exits
  for (auto &exitBlock : exitBBs) {
    if ( !DT.dominates(BBInst, exitBlock) ) {
      D2("The instruction does not dominate all loop exits, specifically:\n\t" << *exitBlock << "\n")
      
      // Additional check on variable liveness outside the loop
      return isDeadOutsideLoop(I,L);
    }
  }

  D2("\t\tOK")
  return true;
}

void findCodeMotionCandidates(vector<Instruction*> &loopInvInstr, DominatorTree &DT, Loop &L){

/*
* function that finds the code motion candidates
*/
  SmallVector<BasicBlock*> exitBBs; 
  L.getExitBlocks(exitBBs);
  
  if ( exitBBs.size() == 0 ){
    D1(" Loop has no exit blocks -> deleting all instructions from loopInvInstr")
    loopInvInstr.clear();
    return;
  }

  #ifdef DEBUG
  D2("------\nLoop EXIT BLOCKS:\n------");
  for (auto E: exitBBs)
    D2(*E);
  D2("------")
  #endif

  for (auto it = loopInvInstr.begin(); it != loopInvInstr.end();) {
    // get instruction from the iterator
    Instruction *I = *it;
    D2("Checking if " << *I << " is a candidate")
    
    if( hasMultipleDef(I, L) ) {
      D1("Erasing " << *I << " from loopInvInstr because has multiple definitions inside the loop ");
      loopInvInstr.erase(it);
    } else if ( !domsAllLivePaths(I, L, DT, exitBBs) ) { 
      D1("Erasing " << *I << " from loopInvInstr because it doesn't dominate all loop exit blocks where is alive ");
      loopInvInstr.erase(it);
    } else { // increase the iterator only if element not deleted
      D2("\t" << *I << " is a valid candidate for code motion")
      ++it;
    }
  }
}


/*
* CODE MOTION FUNCTIONS
*/

/*
* function that checks if the instruction is movable in the preheader
*/
bool isMovable(Value *op, vector<Instruction*> &loopInvInstr, Loop &L) {
  if( isa<ConstantInt>(op) || isa<Argument>(op) || !L.contains( dyn_cast<Instruction>(op) ) ) {
    D2("Operand " << *op << " is a constant or previously moved candidate")
    return true;
  }

  D2("NOT MOVABLE: Operand " << *op << " is another candidate and has to be moved first!")
  return false;

}

/*
* function that moves the instruction to the preheader block
*/
bool Move(Instruction *I, vector<Instruction*> &loopInvInstr, BasicBlock *phBB, Loop &L) {
  
  auto itInst = find(loopInvInstr.begin(), loopInvInstr.end(), I);
  
  if ( itInst == loopInvInstr.end() ){
    D3( " Tying to move an instruction that is not a candidate ")
    return false;
  }
  
  D2(" Move call for instruction " << *I << " to preheader block " << *phBB);
  Value *op1 = I->getOperand(0);
  Value *op2 = I->getOperand(1);

  if (!isMovable(op1, loopInvInstr, L) || Move(dyn_cast<Instruction>(op1), loopInvInstr, phBB, L)) {
    loopInvInstr.erase( itInst ); // remove from the list of loop invariant instructions
    return false;
  }
  
  if(!isMovable(op2, loopInvInstr, L) || Move(dyn_cast<Instruction>(op2), loopInvInstr, phBB, L)) {
    loopInvInstr.erase( itInst ); // remove from the list of loop invariant instructions
    return false;
  }

  // Move the instruction to the preheader block
  I->moveBefore(phBB->getTerminator());
  D1("Moved instruction " << *I << " to preheader block " << *phBB);
  loopInvInstr.erase( itInst );
  return true;

}

/*
* function that performs the code motion
*/
void codeMotion(vector<Instruction*> &loopInvInstr, Loop &L) {
  D1("======\nCode Motion:\n======");

  if (!L.getLoopPreheader()) { // even though we work on loops in normal form, we should keep this test if all previous ones fail
    D1("No preheader for the loop -> cannot perform code motion!")
    return;
  } else {
    BasicBlock* phBB = L.getLoopPreheader();
    D2("Preheader found: " << *phBB); 
    Instruction* lastMoved = phBB->getTerminator(); 
    while(!loopInvInstr.empty()){
      Instruction *I = loopInvInstr.front(); // get the last instruction in the vector
      Move(I, loopInvInstr, phBB, L); // move the instruction to the preheader block
    }
  }
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
    D3("======\nDominance tree in deep-first:\n======");
      for (auto *DTN : depth_first(DT.getRootNode())) {
        D3(DTN);
      }
    #endif

    // instruction vectors to be reused for each loop
    vector<Instruction*> loopInvInstr;

    // iterate on all TOP-LEVEL loops from function
    for ( auto &L: LI ) {
      SmallVector<Loop*> nestVect = L->getLoopsInPreorder();
      for ( auto &NL: nestVect){
        D1("############################\nCURRENTLY WORKING ON THE LEVEL " << NL->getLoopDepth() << " LOOP WITH HEADER BLOCK " << *NL->getHeader() << "\n############################");

        #ifdef DEBUG
        D2("======\nLoop blocks:\n======");
        for (Loop::block_iterator BI = NL->block_begin(); BI != NL->block_end(); ++BI) {
          BasicBlock *B = *BI;
          D2(*B);
        }
        D2("======")
        #endif
        // retrieve loop invariant instructions for current loop
        getLoopInvInstructions(loopInvInstr, *NL);

        if (loopInvInstr.empty()) { // if there's no linv instructions
          D1("\n******** No loop invariant instructions found; continue with next loop... ********\n")
          continue;
        }

        #ifdef DEBUG
        D1("======\nLoop-invariant instructions:\n======");
        for (auto I: loopInvInstr)
          D1(*I);
        #endif

        D1("\n======\nPerforming candidate checks...\n")

        // code motion candidates
        findCodeMotionCandidates(loopInvInstr, DT, *NL);

        if (loopInvInstr.empty()) { // if there's no instructions suitable for the code motion
          D1("\n******** No candidate instructions for code motion found; continue with next loop... ********\n")
          continue;
        }

        #ifdef DEBUG
        D1("======\nCode motion candidates:\n======");
        for (auto I: loopInvInstr)
          D1(*I);
        #endif

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
