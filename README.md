# Seminal Input Features Detection Tool

## Project Overview

This project implements a static analysis tool based on LLVM to detect the key input features that influence the behavior of a C program at critical execution points. Specifically, the tool aims to identify which parts of the program's input determine the behavior at key points, such as conditional branching and function pointer calls, as described in **Part 2**.

### Key Features

1. **Identification of Key Input Features**: The tool analyzes the input of the program and identifies which input variables affect the program's branching and function calls.
2. **Def-Use Analysis**: The tool uses a static analysis technique called *def-use analysis* to trace how program inputs affect control flow and the behavior of the program.

### Example Programs

1. **Example 2.1**: A simple program where the runtime behavior is determined by a loop that depends on the second input variable `n`.

```c++
int main(){
   int id;
   int n;
   scanf("%d, %d", &id, &n);
   int s = 0;
   for (int i=0; i<n; i++){
      s += rand();
   }
   printf("id=%d; sum=%d\n", id, s); 
}

```

**Key Input Feature**: In this program, only the value of n (the second input) determines the runtime behavior (the number of loop iterations). This illustrates the importance of loops in identifying important program features.

2. **Example 2.2 **: A program that reads characters from a file and stores them in an array. The runtime behavior is determined by the length of the input string, but a naive def-use analysis may incorrectly attribute the behavior to the individual characters.

```c++
int main(){
   int id;
   int n;
   scanf("%d, %d", &id, &n);
   int s = 0;
   for (int i=0; i<n; i++){
      s += rand();
   }
   printf("id=%d; sum=%d\n", id, s); 
}

```

**Key Input Feature**: The key input feature in this case is the length of the input string, not the individual characters.This illustrates the importance of identifying IO information.

### Objective

The main goal was to build a static analysis tool that can detect which parts of a program’s inputs are semantically relevant for determining key behaviors like branching decisions and function pointer invocations. This analysis is critical for understanding how input data influences the control flow and overall execution of a program.

### Requirements

- **LLVM/Clang**: The tool is implemented as an LLVM pass to perform the static analysis.
- **C Programming**: Input C programs to be analyzed by the tool.
- **Ubuntu 22.04 LTS**: The tool is designed to work on the NCSU VCL with Ubuntu 22.04 LTS.

### Setup Instructions

1. **Install Dependencies**: Ensure that LLVM, Clang, and other necessary dependencies are installed:

   ```bash
   sudo apt-get update
   sudo apt-get install llvm clang
   ```

2. **Building the Tool**: After setting up the LLVM environment, compile the tool that performs static analysis on input features. Follow the steps in the provided instructions for building the LLVM pass.

### Expected Output

For each analyzed program, the tool will output which parts of the input (or inputs) are relevant to the program’s runtime behavior. For example:

- In **Example 2.1**, the output will indicate that the value of `n` is the key input feature.
- In **Example 2.2**, the output will indicate that the length of the string is the key input feature, not the individual characters.

### Usage

   To run the profiling tools on a C program:

   **Seminal Feature Detetection:**

<!-- Copy over modified files from `~/code/llvm-files-p1` to a clone of the llvm repository named `llvm-source` (while preserving directory structure present in modified files).
- NOTE: these scripts assume you have a directory structure whereby your llvm repository is a sibling to `code` named `llvm-source`. -->

   1. Ensure you have llvm version 17 cloned to a sibling folder of `code` named `llvm-source`

   2. Change directory to `~/code/llvm-tools-p2`

   3. Build llvm with the added pass(es) by running `sudo ./llvm_build.sh`.

   4. After building the LLVM pass run `llvm_test.sh <test-name>` where test-name is the name of the specific file you would like to run the test on from `~/code/tests/`.

   > This will generate a json file `seminal-values.json` containing seminal and candidate seminal features (denoted as "Possible") It will also print the output into the terminal, displaying the results.
