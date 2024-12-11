// SeminalInputDetector.h

#ifndef SEMINAL_INPUT_DETECTOR_H
#define SEMINAL_INPUT_DETECTOR_H

#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

#include "nlohmann/json.hpp"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassPlugin.h"

namespace llvm {



class SeminalInputDetectorPass
    : public PassInfoMixin<SeminalInputDetectorPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
  static bool isRequired() { return true; }
  


};

} // namespace llvm

#endif // SEMINAL_INPUT_DETECTOR_H