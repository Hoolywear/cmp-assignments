/*
* 	Algebraic Identity Pass
*/
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/*
*   get the number of the operand and replace all the uses of the instruction with the operand
*/
void replaceUses(int opNumber, Instruction &I){
    Value *op = I.getOperand(opNumber);
    I.replaceAllUsesWith(op);
    // I.eraseFromParent();
}

/*
* The function implements the Algebric Identity optimization pass
*/
bool AlgebraicIdentity(BasicBlock &B){

    for (Instruction& I : B) {
        
        outs() << I << "\n";

        // Check if the instruction is an add
        if (I.getOpcode() == Instruction::Add) {
            // get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // check if the first operand is zero and the second one is not a constant value
            if ( C1 && !C2 && C1->getValue().isZero() ) {
                replaceUses(1, I);
            }
            else if ( C2 && !C1 && C2->getValue().isZero() ) {
                replaceUses(0, I);
            }
        }
        // check if the instruction is a mul
        if (I.getOpcode() == Instruction::Mul) {
            // get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // check if the first operand is zero and the second one is not a constant value
            if ( C1 && !C2 && C1->getValue().isOne() ) {
                replaceUses(1, I);
            } 
            else if ( C2 && !C1 && C2->getValue().isOne() ) {
                replaceUses(0, I);
            }
        }
    }
    return true;
}

bool runOnFunction(Function &F) {
    bool Transformed = false;

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