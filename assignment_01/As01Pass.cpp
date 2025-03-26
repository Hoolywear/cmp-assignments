#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <cmath>

using namespace llvm;

/*
*   Get the operand number and the instruction and replace all uses with op
*/
void replaceUses(int opNumber, Instruction &I){
    Value *op = I.getOperand(opNumber);
    I.replaceAllUsesWith(op);
    // I.eraseFromParent();
}

/*
*   Return true if the given operand types are "opposite"
*   (specifically, we want add-sub and mul-sdiv pairs to be defined as such)
*/
bool areOppositeOps(int op1, int op2){
    return (op1 == Instruction::Add && op2 == Instruction::Sub)
    || (op1 == Instruction::Sub && op2 == Instruction::Add)
    || (op1 == Instruction::Mul && op2 == Instruction::SDiv)
    || (op1 == Instruction::SDiv && op2 == Instruction::Mul);
}

/*
* 	Algebraic Identity Pass
*/
bool AlgebraicIdentity(BasicBlock &B){
    outs() << "Algebraic Identity\n";
    for (Instruction& I : B) {
        // Check if the instruction is an add or sub
        if ((I.getOpcode() == Instruction::Add) || (I.getOpcode() == Instruction::Sub)) {
            // Get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // Check if the first operand is zero and the second one is not a constant
            if ( C1 && !C2 && C1->getValue().isZero() ) {
                replaceUses(1, I);
            }
            else if ( C2 && !C1 && C2->getValue().isZero() ) {
                replaceUses(0, I);
            }
        }
        // Check if the instruction is a mul or a sub (signed)
        if ((I.getOpcode() == Instruction::Mul) || (I.getOpcode() == Instruction::SDiv)) {
            // Get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // Check if the first operand is zero and the second one is not a constant
            if ( C1 && !C2 && C1->getValue().isOne() ) {
                replaceUses(1, I);
            }
            else if ( C2 && !C1 && C2->getValue().isOne() ) {
                replaceUses(0, I);
            }
        }
        outs() << I << "\n";
    }
    return true;
}


void LShiftReplace(int opNumb ,Instruction &I, ConstantInt *C){
    // Create a new shift instruction
    BinaryOperator *Shift = BinaryOperator::Create(Instruction::Shl, I.getOperand(opNumb), ConstantInt::get(I.getOperand(opNumb)->getType(), C->getValue().logBase2()), "shift", &I);
    // Replace all uses of the mul instruction with the shift instruction
    Shift->insertAfter(&I);
    I.replaceAllUsesWith(Shift);
}
void RShiftReplace(int opNumb ,Instruction &I, ConstantInt *C){
    // Create a new shift instruction
    BinaryOperator *Shift = BinaryOperator::Create(Instruction::AShr, I.getOperand(opNumb), ConstantInt::get(I.getOperand(opNumb)->getType(), C->getValue().logBase2()), "shift", &I);
    // Replace all uses of the mul instruction with the shift instruction
    Shift->insertAfter(&I);
    I.replaceAllUsesWith(Shift);
}
void ShiftSubReplace( int opNumb, Instruction &I, ConstantInt *C){
    // Create a new shift instruction
    BinaryOperator *Shift = BinaryOperator::Create(Instruction::Shl, I.getOperand(opNumb), ConstantInt::get(I.getOperand(opNumb)->getType(), (C->getValue()+1).logBase2()), "shift", &I);
    // Create a new sub instruction
    BinaryOperator *Sub = BinaryOperator::Create(Instruction::Sub, Shift, I.getOperand(opNumb), "sub", &I);
    // Replace all uses of the mul instruction with the sub instruction
    Shift->insertAfter(&I);
    Sub->insertAfter(Shift);
    I.replaceAllUsesWith(Sub);
}
void ShiftAddReplace(int opNumb, Instruction &I, ConstantInt *C){
    // Create a new shift instruction
    BinaryOperator *Shift = BinaryOperator::Create(Instruction::Shl, I.getOperand(opNumb), ConstantInt::get(I.getOperand(opNumb)->getType(), (C->getValue()-1).logBase2()), "shift", &I);
    // Create a new add instruction
    BinaryOperator *Add = BinaryOperator::Create(Instruction::Add, Shift, I.getOperand(opNumb), "add", &I);
    // Replace all uses of the mul instruction with the add instruction
    Shift->insertAfter(&I);
    Add->insertAfter(Shift);
    I.replaceAllUsesWith(Add);
}

