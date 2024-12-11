/**
 * Seminal Input Detection Tool for LLVM.
 *
 * @file SeminalInputDetector.cpp
 * @brief tool analyzes the influence of external input variables on the
 * execution of a program within the LLVM framework. It tracks the
 * definition-use chains of variables and detects those that are influenced by
 * input functions such as `scanf`, `fopen`, and `getc`, among others. The tool
 * produces insights into how these input variables propagate through the
 * program, helping to identify potential sources of program behavior that are
 * affected by external inputs.
 *
 * @author James Bayne (jbayne)
 * @author Drew Cada (aecada2)
 * @institution North Carolina State University (NCSU)
 * @course CSC 512
 * @instructor Xipeng Shen
 *
 * @tool_name seminal_input_detector
 * @version 1.0.0
 * @description A tool to detect and track input-influenced variables in an LLVM
 * program.
 *
 * This tool utilizes LLVM's pass framework to perform static analysis on the
 * control flow and data flow of a program to detect variables that are
 * influenced by user input. This can help identify which parts of the program
 * are most sensitive to external inputs, assisting with debugging,
 * optimization, and understanding of program behavior in contexts where input
 * variables are critical.
 */

// IO and datastructure imports
#include <fstream>
#include <llvm/IR/PassManager.h>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

// LLVM imports
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

// standard json libary import
#include "nlohmann/json.hpp"

#include "llvm/Transforms/Utils/SeminalInputDetector.h"

// using standard llvm namespace
using namespace llvm;

using Json = nlohmann::json;

/**
 * Represents information about a variable, including its name and the line
 * number where it is defined or used. This structure is used to track
 * variable details in accordance with def-use analysis requirements.
 */
struct VarInfo {
  /** The name of the variable. This field stores the variable's identifier,
   * typically as a string.*/
  std::string name;

  /**
   * The line number where the variable is defined or used.
   * This field helps to track the location of the variable in the source
   * code. A value of -1 indicates an invalid or undefined line number.
   */
  int line;

  /**
   * Default constructor for VarInfo. Initializes name as an empty string and
   * line as -1. The default line number of -1 indicates that no valid line
   * has been assigned.
   */
  VarInfo() : name(""), line(-1) {}

  /**
   * Parameterized constructor for VarInfo. Initializes the variable with the
   * provided name and line number.
   *
   * @param n The name of the variable.
   * @param l The line number where the variable is defined or used.
   */
  VarInfo(std::string n, int l) : name(n), line(l) {}
};

nlohmann::json importantVar;

struct JsonFileWriter {
  ~JsonFileWriter() {
    std::ofstream file("seminal-values.json");
    file << importantVar.dump(4);
    file.close();
  }
} jsonFileWriter;

// Declare the function prototype at the beginning
DbgDeclareInst *getDbg(Value *targetValue, Function *F);

void getDefUseChain(Value *value, std::set<Value *> *visited,
                    std::unordered_map<std::string, VarInfo> *variableMap,
                    Function *F);

// ---- HELPER FUNCTIONS ----

/**
 * Handles allocations (AllocaInst), adding relevant variable information to the
 * map.
 *
 * @param instruction The instruction that might be an allocation.
 * @param function The function in which the allocation occurs.
 * @param VarInfoMap A map to store variables and their information.
 */
void handleAllocations(Instruction *instruction, Function *function,
                       std::unordered_map<std::string, VarInfo> *VarInfoMap) {
    // CHECK: Ensure that the required pointers are not null
  if (!instruction) {
    llvm::errs() << "Error: Null instruction passed to handleAllocations.\n";
    return;  // Exit early if the instruction is null
  }

  if (!function) {
    llvm::errs() << "Error: Null function passed to handleAllocations.\n";
    return;  // Exit early if function is null
  }

  if (!VarInfoMap) {
    llvm::errs() << "Error: Null VarInfoMap passed to handleAllocations.\n";
    return;  // Exit early if VarInfoMap is null
  }

  // Exit early if the instruction is not an AllocaInst
  AllocaInst *allocaInst = dyn_cast<AllocaInst>(instruction);
  if (!allocaInst)
    return;

  // Exit early if no dbgDeclare or variable is found
  DbgDeclareInst *dbgDeclare = getDbg(allocaInst, function);
  if (!dbgDeclare || !dbgDeclare->getVariable())
    return;

  std::string varName = dbgDeclare->getVariable()->getName().str();
  int lineNo = dbgDeclare->getDebugLoc().getLine();
  // Track the variable allocation in the map
  (*VarInfoMap)[varName] = VarInfo(varName, lineNo);
}

