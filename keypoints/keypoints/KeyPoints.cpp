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

struct KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
    private: 
    int counter;
    std::set<BasicBlock*> seen;
    void handleSwitch(StringRef file_name, SwitchInst &SI) {
    };
    void handleBranch(Module &M, StringRef file_name, BranchInst &BI) {
        // errs() << "I'm a Branch Instruction: " << BI << "\n";
        
        auto num_ops = BI.getNumOperands();
        // errs() << "Num ops: " << num_ops << "\n";
        // if it has under 3 ops, then it likely means that it's a synthetic branch and not one we want to analyze
        if (num_ops < 3) {
            return;
        }
        auto condition_line = BI.getDebugLoc().getLine();
        int ctr = 0;
        // iterate over the branches
        // branch has first operand 0 as condition
        // others after that are actual basic block branches
        // errs() << "Seeing if this works: " << test << "\n";
        for(auto i = BI.op_begin(); i != BI.op_end(); i++, ctr++) {
            if (isa<BasicBlock>(*i)) {
                auto BB = dyn_cast<BasicBlock>(*i);
                // errs() << "Op " << ctr << " is a basic block\n";
                // errs() << "BB name: ";
                // BB->printAsOperand(errs(), false);
                // errs() << "\n";

                if(seen.insert(BB).second) {
                    // errs() << "New basic block\n";
                } else {
                    // errs() << "Seen this basic block\n";
                    continue;
                }

                int start_line;
                for (auto &I : *BB) {
                    // get the starting line for the branch dictionary
                    start_line = I.getDebugLoc().getLine();
                    break;
                }
                BranchEntry BE(counter++, file_name, condition_line, start_line);
                // errs() << "br_" << BE.id << ": " << BE.file_name << ", " << BE.condition_line << ", " << BE.block_start_line << "\n";
                for (auto &I : *BB) {
                    // insert the instruction
                    LLVMContext &context = M.getContext();

                    // Declare C standard library printf 
                    std::vector<Type *> printfArgsTypes({Type::getInt8PtrTy(context)});
                    FunctionType *printfType = FunctionType::get(Type::getInt32Ty(context), printfArgsTypes, true);
                    auto printfFunc = M.getOrInsertFunction("printf", printfType);

                    IRBuilder<> builder(&I);
                    Value *str = builder.CreateGlobalStringPtr("br_" + std::to_string(BE.id) + "\n", "str");
                    std::vector<Value *> argsV({str});
                    builder.CreateCall(printfFunc, argsV, "calltmp");
                    break;
                }
            }
        }
    };
    public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        // errs() << "In Module: " << M.getName() << "\n";
        for (auto &F : M) {
            int counter = 0;
            // errs() << "Function body:\n" << F << "\n";
            for (auto &B : F) {
                // errs() << "Basic block:\n" << B << "\n";
                for (auto &I : B) {
                    // this is how to check which type instruction is
                    if (isa<SwitchInst>(I)) {
                        auto SI = dyn_cast<SwitchInst>(&I);
                        handleSwitch(M.getName(), *SI);
                    }
                    if (isa<BranchInst>(I)) {
                        auto BI = dyn_cast<BranchInst>(&I);
                        handleBranch(M, M.getName(), *BI);
                    } else {
                        // errs() << "Different Instruction: " << I << "\n";
                    }
                }
            }
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
