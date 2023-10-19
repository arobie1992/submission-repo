#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace llvm;

namespace {

class BranchEntry {
    public: 
    const int id;
    const StringRef file_name;
    const int condition_line;
    const int block_start_line;

    BranchEntry(int id, StringRef file_name, int condition_line, int block_start_line): 
        id(id), 
        file_name(file_name), 
        condition_line(condition_line), 
        block_start_line(block_start_line) 
    {}
};

void printBranchEntry(BranchEntry &BE) {
    errs() << "br_" << BE.id << ": " << BE.file_name << ", " << BE.condition_line << ", " << BE.block_start_line << "\n";    
}

struct KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
    private: 
    int counter;
    std::set<BasicBlock*> seen;
    int getStartLine(BasicBlock &BB) {
        for (auto &I : BB) {
            return I.getDebugLoc().getLine();
        }
        // this shouldn't happen, but shouldn't cause a ton of issues if it does
        // mostly will probably just be some funky output in the dictionary
        return -1;
    };
    void addBranchTag(Module &M, StringRef file_name, int condition_line, BasicBlock &BB) {
        if(!seen.insert(&BB).second) {
            // we've already seen this one and transformed it
            return;
        }
        BranchEntry BE(counter++, file_name, condition_line, getStartLine(BB));
        printBranchEntry(BE);
        for (auto &I : BB) {
            LLVMContext &context = M.getContext();
            std::vector<Type *> printfArgsTypes({Type::getInt8PtrTy(context)});
            FunctionType *printfType = FunctionType::get(Type::getInt32Ty(context), printfArgsTypes, true);
            auto printfFunc = M.getOrInsertFunction("printf", printfType);
            IRBuilder<> builder(&I);
            Value *str = builder.CreateGlobalStringPtr("br_" + std::to_string(BE.id) + "\n", "str");
            std::vector<Value *> argsV({str});
            builder.CreateCall(printfFunc, argsV, "calltmp");
            break;
        }
    };
    void handleSwitch(StringRef file_name, SwitchInst &SI) {
    };
    void handleBranch(Module &M, StringRef file_name, BranchInst &BI) {
        if (!BI.getDebugLoc()) {
            // invalid debug location so don't attempt since getting the start line will fail
            return;
        }
        // check with Dr. Shen to see if we need to worry about goto since thre's no good way to differentiate it from for loop jumps and it makes things look rather funky.
        // also check about the branch for if vs if/else
        auto num_ops = BI.getNumOperands();
        if (num_ops == 1) {
            // it's some sort of immediate, unconditional jump, like a goto or the end of a block
            auto op = BI.getOperand(0);
            if (isa<BasicBlock>(op)) {
                auto BB = dyn_cast<BasicBlock>(op);
                addBranchTag(M, file_name,BI.getDebugLoc().getLine(), *BB);
            }
        } else if (num_ops == 3) {
            // it's some sort of user-defined conditional like an if or a loop
            auto condition = BI.getOperand(2);
            if (isa<BasicBlock>(condition)) {
                auto BB = dyn_cast<BasicBlock>(condition);
                addBranchTag(M, file_name,BI.getDebugLoc().getLine(), *BB);
            }
            auto alternative = BI.getOperand(1);
            if (isa<BasicBlock>(alternative)) {
                auto BB = dyn_cast<BasicBlock>(alternative);
                addBranchTag(M, file_name,BI.getDebugLoc().getLine(), *BB);
            }
        }
    };
    public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        // errs() << "In Module: " << M.getName() << "\n";
        for (auto &F : M) {
            int counter = 0;
            for (auto &B : F) {
                // errs() << "Basic block:\n" << B << "\n";
                for (auto &I : B) {
                    // this is how to check which type instruction is
                    if (isa<SwitchInst>(I)) {
                        auto SI = dyn_cast<SwitchInst>(&I);
                        handleSwitch(M.getName(), *SI);
                    }
                    if (isa<BranchInst>(I)) {
                        // errs() << "Branch: " << I << "\n";
                        if(I.getDebugLoc()) {
                            // errs() << "Debug is valid\n";
                        } else {
                            // errs() << "Debug is invalid\n";
                        }
                        auto BI = dyn_cast<BranchInst>(&I);
                        handleBranch(M, M.getName(), *BI);
                    } else {
                        // errs() << "Different Instruction: " << I << "\n";
                    }
                }
            }
            // errs() << "Function body:\n" << F << "\n";
        }
        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "KeyPoints pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(KeyPointsPass());
                });
        }
    };
}
