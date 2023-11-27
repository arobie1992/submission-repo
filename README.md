# CSC 512 Project Fall 2023 part 1

## 1 Prerequisites
Since this is intended to be run on the VCL Ubuntu 22.04 instances, it has been built to run on there. You will need the followig dependencies for all pieces to run successfully.

- cmake 
- clang-15 
- lldb-15 
- lld-15 
- clangd-15
- valgrind

If any of these dependencies are not met, the scripts that require them should exit gracefully with an appropriate error message. To install these, you can use the following command: 
```
sudo apt install -y cmake clang-15 lldb-15 lld-15 clangd-15 valgrind
```

To check if these are installed, you can run `which {dep}` where `{dep}` is one of the listed dependencies. If the dependency is installed, the path to the executable will be displayed. If it is not, nothing will be printed.

## 2 Instrumenting Code
First, build the plugin by running the `keypoints/buildplugin.sh` script. You can attempt to build it manually by following the commands in the script, but the script should prove more convenient without restricting flexibility. This will generate a plugin in the `keypoints/build/keypoints` directory called `KeyPointsPass.so`. The `.so` extension is the extension on the VCL Ubuntu VM; the plugin should work on other platforms, but the extension may differ. You can copy this executable to wherever you wish.

Next, to generate an instrumented executable, you will need to locate the `keypoints/support/branchlog.c` file. This can also be moved to wherever you wish, but must be included in your list of compliation files. You must also ensure that none of the following files exist in the directory you are compiling in.

- branch_dictionary.txt
- counter.log

Once you have ensured that these files are not present, run the following command:
```
clang-15 -gdwarf-4 -fpass-plugin="{path to KPP.so}/KeyPointsPass.so" {path to branchlog}/branchlog.c {your files and any other arguments}
```

For example, if you wish to compile a file called `foo.c`, generate an executable called `foo`, and have both `KeyPointsPass.so` and `branchlog.c` in the pwd, you can run the command like so:
```
clang-15 -gdwarf-4 -fpass-plugin="KeyPointsPass.so" branchlog.c foo.c -o foo
```

You will end up with a `branch_dictionary.txt` file and your `foo` executable. There will also be a `counter.log` file that is simply an artifact of the compilation. At this point, you can run `foo` as you would any typical executable. The `branch_dictionary.txt` will contain the branch tags, their source file, line number, and the line number of each alternative. It may be best to use the `-O0` compiler flag to disable optimizations. Allowing optimization should not negatively affect the behavior of the plugin, but may result in branches being reorganized or eliminated.

You can also use the `keypoints/instrument.sh` script to generate your executable and avoid some of the potential cleanup listed here. To do this, pass the location of the plugin as the first argument and then all the source files, including the `branchlog.c` file. For example, if you have the plugin in a `plugins/` directory and your source file `foo.c` and `branchlog.c` in a `src/` directory, you would run the script like so:
```
./instrument.sh plugins/KeyPointsPass.so src/foo.c src/branchlog.c
```

However, this has several restrictions:

1. It cannot support additional compiler arguments. Only source code files.
2. It always runs with `-O0`.
3. The generated executable will always be the default `a.out`.

If this succeeds, you will get a file called `branch_dictionary.txt.{pid}` where `{pid}` is the pid of the script run. If it fails for whatever reason, you will see a directory called `tmp-${pid}`.

