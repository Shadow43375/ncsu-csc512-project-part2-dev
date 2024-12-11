#include "llvm/Transforms/Utils/FunctionPointerLogger.h"

using namespace llvm;

void BranchDictionary::addBranch(unsigned ID, std::string filename,
                                 unsigned sourceLine, unsigned targetLine) {
  branches[ID] = std::make_tuple(filename, sourceLine, targetLine);
}

void BranchDictionary::writeToFile(const std::string &filename) {
  std::error_code EC;
  raw_fd_ostream OS(filename, EC);
  for (const auto &entry : branches) {
    OS << "br_" << entry.first << ": " << std::get<0>(entry.second) << ", "
       << std::get<1>(entry.second) << ", " << std::get<2>(entry.second)
       << "\n";
  }
}

PreservedAnalyses FunctionPointerLoggerPass::run(Function &F,
                                                 FunctionAnalysisManager &AM) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();

  // Type definitions
  Type *Int8Ty = Type::getInt8Ty(Ctx);
  Type *Int8PtrTy = PointerType::getUnqual(Int8Ty);
  PointerType *FilePtrTy = PointerType::getUnqual(Int8PtrTy);

  // Function declarations
  FunctionType *FOpenTy =
      FunctionType::get(FilePtrTy, {Int8PtrTy, Int8PtrTy}, false);
  FunctionCallee FOpen = M->getOrInsertFunction("fopen", FOpenTy);

  FunctionType *FPrintfTy =
      FunctionType::get(Type::getInt32Ty(Ctx), {FilePtrTy, Int8PtrTy}, true);
  FunctionCallee FPrintf = M->getOrInsertFunction("fprintf", FPrintfTy);

  FunctionType *FCloseTy =
      FunctionType::get(Type::getInt32Ty(Ctx), {FilePtrTy}, false);
  FunctionCallee FClose = M->getOrInsertFunction("fclose", FCloseTy);

  // Global FILE* variable
  GlobalVariable *FilePtr =
      new GlobalVariable(*M, FilePtrTy, false, GlobalValue::ExternalLinkage,
                         ConstantPointerNull::get(FilePtrTy), "log_file");

  // Open file at the entry point
  if (F.getName() == "main") {
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
    Value *FileName = Builder.CreateGlobalString("branch-pointer_trace.txt");
    Value *Mode = Builder.CreateGlobalString("w");
    Value *FileHandle = Builder.CreateCall(FOpen, {FileName, Mode});
    Builder.CreateStore(FileHandle, FilePtr);
  }

  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *Call = dyn_cast<CallInst>(&I)) {
        if (Call->isIndirectCall()) {
          IRBuilder<> Builder(Call);
          Value *FuncPtr = Call->getCalledOperand();

          // Load FILE* from the global variable
          Value *FileHandle = Builder.CreateLoad(FilePtrTy, FilePtr);

          // Create format string
          Value *FormatStr = Builder.CreateGlobalStringPtr("*func_%p\n");

          // Create fprintf call
          Builder.CreateCall(FPrintf, {FileHandle, FormatStr, FuncPtr});
        }
      } else if (auto *Br = dyn_cast<BranchInst>(&I)) {
        if (Br->isConditional()) {
          const DebugLoc &DL = Br->getDebugLoc();
          if (DL) {
            // Get source location info
            StringRef Filename = DL->getFilename();
            unsigned SourceLine = DL.getLine();

            // Get branch IDs
            unsigned TrueBranchID = nextBranchID++;
            unsigned FalseBranchID = nextBranchID++;

            // Get target basic blocks
            BasicBlock *TrueDest = Br->getSuccessor(0);
            BasicBlock *FalseDest = Br->getSuccessor(1);

            // Insert logging in TrueDest
            {
              IRBuilder<> Builder(&*TrueDest->getFirstInsertionPt());
              // Load FILE* from the global variable
              Value *FileHandle = Builder.CreateLoad(FilePtrTy, FilePtr);
              Value *FormatStr = Builder.CreateGlobalStringPtr("br_%d\n");
              Builder.CreateCall(
                  FPrintf,
                  {FileHandle, FormatStr,
                   ConstantInt::get(Type::getInt32Ty(Ctx), TrueBranchID)});
            }

            // Insert logging in FalseDest
            {
              IRBuilder<> Builder(&*FalseDest->getFirstInsertionPt());
              // Load FILE* from the global variable
              Value *FileHandle = Builder.CreateLoad(FilePtrTy, FilePtr);
              Value *FormatStr = Builder.CreateGlobalStringPtr("br_%d\n");
              Builder.CreateCall(
                  FPrintf,
                  {FileHandle, FormatStr,
                   ConstantInt::get(Type::getInt32Ty(Ctx), FalseBranchID)});
            }

            // Get line numbers of the first instructions in the successor
            // blocks
            unsigned TrueDestLine = 0, FalseDestLine = 0;

            // Find first instruction with debug info in true branch
            for (Instruction &Inst : *TrueDest) {
              if (Inst.getDebugLoc()) {
                TrueDestLine = Inst.getDebugLoc().getLine();
                break;
              }
            }

            // Find first instruction with debug info in false branch
            for (Instruction &Inst : *FalseDest) {
              if (Inst.getDebugLoc()) {
                FalseDestLine = Inst.getDebugLoc().getLine();
                break;
              }
            }

            // Add branch info to dictionary
            branchDict.addBranch(TrueBranchID, Filename.str(), SourceLine,
                                 TrueDestLine);
            branchDict.addBranch(FalseBranchID, Filename.str(), SourceLine,
                                 FalseDestLine);
          }
        }
      }
    }
  }

  // Close the file at the end of main
  if (F.getName() == "main") {
    for (auto &BB : F) {
      if (isa<ReturnInst>(BB.getTerminator())) {
        IRBuilder<> Builder(BB.getTerminator());
        // Load FILE* from the global variable
        Value *FileHandle = Builder.CreateLoad(FilePtrTy, FilePtr);
        Builder.CreateCall(FClose, {FileHandle});
      }
    }
  }

  // Preserve logic for writing to branchdictionary.txt
  branchDict.writeToFile("branch-dictionary.txt");

  return PreservedAnalyses::all();
}