/**
 * Handles stores (StoreInst), tracking the definition-use chain for stored
 * values and locations.
 *
 * @param instruction The instruction that might be a store.
 * @param seenValues A set of visited values to avoid re-processing.
 * @param VarInfoMap A map to store variables and their information.
 * @param function The function in which the store occurs.
 */
void handleStores(Instruction *instruction, std::set<Value *> *seenValues,
                  std::unordered_map<std::string, VarInfo> *VarInfoMap,
                  Function *function) {

    // CHECK: Ensure that the required pointers are not null
  if (!instruction) {
    llvm::errs() << "Error: Null instruction passed to handleStores.\n";
    return;  // Exit early if the instruction is null
  }

  if (!seenValues) {
    llvm::errs() << "Error: Null seenValues passed to handleStores.\n";
    return;  // Exit early if seenValues is null
  }

  if (!VarInfoMap) {
    llvm::errs() << "Error: Null VarInfoMap passed to handleStores.\n";
    return;  // Exit early if VarInfoMap is null
  }

  if (!function) {
    llvm::errs() << "Error: Null Function passed to handleStores.\n";
    return;  // Exit early if function is null
  }

  // Exit early if the instruction is not a StoreInst
  StoreInst *storeInst = dyn_cast<StoreInst>(instruction);
  if (!storeInst)
    return;

  Value *storedValue = storeInst->getValueOperand();
  Value *storedLocation = storeInst->getPointerOperand();
  // Track the def-use chains for stored values and locations
  getDefUseChain(storedValue, seenValues, VarInfoMap, function);
  getDefUseChain(storedLocation, seenValues, VarInfoMap, function);
}

/**
 * Finds the DbgDeclareInst for a given value in a function.
 *
 * @param targetValue The value whose DbgDeclareInst we are searching for.
 * @param function The function in which the DbgDeclareInst might be found.
 * @return A pointer to the DbgDeclareInst if found, or nullptr if not found.
 */
DbgDeclareInst *getDbg(Value *targetValue, Function *F) {

  // CHECK: Ensure that the required pointers are not null
  if (!targetValue) {
    llvm::errs() << "Error: Null targetValue passed to getDbg.\n";
    return nullptr;  // Return nullptr to indicate failure
  }

  if (!F) {
    llvm::errs() << "Error: Null Function passed to getDbg.\n";
    return nullptr;  // Return nullptr to indicate failure
  }

  // Iterate over each BasicBlock in the Function
  for (auto blockIt = F->begin(); blockIt != F->end(); ++blockIt) {
    BasicBlock &basicBlock = *blockIt;

    // Iterate over each Instruction in the BasicBlock
    for (auto instIt = basicBlock.begin(); instIt != basicBlock.end();
         ++instIt) {
      Instruction &instruction = *instIt;

      // Check if the Instruction is a DbgDeclareInst
      if (DbgDeclareInst *dbgDeclareInst =
              dyn_cast<DbgDeclareInst>(&instruction)) {
        // Check if the address of the DbgDeclareInst matches the given
        // targetValue
        if (dbgDeclareInst->getAddress() == targetValue) {
          return dbgDeclareInst;
        }
      }
    }
  }

  return nullptr; // Return null if no matching DbgDeclareInst is found
}

/**
 * Recursively finds the definition-use chain of a given value, recording
 * important variables.
 *
 * @param value The value to start tracking from.
 * @param visited A set of visited values to avoid processing the same value
 * multiple times.
 * @param variableMap A map to store variables and their information.
 * @param F The function in which the value resides.
 */
