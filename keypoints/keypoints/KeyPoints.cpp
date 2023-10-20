// The starting skeleton for this code was from this post: https://www.cs.cornell.edu/~asampson/blog/llvm.html
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <iostream>
#include <fstream>

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

// this is unused at the moment, but keeping it because it's helpful
void printBranchEntry(BranchEntry &BE) {
    errs() << "br_" << BE.id << ": " << BE.file_name << ", " << BE.condition_line << ", " << BE.block_start_line << "\n";    
}

void writeBranchDictionary(std::vector<BranchEntry> &branchEntries) {
    std::ofstream branch_dict("branch_dictionary.txt");
    for (auto BE : branchEntries) {
        branch_dict << "br_" << BE.id << ": " << BE.file_name.str() << ", " << BE.condition_line << ", " << BE.block_start_line << "\n";
    }
    branch_dict.close();
}

struct KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
    private: 
    int counter;
    std::set<BasicBlock*> seen;
    std::vector<BranchEntry> branchEntries;
    int getStartLine(BasicBlock &BB) {
        for (auto &I : BB) {
            if (I.getDebugLoc()) {
                return I.getDebugLoc().getLine();
            }
        }
        // this thankfully hasn't come up so far, but if it does, 
        // it shouldn't cause program issues, just some funky output
        return -1;
    };
    void addBranchTag(Module &M, int condition_line, BasicBlock &BB) {
        if(!seen.insert(&BB).second) {
            // we've already seen this one and transformed it
            return;
        }
        BranchEntry BE(counter++, M.getName(), condition_line, getStartLine(BB));
        branchEntries.push_back(BE);
        for (auto &I : BB) {
            // code for print function adapted from this SO post: 
            // https://stackoverflow.com/questions/49558395/adding-a-simple-printf-in-a-llvm-pass
            // TODO switch this to fopen append and write: https://stackoverflow.com/questions/19429138/append-to-the-end-of-a-file-in-c
            LLVMContext &context = M.getContext();
            std::vector<Type *> printfArgsTypes({Type::getInt8PtrTy(context)});
            FunctionType *printfType = FunctionType::get(Type::getInt32Ty(context), printfArgsTypes, true);
            auto printfFunc = M.getOrInsertFunction("printf", printfType);
            IRBuilder<> builder(&I);
            Value *str = builder.CreateGlobalStringPtr("br_" + std::to_string(BE.id) + "\n", "str");
            std::vector<Value *> argsV({str});
            // TODO might add ID to the name
            builder.CreateCall(printfFunc, argsV, "brtag");
            // break after the first instruction because we only want to insert the tag at the start of the block
            break;
        }
    };
    void handleSwitch(Module &M, SwitchInst &SI) {
        if (!SI.getDebugLoc()) {
            // invalid debug location so don't attempt since getting the condition line will fail
            return;
        }
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
            // this results in the plugin essentially being a no-op if clang is run without -g
            return;
        }

        if (BI.isUnconditional()) {
            return;
        }

        auto condition = BI.getSuccessor(0);
        addBranchTag(M, BI.getDebugLoc().getLine(), *condition);
        auto alternative = BI.getSuccessor(1);
        addBranchTag(M, BI.getDebugLoc().getLine(), *alternative);
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
        writeBranchDictionary(branchEntries);
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
