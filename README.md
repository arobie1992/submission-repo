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

The `-gdwarf-4` flag is necessary to get debug information to retrieve information such as line numbers of the conditions and alternatives. You may try using the `-g` flag, but there is a chance this will fail. During testing, I ran into an issue with architecture compatibility and had to use the `-gdwarf-4` flag instead. Since then any tests have exclusively used the `-gdwarf-4` flag which has worked.

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
The key points pass is implemented as an LLVM pass that inspects the LLVM IR for branch, switch, and call instructions. When it finds a branch or switch instruction, it inserts a new instruction to call to a support function, `csc512project_log_branch` that accepts an integer argument of the branch ID. This support function opens a file called `branch_trace.txt` for append, writes a branch tag with the ID to it, and closes the file. The pass also adds a record of the branch and the alternatives to the `branch_dictionary.txt`.

When it encounters a call instruction, it checks to see if the instruction is a direct call or an indirect call. If it is a direct call, it skips it. If it is indirect, this means that the call is to a function pointer. The pass inserts a new instruction to call another support function, `csc512project_log_fp`. This accepts a void pointer that is the function pointer. It opens the same `branch_trace.txt` file, writes the address of the function pointer, and then closes the file.

The file must be opened in append mode so that later writes do not overwrite previous ones due to the file being opened and closed in every support call. The decision to have the support calls open and close the file was made to simplify implementation of writing the file. This way, we can ensure that the file is always open when we attempt to write and ensure that we never leave it inappropriately open after writing. However, the need to open the file in append mode is also why you must ensure that no existing `branch_trace.txt` file exists. If there is one, it will result in the current execution's trace being appended to the previous one. The fact that `branch_trace.txt` is not qualified also means that concurrent runs may interfere with each other similarly. As such, you are advised to do these in isolation.

Additionally, the pass uses the `counter.log` file to support instrumenting multiple C source files. Since module passes cannot pass information between each other, an internal counter would result in all source files starting with branch 0 and progressing from there. This would undoubtedly cause confusion when reviewing a trace. The file contains the last value of the counter from the previous module pass so that subsequent ones can pick up at the appropriate spot. However, this does mean that if you instrument code without first deleting a previous run's `counter.log` file, your branch tags will begin after whatever the maximum branch of the previously instrumented code was.

Finally, the `branch_dictionary.txt` must also be opened in append mode to support multiple C source files. Since each module pass is run in isolation, if we were to open the file without append mode, each module would overwrite the previous one, leaving only the last analyzed module in the dictionary. This is obviously not ideal, but once again means that users must be judicious about deleting a previous run's `branch_dictionary.txt` file so that the most recent run's isn't erroneously appended.

#### 4.1.1 Improvements
Many of these issues could likely be improved or altogether removed through more knowledgable and judiciuos usage of LLVM; unfortunately, there was not time to implement this. Here we outline why these decisions were made and some of the potential improvements.

##### 4.1.1.1 Support functions
As mentioned the support functions require users to include another C source file in their compilation and can potentially result in name collisions. If we were to insert the instructions to open, write, and close the file directly in the LLVM IR this would eliminate the need for the support functions. The reason this was not done is because it requires figuring out how to represent a `FILE *` in the function declarations. This may be a simple matter for someone more familiar with LLVM, but time did not permit for the necessary experimentation.

##### 4.1.1.2 Run collisions
A number of files can cause collisions between runs if users are not judicious about cleanup and isolating executions. This is due to the inability to pass information between passes in LLVM. There are two possible solutions to this. 

First would be to figure out some way to provide a qualifier to the compilation. If we could do this, then all the generated files, including the insertions to open and close `branch_trace.txt` could be qualified with the input and so long as a user did not pass the same qualifier into multiple runs, no collisions would occur either between subsequent or concurrent runs.

Second is to move the pass to a different location. From a brief discussion with Dr. Shen, it seems that LLVM includes the ability to write link-time passes that can extend between modules. This would allow retaining information between the modules. However, this would also likely require a total rewrite of the pass and the infrastructure for the pass, which proved infeasible.

Given the issues with this, we opted to include the `instrument.sh` script which uses a working directory to skirt these issues. It is not ideal, but it should prove helpful for simpler situations.

##### 4.1.1.3 Unsupported constructs
Currently, there appears to be some bugs surrounding logical combination operators (`&&` and `||`) in while and for loop conditions and return statements. For example:
```
while (n+word_length < length && line[n+word_length] != ' ')
  ++word_length;
```

LLVM represents this as the typical while loop construct of conditional and unconditional branch instructions. But additionally, it represents the `&&` operator's short-circuting behavior as an additional set of conditional branch instructions. You can think of it like this:
```
while(
    if(n+word_length < length == false) {
        yeild false
    } else {
        yield line[n+word_length] != ' '
    }
) {
    ++word_length;
}
```

When inspecting the LLVM IR, the code appears to be correctly instrumented, inserting branch logs into the if and else branches in addition to the normal while constructs. However, when the compiler moves on to machine code generation, it fails due to a segfault. I was not able to track down the root cause for this. The LLVM error report was not particularly helpful, likely due to my installation not being built with debug symbols enabled. I spoke with Dr. Shen and he said that it was okay to modify the code to avoid this construction. As such, the above example was modified to the logically equivalent:
```
while (1) {
  if (n+word_length >= length) {
    break;
  }
  if (line[n+word_length] == ' ') {
    break;
  }
  ++word_length;
}
```