void getDefUseChain(Value *value, std::set<Value *> *visited,
                    std::unordered_map<std::string, VarInfo> *variableMap,
                    Function *F) {

      // CHECK: Ensure that the required pointers are not null
  if (!value) {
    llvm::errs() << "Error: Null value passed to getDefUseChain.\n";
    return;
  }
  
  if (!visited) {
    llvm::errs() << "Error: Null visited set passed to getDefUseChain.\n";
    return;
  }

  if (!variableMap) {
    llvm::errs() << "Error: Null variable map passed to getDefUseChain.\n";
    return;
  }

  if (!F) {
    llvm::errs() << "Error: Null function passed to getDefUseChain.\n";
    return;
  }

  // CHECK: the value has already been visited, exit early to avoid infinite
  // loops
  if (!visited->insert(value).second) {
    return;
  }

  // If the value is an instruction, process it
  if (Instruction *inst = dyn_cast<Instruction>(value)) {

    // If the instruction is a LoadInst (i.e., loading a value from memory)
    if (LoadInst *LoadInstVar = dyn_cast<LoadInst>(inst)) {
      Value *loadedValue =
          LoadInstVar->getPointerOperand(); // Get the pointer being loaded from

      // Find the DbgDeclareInst for the loaded value
      DbgDeclareInst *DbgDeclare = getDbg(loadedValue, F);
      if (DbgDeclare && DbgDeclare->getVariable()) {
        // If we find a DbgDeclare, record the variable's name and location in
        // the variable map
        std::string varName = DbgDeclare->getVariable()->getName().str();
        int lineNo = DbgDeclare->getDebugLoc().getLine();
        (*variableMap)[varName] =
            VarInfo(varName, lineNo); // Add the variable info to the map
      }

      // Recursively track the loaded value (i.e., follow the def-use chain)
      getDefUseChain(loadedValue, visited, variableMap, F);

      // If the instruction is a StoreInst (i.e., storing a value into memory)
    } else if (StoreInst *StoreInstVar = dyn_cast<StoreInst>(inst)) {
      Value *storedValue =
          StoreInstVar->getValueOperand(); // Get the value being stored
      Value *storedLocation =
          StoreInstVar
              ->getPointerOperand(); // Get the location where it's being stored

      // Recursively track both the stored value and the location (i.e., follow
      // the def-use chain)
      getDefUseChain(storedValue, visited, variableMap, F);
      getDefUseChain(storedLocation, visited, variableMap, F);

      // If the instruction is a CallInst (i.e., a function call)
    } else if (CallInst *CI = dyn_cast<CallInst>(inst)) {

      // If the call has a return value, track it recursively (ignoring
      // void-returning calls)
      if (CI->getType() != Type::getVoidTy(F->getContext())) {
        getDefUseChain(CI, visited, variableMap, F);
      }

      // Track all the arguments passed to the function call
      for (auto arg = CI->arg_begin(); arg != CI->arg_end(); ++arg) {
        Value *argValue = *arg;
        getDefUseChain(argValue, visited, variableMap,
                       F); // Recursively track each argument
      }

      // If the instruction is of some other type (e.g., binary operation,
      // comparison)
    } else {
      // Recursively track all operands of the instruction
      for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
        getDefUseChain(inst->getOperand(i), visited, variableMap, F);
      }
    }
  }
}

// ---- END HELPER FUNCTIONS ----

// ---- IO FUNCTIONS ----

/**
 * Creates a JSON object representing the influential variables in the function.
 *
 * @param variableMap A map of variable names to their information.
 * @param ioVar A set of IO variables that have been identified.
 * @return A JSON array representing the influential variables.
 */
Json createVariablesJson(const std::unordered_map<std::string, VarInfo> *varMap,
                         const std::unordered_set<std::string> *ioVar) {

    // CHECK: Ensure that the required pointers are not null
  if (!varMap) {
    llvm::errs() << "Error: Null varMap passed to createVariablesJson.\n";
    return Json::array();  // Return an empty array to indicate failure
  }

  if (!ioVar) {
    llvm::errs() << "Error: Null ioVar passed to createVariablesJson.\n";
    return Json::array();  // Return an empty array to indicate failure
  }
                          
  // Iterate through all variables to find IO and potential influential
  // variables
  bool found = false;
  Json variablesJson = Json::array();
  for (auto it = varMap->begin(); it != varMap->end(); ++it) {
    const VarInfo &info =
        it->second; // Access the value (VarInfo) using the iterator
    Json jvar;

    if (ioVar->count(info.name) > 0) {
      jvar["type"] = "IO";
      jvar["name"] = info.name;
      jvar["line"] = info.line;
      found = true;
    } 
    // else {
    //   jvar["type"] = "Possible";
    //   jvar["name"] = info.name;
    //   jvar["line"] = info.line;
    // }

    variablesJson.push_back(jvar); // Add the variable to the JSON
  }

  return variablesJson;
}

