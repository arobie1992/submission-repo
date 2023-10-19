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
            if (I.getDebugLoc()) {
                return I.getDebugLoc().getLine();
            }
        }
        // this is happening for the if/else if/else on one of the unconditional branches
        // need to check about that as well
        // could just skip ones that aren't valid, but that seems a bit hacky
        return -1;
    };
    void addBranchTag(Module &M, int condition_line, BasicBlock &BB) {
        if(!seen.insert(&BB).second) {
            // we've already seen this one and transformed it
            return;
        }
        BranchEntry BE(counter++, M.getName(), condition_line, getStartLine(BB));
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
    void handleSwitch(Module &M, SwitchInst &SI) {
        if (!SI.getDebugLoc()) {
            // invalid debug location so don't attempt since getting the condition line will fail
            return;
        }
        // errs() << SI << "\n";
        auto condition_line = SI.getDebugLoc().getLine();
        auto &D = *SI.getDefaultDest();
        for (auto i = 0; i < SI.getNumSuccessors(); i++) {
            auto &S = *SI.getSuccessor(i);
            if (&S == &D) {
                // skip the default block for the end
                continue;
            }
            addBranchTag(M, condition_line, S);
        }
        addBranchTag(M, condition_line, D);
    };
    void handleBranch(Module &M, BranchInst &BI) {
        if (!BI.getDebugLoc()) {
            // invalid debug location so don't attempt since getting the condition line will fail
            return;
        }
        // check with Dr. Shen to see if we need to worry about goto since thre's no good way to differentiate it from for loop jumps and it makes things look rather funky. Based on the assignment, seems like skipping unconditional ones makes more sense, but then we get into the somewhat counter-intuitive if vs. if-else behavior
        // also check about the branch for if vs if/else
        if (BI.isUnconditional()) {
            return;
            // it's some sort of immediate, unconditional jump, like a goto or the end of a block
            auto op = BI.getSuccessor(0);
            addBranchTag(M, BI.getDebugLoc().getLine(), *op);
        } else {
            // it's some sort of user-defined conditional like an if or a loop
            auto condition = BI.getSuccessor(0);
            addBranchTag(M, BI.getDebugLoc().getLine(), *condition);
            auto alternative = BI.getSuccessor(1);
            addBranchTag(M, BI.getDebugLoc().getLine(), *alternative);
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
                        handleSwitch(M, *SI);
                    }
                    if (isa<BranchInst>(I)) {
                        auto BI = dyn_cast<BranchInst>(&I);
                        handleBranch(M, *BI);
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