/*
*     Advanced Strength Reduction Pass
*/
bool AdvancedStrengthReduction(BasicBlock &B){
    outs() << "Advanced Strength Reduction\n";
    std::vector<Instruction *> InstructionsToRemove;
    for (auto Inst = B.begin(); Inst != B.end(); ++Inst) { // Iteratore esplicito
        Instruction &I = *Inst;
        outs() << I << "\n";
        // Check if the instruction is a mul
        if (I.getOpcode() == Instruction::Mul) {
            /*
            * Base Strength Reduction 
            */
            // Get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0));
            ConstantInt *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // Check if the first operand is a power of 2 and the second one is not a constant
            /*
                Note that every case also check if the constant value is not 1 (Algebraic Identity optimization)
            */
            if ( C1 && !C2 && C1->getValue().isPowerOf2() ) {
                LShiftReplace(1, I, C1);
            }
            // Check if the second operand is a power of 2 and the first one is not a constant
            else if ( C2 && !C1 && C2->getValue().isPowerOf2()) {
                LShiftReplace(0, I, C2);
            }
            /*
            * Advanced Strength Reduction
            */
            // First operand is a constant power of 2 +1 and -1 and the second one is not a constant
            else if ( C1 && !C2 && (C1->getValue()+1).isPowerOf2() ){
                ShiftSubReplace(1, I, C1);
            }
            else if ( C1 && !C2 && (C1->getValue()-1).isPowerOf2() ){
                ShiftAddReplace(1, I, C1);
            }
            // Second operand is a constant power of 2 +1 and -1 and the second one is not a constant
            else if ( C2 && !C1 && (C2->getValue()+1).isPowerOf2() ){
                ShiftSubReplace(0, I, C2);
            }
            else if ( C2 && !C1 && (C2->getValue()-1).isPowerOf2() ){
                ShiftAddReplace(0, I, C2);
            }
        }
        if ( I.getOpcode() == Instruction::SDiv ){
            /*
            * Base Strength Reduction
            */
            // Get the operands of the instruction
            ConstantInt *C1 = dyn_cast<ConstantInt>(I.getOperand(0)), *C2 = dyn_cast<ConstantInt>(I.getOperand(1));
            // Check if the first operand is a power of 2 and the second one is not a constant
            if ( C1 && !C2 && C1->getValue().isPowerOf2()) {
                RShiftReplace(1, I, C1);
            }
            // Check if the second operand is a power of 2 and the first one is not a constant
            else if ( C2 && !C1 && C2->getValue().isPowerOf2()) {
                RShiftReplace(0, I, C2);
            }
        }
    }
    return true;
}

/*
 *      Multi-Instruction Optimization
 */

void multiInstructionSearch(Instruction &I, uint64_t currentValue) {
    // Check if instruction is a binary operation of interest (add, sub)
    unsigned int OP = I.getOpcode();
    if (OP == Instruction::Add || OP == Instruction::Sub) {
        Value* LHS = I.getOperand(0), *RHS = I.getOperand(1);
        ConstantInt* C1 = dyn_cast<ConstantInt>(LHS), *C2 = dyn_cast<ConstantInt>(RHS);
        Value* Substitute = nullptr;        // The non constant operand
        ConstantInt* ConstantOp = nullptr;  // The constant operand
        // Check if only one of the operands is a constant
        if (C1 && !C2 && OP != Instruction::Sub) {
            Substitute = RHS;
            ConstantOp = C1;
        } else if (!C1 && C2) {
            Substitute = LHS;
            ConstantOp = C2;
        } else { // If both operands are constants or both are not constants
            return; 
        }
        // Iterate over all users of the instruction
        for (auto Iter = I.user_begin(); Iter != I.user_end(); ++Iter) {
            Instruction* UserInst = dyn_cast<Instruction>(*Iter);
            // Skip iteration if the user is not a direct "opposite" of the usee
            if (!areOppositeOps(OP,UserInst->getOpcode())) {
                continue;
            } else {
                ConstantInt* UserC1 = dyn_cast<ConstantInt>(UserInst->getOperand(0)),*UserC2 = dyn_cast<ConstantInt>(UserInst->getOperand(1));
                uint64_t newValue = currentValue + ConstantOp->getZExtValue();
                if ((UserC2 && (UserC2->getZExtValue() == newValue)) ||
                (UserC1 && (UserC1->getZExtValue() == newValue))) {
                    outs() << "Substitute" << *UserInst << " with ";
                    Substitute->printAsOperand(outs(), false);
                    outs() << "\n";
                    UserInst->replaceAllUsesWith(Substitute);
                } else {
                    // RICHIAMA FUNZ RICORSIVAMENTE!
                    multiInstructionSearch(*UserInst,newValue);
                }
            }
        }
    }
}
bool MultiInstructionOptimization(BasicBlock &B){

    outs() << "Multi-Instruction Optimization\n";
    // For all intructions in the basic block
    for (Instruction& I : B){
        outs() << I << "\n";
        multiInstructionSearch(I,0);
    }
    return true;
}

bool runOnFunction(Function &F, int passNumb) {
    bool Transformed = false;
    // Iterate over all basic blocks in the function
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        if ( (passNumb == 1) && AlgebraicIdentity(*Iter))
        {
            Transformed = true;
        }
        else if ( (passNumb == 2) && AdvancedStrengthReduction(*Iter))
        {
            Transformed = true;
        }
        else if ( (passNumb == 3) && MultiInstructionOptimization(*Iter))
        {
            Transformed = true;
        }
    }

    return Transformed;
}


//-----------------------------------------------------------------------------
// Pass implementation
//-----------------------------------------------------------------------------
namespace {
    // First pass ( Algebraic Identity )
    struct As01Pass1: PassInfoMixin<As01Pass1> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool res = runOnFunction(F, 1);
        return PreservedAnalyses::all();
    }
    static bool isRequired() { return true; }
    };

    // Second pass ( Advanced Strength Reduction )
    struct As01Pass2: PassInfoMixin<As01Pass2> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
            bool res = runOnFunction(F, 2);
            return PreservedAnalyses::all();
    }
        static bool isRequired() { return true; }
    };

    // Third pass ( Multi-Instruction Optimization )
    struct As01Pass3: PassInfoMixin<As01Pass3> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
            bool res = runOnFunction(F, 3);
            return PreservedAnalyses::all();
    }
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
                  if (Name == "AlgId") {
                    FPM.addPass(As01Pass1());
                    return true;
                  }
                  if ( Name == "AdvStrRed" ){
                    FPM.addPass(As01Pass2());
                    return true;
                  }
                  if ( Name == "MultiInstOpt" ){
                    FPM.addPass(As01Pass3());
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