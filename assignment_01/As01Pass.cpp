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

using namespace llvm;

// 𝑥 + 0 = 0 + 𝑥 --> 𝑥
// 𝑥 × 1 = 1 × 𝑥 --> 𝑥
bool AlgebraicIdentity(BasicBlock &B){

    for (Instruction& I : B) {

        outs() << I << "\n";
        
        // Check if the instruction is a sum
        if (I.getOpcode() == Instruction::Add) {
            outs() << "Found a sum\n";
            
            // get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            /*
            */
            if ( C1 && !C2 && C1->getValue().isZero() ) {
                outs() << "Found a zero\n";
                Value *op2 = I.getOperand(1);
                I.replaceAllUsesWith(op2);
                // I.eraseFromParent();
            } 
            else if ( C2 && !C1 && C2->getValue().isZero() ) {
                outs() << "Found a zero\n";
                Value *op1 = I.getOperand(0);
                I.replaceAllUsesWith(op1);
                // I.eraseFromParent();
            }
        }
        
        if (I.getOpcode() == Instruction::Mul) {
            outs() << "Found a mul\n";
            
            // get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            /*
            */
            if ( C1 && !C2 && C1->getValue().isOne() ) {
                outs() << "Found a one\n";
                Value *op2 = I.getOperand(1);
                I.replaceAllUsesWith(op2);
                // I.eraseFromParent();
            } 
            else if ( C2 && !C1 && C2->getValue().isOne() ) {
                outs() << "Found a one\n";
                Value *op1 = I.getOperand(0);
                I.replaceAllUsesWith(op1);
                // I.eraseFromParent();
            }
        }



    }

    return true;
}

bool runOnFunction(Function &F) {
    bool Transformed = false;
    /*
        Iterare sulla funzione significa iterare sul suo CFG
    */
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        if (AlgebraicIdentity(*Iter)) {
            Transformed = true;
        }
    }

    return Transformed;
}


//-----------------------------------------------------------------------------
// TestPass implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {


// New PM implementation
struct As01Pass: PassInfoMixin<As01Pass> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  
  /* APPUNTI
   *	andiamo ad estendere questa funzione
   */
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {

    bool res = runOnFunction(F);

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
  return {LLVM_PLUGIN_API_VERSION, "as-01-pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "as-01-pass") {
                    FPM.addPass(As01Pass());
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