// ---- END IO FUNCTIONS ----

// ----  Core Functions ----

/**
 * Processes the loops in the given LoopInfo object, tracking variables involved
 * in the loop.
 *
 * @param LI The LoopInfo containing the loops to process.
 * @param seen A set of values that have already been visited.
 * @param vMap A map to store variables and their information.
 * @param F The function in which the loops reside.
 */
void processLoops(LoopInfo *LI, std::set<Value *> *seen,
                  std::unordered_map<std::string, VarInfo> *vMap, Function *F) {
  // Iterate over each loop in LoopInfo
  for (auto it = LI->begin(); it != LI->end(); ++it) {
    Loop *loop = *it;

    // Get the loop's header basic block
    BasicBlock *header = loop->getHeader();

    // Iterate over each Instruction in the header block
    for (auto I = header->begin(); I != header->end(); ++I) {

      // Check: Is this a conditional branch (Branch Instruction)?
      if (auto *BI = dyn_cast<BranchInst>(&*I)) {
        if (BI->isConditional()) {
          // If it's a conditional branch, analyze its condition
          Value *condition = BI->getCondition(); // Get the condition operand

          // Check: Is this condition an Instruction?
          if (Instruction *inst = dyn_cast<Instruction>(condition)) {
            // Iterate over all operands of the instruction
            for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
              // Track the definition-use chain for each operand
              getDefUseChain(inst->getOperand(i), seen, vMap, F);
            }
          }
        }
      }
    }
  }
}

/**
 * Analyzes input-related functions in the provided function.
 *
 * @param function The function in which input-related functions are to be
 * analyzed.
 * @param VarInfoMap A map to store variable information encountered during
 * analysis.
 */
void analyzeInputFunctions(Function *function,
                           std::unordered_map<std::string, VarInfo> *VarInfoMap,
                           std::unordered_set<std::string> *ioVar) {
  // Search for input-related variables.
  for (auto blockIt = function->begin(); blockIt != function->end();
       ++blockIt) {
    BasicBlock &basicBlock = *blockIt;

    // Iterate over each instruction in the basic block
    for (auto instIt = basicBlock.begin(); instIt != basicBlock.end();
         ++instIt) {
      Instruction &instruction = *instIt;

      // CHECK: is this is a call instruction
      if (CallInst *instPointer = dyn_cast<CallInst>(&instruction)) {
        Function *funPointer = instPointer->getCalledFunction();
        if (!funPointer) {
          continue; // Skip if no called function
        }

        std::string funcName = funPointer->getName().str();

        // Simulating a "switch" using a map or series of if-else for function
        // names
        if (funcName.find("scanf") != std::string::npos) {
          // Handle "scanf" like input function
          for (unsigned argIdx = 0; argIdx < instPointer->arg_size();
               ++argIdx) {
            Value *argValue = instPointer->getArgOperand(argIdx);
            DbgDeclareInst *dbgDeclare = getDbg(argValue, function);
            if (dbgDeclare && dbgDeclare->getVariable()) {
              std::string varName = dbgDeclare->getVariable()->getName().str();
              int lineNo = dbgDeclare->getDebugLoc().getLine();

              (*VarInfoMap)[varName] = VarInfo(varName, lineNo);
              ioVar->insert(varName);
            }
          }
        } else if (funcName.find("fopen") != std::string::npos) {
          // Handle "fopen" like input function
          for (auto iter = instPointer->getIterator(); iter != basicBlock.end();
               ++iter) {
            if (StoreInst *storeInst = dyn_cast<StoreInst>(&*iter)) {
              if (storeInst->getValueOperand() == instPointer) {
                Value *storedLocation = storeInst->getPointerOperand();
                DbgDeclareInst *dbgDeclare = getDbg(storedLocation, function);

                if (dbgDeclare && dbgDeclare->getVariable()) {
                  std::string varName =
                      dbgDeclare->getVariable()->getName().str();
                  int lineNo = dbgDeclare->getDebugLoc().getLine();

                  (*VarInfoMap)[varName] = VarInfo(varName, lineNo);
                  ioVar->insert(varName);
                  break; // Stop after finding the first I/O variable related to
                         // fopen
                }
              }
            }
          }
        } else if (funcName.find("getc") != std::string::npos) {
          // Handle "getc" like input function
          for (unsigned argIdx = 0; argIdx < instPointer->arg_size();
               ++argIdx) {
            Value *argValue = instPointer->getArgOperand(argIdx);
            DbgDeclareInst *dbgDeclare = getDbg(argValue, function);
            if (dbgDeclare && dbgDeclare->getVariable()) {
              std::string varName = dbgDeclare->getVariable()->getName().str();
              int lineNo = dbgDeclare->getDebugLoc().getLine();

              (*VarInfoMap)[varName] = VarInfo(varName, lineNo);
              ioVar->insert(varName);
            }
          }
        }
      }
    }
  }
}