A similar issue seems to arise with combination operators in assignments, `int x = a || b`. Oddly, this doesn't seem to happen in if statements. I can't say whether it occurs in switch statements.

##### 4.1.1.4 Slow execution
Depending on the number of branches and how often they are hit, the instrumented programs can take orders of magnitude longer to execute than their uninstrumented versions. This is likely due to the extra writes and specifically the decision to have the log functions open and close the file every time they log a branch execution. Some slowdown is inescapable due to the extra work the instrumented code must do, but the major bottleneck of the file operations can likely be improved. Since the support functions open and close the file, every single branch log must perform the necessary syscalls to get access to the file and flush the write buffer which means they do not gain any benefit of typical buffered I/O performance improvements. A couple different approaches, discussed next, could be taken to mitigate this issue. 

The first would be to open the file at the start of the main method and then close it just before the main function returns. However, this drastically increases the complexity of the plugin as it must also check for the main function, find all possible exit points, and properly add the close file instruction to all of them. The complexity of this risks either attempting to close an already closed file or not properly closing the file, both of which may result in breaking the instrumented program or other unexpected outcomes.

The second would be to keep an internal buffer, likely an array, of tags that were hit, and then upon program termination, printing all of them to the file. This has the similar issue of complexity and finding all the exit points of the program. The risks are slightly different though. First, this would increase the memory footprint of the program by needing to store all the branch IDs. Second, the support functions would need to implement the appropriate array allocation and growth behaviors, leaving more possible room for errors. Third, incorrect instrumentation of the exit points could result in the branch trace not being printed at all or even being printed multiple times in full.

##### 4.1.1.5 Module name in instrument.sh
LLVM uses the fully qualified file name of the input C files as the module. This includes things like relative path. Because the `instrument.sh` script creates a working directory, it must update the file paths slightly to include `../` prefixed to all the file names. This results in the branch dictionary having slightly different names than the files passed in. While this is not major, it is an area for improvement. This could be done by having some flag to the plugin to tell it to trim these prefixes.

##### 4.1.1.6 Handling file failures
Writing the branch trace requires opening and closing the file. These operations can fail, but at the moment the plugin does not account for this situation. It does this so as to not add additional branches that may erroneously be picked up when running the passes. This however, is not ideal, and could be addressed by having the pass detect what is an inserted branch versus what is a branch from the source code. One way to do this would be checking the module name. There may be some other ways to do this, such as ones provided through LLVM, but we were not able to find a means to do so within the timeline of the project.

### 4.2 Instruction Count
This section was significantly easier. All that is necessary is running the program with Valgrind's callgrind tool. This tool generates an output file that includes the total number of instructions along with a significant amount of data. From this point, all that is necessary to get the total count is to grep the file. This is essentially all the `countinstrs.sh` script does.

#### 4.2.1 Improvements
The improvements to the instruction count are far less extensive as the tool was much simpler and had fewer areas for tricky configuration.

##### 4.2.1.1 Run collisions
While the instruction counting portion of part 1 is not as susceptible to run collisions as the instrumentation portion, it still does run into some risks with the Callgrind report file and the file to which all the output is logged. These could be addressed by adding the pid as qualifier to the file names as defined in the shell script. This would serve a similar purpose to how the instrument.sh script makes use of the temp working directory and how Callgrind generates the default report file name—in fact, this is where we drew the idea for the temp directory from. The reason we could not use the default name generation was because it made it difficult to discover the report file. However, if we were to qualify the file name in `countinstrs.sh`, it would make it unique while also allowing us to easily discover it. The only reason this is not implemented is due to time constraints and responsibilities with other classes.

## 5 Test Cases
A number of test cases are provided in the `test-files` directory. They are split into a few subdirectories.

### 5.1 Simple
In the `simple` directory, there are a number of files. The vast majority of these are stand-alone and are intended to test the plugin behavior in isolation. Their names should give an idea of what C behavior they represent. 

The one exception to this is the three files `externalcall.c`, `externalfunc.c`, and `externalfunc.h`. These test the ability of the plugin to support programs split across multiple files and must be compiled together. As with typical C programs, you need only pass the two `.c` files to the compiler; the `.h` file is simply so the `externalcall.c` file can know about the function declaration.

The `funcpointer.c` file will result in an empty `branch_dictionary.txt`, but will still result in a branch trace. This is expected as there are no branches in the code to be written to the dictionary, but it still involves a call to a function pointer, which writes its value to `branch_trace.txt`.

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

### 5.2 Realworld
The `realworld` directory contains a source file from a real-world program. Details are provided below.

#### 5.2.1 fmt
The `fmt` subdirectory contains a modified version of Apple's `fmt` command source code. The original can be found here: https://opensource.apple.com/source/text_cmds/text_cmds-106/fmt/fmt.c.auto.html. Modifications were made to ensure that it could successfully build on the VCL Ubuntu machines as well as to address a couple points the plugin could not handle within the bounds permitted by Dr. Shen. The primary change was to address logical combination operations in while loops and return statements. See section [section 4.1.1.3](#4113-unsupported-constructs) for more information on why this change was necessary.

The `fmt.c` file has been tested and will successfully compile when compiled with the plugin with the plugin. The generated executable has also been tested and behaves equivalently. However, I highly recommend using a small file to test it; the instrumented version is painfully slow. See [section 4.1.1.4](#4114-slow-execution) for some discussion on this.
