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

/*
* InitializeLoopInst function
* Initialize the vector LoopInst with all the instructions in the loop passed as parameter
*/
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


enum linv_t {

  unknown,
  linv,
  not_linv

};


linv_t isLoopInvOp(Value *op, vector<Instruction*> &LoopInst,  vector<Instruction*> &LoopInv_inst){

  if ( find(LoopInv_inst.begin(), LoopInv_inst.end(), op ) != LoopInv_inst.end()  ){
    outs() << "LINV instr: " << *op << '\n';
    return linv;
  }
  else if ( find(LoopInst.begin(), LoopInst.end(), op ) != LoopInst.end() ) {
    outs() << "NOT LINV instr" << *op << '\n';
    return not_linv;
  }
  else if ( isa<ConstantInt>(op) ){
    outs() << "LINV instr" << *op << '\n';
    return linv;
  }
  outs() << "UNKNOWN instr" << *op << '\n';

  return unknown;

}



/*
 *  Looks for loop invariant instructions and save them on LoopInv_ins 
 */
void LoopInvInstChecks(vector<Instruction*> &LoopInst, vector<Instruction*> &LoopInv_inst){

  // run through the vector until convergence is met (no unknown instructions left)
  while (!LoopInst.empty()){
    // for all instructions in loop
    outs() << "Istruzioni ancora unknown:\n";
    for (auto &I: LoopInst)
      outs() << *I << '\n';
    outs() << "--------------------------\n";
    for (auto it = LoopInst.begin(); it != LoopInst.end();){
      auto I = *it;
      outs() << "Istruzione attualmente analizzata: " << *I << '\n';
      // retrieve current position inside the vector
      // auto it = find(LoopInst.begin(), LoopInst.end(), I );
      // if is a valid instruction type for loop invariant checks
      if ( !I->isBinaryOp() && !I->isUnaryOp() && !I->isBitwiseLogicOp() ) {
        outs() << "ELIMINO perché non unary o binary\n";
        LoopInst.erase(it);
        outs() << "Istruzioni ancora unknown POST ERASE:\n";
        for (auto &I: LoopInst)
          outs() << *I << '\n';
        outs() << "--------------------------\n";
      }
      else{
        bool skip = false;

        for (auto Op = I->operands().begin(); Op != I->operands().end(); ++Op) {
          linv_t op_t = isLoopInvOp(*Op, LoopInst, LoopInv_inst);

          outs() << "linv_t: " << op_t << '\n';

          if ( op_t == unknown ){
            skip = true;
            it++;
            break;
          }
          else if ( op_t ==  not_linv ){
            LoopInst.erase(it);
            skip = true;
            break;
          }

        }
        // if none of the operands are loop-variant/unknown, the instruction can be labeled as loop-invariant
        if ( !skip ){
          outs() << "Pushing back instruction " << *I << '\n';
          LoopInv_inst.push_back(I);
          LoopInst.erase(it);
        }
      }
    }
  }
}

/*
* FindLoopInv function
* The function find the Loop Invariant Instructions and iterate until convergent
* Return the vector with the Loop Invariant Instructions
*/
vector<Instruction*> FindLoopInv(Loop &L) {
  /*
   * LoopInst contains all loop instructions
   */
  vector<Instruction*> LoopInst;
  vector<Instruction*> LoopInv_inst;

  // initialize LoopInst with all instructions
  InitializeLoopInst(LoopInst, &L);

  // check which instructions are loop invariant
  LoopInvInstChecks(LoopInst, LoopInv_inst);

  for ( auto &I: LoopInv_inst ){
    outs() << "Loop inv inst: " << *I << "\n";
  }

  // verificare istruzione binary bitwise, ternario

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
