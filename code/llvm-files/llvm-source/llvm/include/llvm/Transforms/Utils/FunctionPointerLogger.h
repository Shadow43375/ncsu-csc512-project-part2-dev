#ifndef LLVM_TRANSFORMS_UTILS_FUNCTIONPOINTERLOGGER_H
#define LLVM_TRANSFORMS_UTILS_FUNCTIONPOINTERLOGGER_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include <map>
#include <string>

namespace llvm {

class BranchDictionary {
public:
    void addBranch(unsigned ID, std::string filename, unsigned sourceLine, 
                   unsigned targetLine);
    void writeToFile(const std::string &filename);
private:
    std::map<unsigned, std::tuple<std::string, unsigned, unsigned>> branches;
};

class FunctionPointerLoggerPass : public PassInfoMixin<FunctionPointerLoggerPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    static bool isRequired() { return true; }
private:
    BranchDictionary branchDict;
    unsigned nextBranchID = 1;
};

} // namespace llvm

#endif