Further details on many of the requirements addressed briefly here are given in [section 4.1](#41-key-points).

## 3 Instruction Count
To run your executable and obtain an instruction count, simply run the `instrcnt/countinstrs.sh` and pass your executable and any arguments to it. For example, to run an exectuable called `foo` with an argument `bar.txt`, run the below command:
```
./couninstrs.sh foo bar.txt
``` 

You will see any standard output for the execution followed by a message and the total number of execcuted instructions. For example:
```
{program output here}

Program execution finished. Total number of executed instructions:
276,364 (100.0%)  PROGRAM TOTALS
```

You will also see a `callgrind-results.txt.{pid}` file where `{pid}` is the pid of the call to the script. This has more extensive information on the program execution that you may peruse. Its presence should not negatively impact future executions of the program as the pid qualifier should prevent collisions, and at worst, overwrite a previous run's file. You will also see an `out.log` file. This is the output of Callgrind itself and is written to a log file to keep it from being interleaved with the program's output. This may cause collisions between concurrent runs, but should not interfere in subsequent runs as it will simply be overwritten.

## 4 Implementation

### 4.1 Key Points
The key points pass is implemented as an LLVM pass that inspects the LLVM IR for branch, switch, and call instructions. When it finds a branch or switch instruction, it inserts a new instruction to call to a support function, `csc512project_log_branch` that accepts a string argument of the branch tag. This support function opens a file called `branch_trace.txt` for append, writes the branch tag to it, and closes the file. The pass also adds a record of the branch and the alternatives to the `branch_dictionary.txt`.

When it encounters a call instruction, it checks to see if the instruction is a direct call or an indirect call. If it is a direct call, it skips it. If it is indirect, this means that the call is to a function pointer. The pass inserts a new instruction to call another support function, `csc512project_log_fp`. This accepts a void pointer that is the function pointer. It opens the same `branch_trace.txt` file, writes the address of the function pointer, and then closes the file.

The file must be opened in append mode so that later writes do not overwrite previous ones due to the file being opened and closed in every support call. The decision to have the support calls open and close the file was made to simplify implementation of writing the file. This way, we can ensure that the file is always open when we attempt to write and ensure that we never leave it inappropriately open after writing. However, the need to open the file in append mode is also why you must ensure that no existing `branch_trace.txt` file exists. If there is one, it will result in the current execution's trace being appended to the previous one. The fact that `branch_trace.txt` is not qualified also means that concurrent runs may interfere with each other similarly. As such, you are advised to do these in isolation.

Additionally, the pass uses the `counter.log` file to support instrumenting multiple C source files. Since module passes cannot pass information between each other, an internal counter would result in all source files starting with branch 0 and progressing from there. This would undoubtedly cause confusion when reviewing a trace. The file contains the last value of the counter from the previous module pass so that subsequent ones can pick up at the appropriate spot. However, this does mean that if you instrument code without first deleting a previous run's `counter.log` file, your branch tags will begin after whatever the maximum branch of the previously instrumented code was.

Finally, the `branch_dictionary.txt` must also be opened in append mode to support multiple C source files. Since each module pass is run in isolation, if we were to open the file without append mode, each module would overwrite the previous one, leaving only the last analyzed module in the dictionary. This is obviously not ideal, but once again means that users must be judicious about deleting a previous run's `branch_dictionary.txt` file so that the most recent run's isn't erroneously appended.

#### 4.1.1 Improvements
Many of these issues could likely be improved or altogether removed through more knowledgable and judiciuos usage of LLVM; unfortunately, there was not time to implement this. Here we outline why these decisions were made and some of the potential improvements.

##### Support functions
As mentioned the support functions require users to include another C source file in their compilation and can potentially result in name collisions. If we were to insert the instructions to open, write, and close the file directly in the LLVM IR this would eliminate the need for the support functions. The reason this was not done is because it requires figuring out how to represent a `FILE *` in the function declarations. This may be a simple matter for someone more familiar with LLVM, but time did not permit for the necessary experimentation.

##### Run collisions
A number of files can cause collisions between runs if users are not judicious about cleanup and isolating executions. This is due to the inability to pass information between passes in LLVM. There are two possible solutions to this. 

First would be to figure out some way to provide a qualifier to the compilation. If we could do this, then all the generated files, including the insertions to open and close `branch_trace.txt` could be qualified with the input and so long as a user did not pass the same qualifier into multiple runs, no collisions would occur either between subsequent or concurrent runs.

Second is to move the pass to a different location. From a brief discussion with Dr. Shen, it seems that LLVM includes the ability to write link-time passes that can extend between modules. This would allow retaining information between the modules. However, this would also likely require a total rewrite of the pass and the infrastructure for the pass, which proved infeasible.

Given the issues with this, we opted to include the `instrument.sh` script which uses a working directory to skirt these issues. It is not ideal, but it should prove helpful for simpler situations.

### 4.2 Instruction Count
This section was significantly easier. All that is necessary is running the program with Valgrind's callgrind tool. This tool generates an output file that includes the total number of instructions along with a significant amount of data. From this point, all that is necessary to get the total count is to grep the file. This is essentially all the `countinstrs.sh` script does.

#### 4.2.1 Improvements
There are likely improvements that could be made to this section as well, such as added flexibility in output formatting or where to display the output, but given the scope of the project, most of these are rather inconsequential.

## Test Cases
A number of test cases are provided in the `test-files` directory. They are split into a few subdirectories.

### Simple
In the `simple` directory, there are a number of files. The vast majority of these are stand-alone and are intended to test the plugin behavior in isolation. Their names should give an idea of what C behavior they represent. 

The one exception to this is the three files `externalcall.c`, `externalfunc.c`, and `externalfunc.h`. These test the ability of the plugin to support programs split across multiple files and must be compiled together. As with typical C programs, you need only pass the two `.c` files to the compiler; the `.h` file is simply so the `externalcall.c` file can know about the function declaration.

Also note that `goto.c` will not generate a `branch_trace.txt`. This is expected as it does not contain any conditional branches, so no instrumentation was added and no file is opened or written while it runs. It will still generate an empty `branch_dictionary.txt`.

The below files have been tested and verified as working with the keypoints plugin and `countinstrs.sh` script. Testing was performed using the `instrument.sh` script, so while manually compiling the files should behave the same, I recommend using the script.

- dowhile.c
- externalcall.c 
- externalfunc.c 
- externalfunc.h 
- for.c 
- funcptr.c 
- goto.c 
- if.c 
- ifelse.c
- ifelseifelse.c 
- switch.c 
- switchdef.c
- while.c

The below files have not yet been verified.

- N/A

### Realworld
The `realworld` directory contains a couple source files that are taken from real-world programs. 

The `fmt` subdirectory contains a modified version of Apple's `fmt` command source code. The original can be found here: https://opensource.apple.com/source/text_cmds/text_cmds-106/fmt/fmt.c.auto.html. Modifications were made to ensure that it could successfully build on the VCL Ubuntu machines as well as to address a couple points the plugin could not handle within the bounds permitted by Dr. Shen.

The `mongoose` subdirectory contains an older, and also modified, version of the Mongoose C web server library. Further details on the project and the most recent version of the library can be found here: https://github.com/cesanta/mongoose. Modifications were made to address a couple points the plugin could not handle within the bounds permitted by Dr. Shen.

These have not yet been tested to ensure that they work with the compiler plugin.