/**
 * Pairs input variables with termination variables and prints or processes
 * them.
 *
 * @param variableMap A map containing variable names and their information.
 * @param ioVar A set of IO variables that have been identified.
 * @param F The function being analyzed (used to get the function name).
 */
void pairInputTerminal(std::unordered_map<std::string, VarInfo> *variableMap,
                       const std::unordered_set<std::string> *ioVar,
                       Function *F) {
  // Create JSON for the variables
  Json functionJson;
  functionJson["function"] =
      F->getName().str(); // Use F.getName() to get the function name
  Json variablesJson = createVariablesJson(variableMap, ioVar);

  if (!variablesJson.empty()) {
    functionJson["important_variables"] = variablesJson;
    importantVar.push_back(
        functionJson); // Assuming importantVar is defined elsewhere
  }
}

/**
 * Handles the detection and analysis of variables in a function, including
 * allocations and stores.
 *
 * @param function The function in which variable information is to be handled.
 * @param VarInfoMap A map for storing variable information.
 * @param seenValues A set of visited values to avoid re-processing.
 */
void handleFunctionVariables(
    Function *function, std::unordered_map<std::string, VarInfo> *VarInfoMap,
    std::set<Value *> *seenValues) {
  for (auto BBIt = function->begin(); BBIt != function->end(); ++BBIt) {
    BasicBlock &basicBlock = *BBIt;
    // Loop over all instructions inside the basic block
    for (auto IIt = basicBlock.begin(); IIt != basicBlock.end(); ++IIt) {
      Instruction &instruction = *IIt;

      // Handle allocations
      handleAllocations(&instruction, function, VarInfoMap);

      // Handle stores
      handleStores(&instruction, seenValues, VarInfoMap, function);
    }
  }
}

// ---- END CORE FUNCTIONS ----

// ---- CLIENT FUNCTION ----

/**
 * Analyzes the given function to detect influential variables.
 *
 * @param function The function to analyze.
 * @param loopInfo The loop information used in the analysis.
 */
void analyze(Function *function, LoopInfo *loopInfo) {
  std::unordered_set<std::string> ioVar;
  std::set<Value *> seenValues;
  std::unordered_map<std::string, VarInfo> VarInfoMap;

  // Locate loops in the given function
  processLoops(loopInfo, &seenValues, &VarInfoMap, function);

  // Find source of all function variables
  handleFunctionVariables(function, &VarInfoMap, &seenValues);

  // Search for input-related variables.
  analyzeInputFunctions(function, &VarInfoMap, &ioVar);

  // Pair input variable with termination variable and get the variable line
  // number and name
  pairInputTerminal(&VarInfoMap, &ioVar, function);
}

// ---- END CLIENT FUNCTION ----

// ---- PASS DEFINITION ----

/**
 * Executes the Seminal Input Detector pass on a given function.
 *
 * @param F The function to analyze.
 * @param FAM The function analysis manager providing loop analysis results.
 * @return The preserved analyses after the pass runs.
 */
PreservedAnalyses SeminalInputDetectorPass::run(Function &F,
                                                FunctionAnalysisManager &FAM) {

  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  analyze(&F, &LI);
  return PreservedAnalyses::all();
}
// ---- END PASS DEFINITION ----
