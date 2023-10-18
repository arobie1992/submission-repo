#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class BranchEntry {
    public: 
    const int id;
    const StringRef file_name;
    const int condition_line;
    const int starting_line;

    BranchEntry(int id, StringRef file_name, int condition_line, int starting_line): 
        id(id), 
        file_name(file_name), 
        condition_line(condition_line), 
        starting_line(starting_line) 
    {}
};

struct KeyPointsPass : public PassInfoMixin<KeyPointsPass> {
    private: 
    std::set<BasicBlock*> seen;
    void handleSwitch(SwitchInst *SI) {

    };
    void handleBranch(BranchInst *BI) {
        errs() << "I'm a Branch Instruction: " << BI << "\n";
        int ctr = 0;
        // iterate over the branches
        // branch has first operand 0 as condition
        // others after that are actual basic block branches
        auto test = BI->getNumOperands();
        // errs() << "Seeing if this works: " << test << "\n";
        for(auto i = BI->op_begin(); i != BI->op_end(); i++, ctr++) {
            if (isa<BasicBlock>(*i)) {
                auto BB = dyn_cast<BasicBlock>(*i);
                // errs() << "Op " << ctr << " is a basic block\n";
                errs() << "BB name: ";
                BB->printAsOperand(errs(), false);
                errs() << "\n";

                if(seen.insert(BB).second) {
                    errs() << "New basic block\n";
                } else {
                    errs() << "Seen this basic block\n";
                }

                for (auto &I : *BB) {
                    errs() << "BB Instr: " << I << " at line: " << I.getDebugLoc().getLine() << "\n";
                    
                }
            }
        }
    };
    public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        errs() << "In Module: " << M.getName() << "\n";
        for (auto &F : M) {
            int counter = 0;
            errs() << "Function body:\n" << F << "\n";
            for (auto &B : F) {
                errs() << "Basic block:\n" << B << "\n";
                for (auto &I : B) {
                    // this is how to check if branch
                    if (isa<SwitchInst>(I)) {
                        auto SI = dyn_cast<SwitchInst>(&I);
                        handleSwitch(SI);
                    }
                    if (isa<BranchInst>(I)) {
                        auto BI = dyn_cast<BranchInst>(&I);
                        handleBranch(BI);
                    } else {
                        errs() << "Different Instruction: " << I << "\n";
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
