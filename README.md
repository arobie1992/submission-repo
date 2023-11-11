# CSC 512 Project Fall 2023 part 1

## Running
First, build the plugin by running the `keypoints/buildplugin.sh` script.

Next, run all analysis through the `profile.sh` script. To do this, simply pass your C soure files as you would to Clang or GCC. For example, given source files foo.c and bar.c, you can run `profile.sh foo.c bar.c`. This compiles and executes the code and prints the analysis information to stdout and appropriate files.

## Requirements
This is intended to be run on the VCL Ubuntu 22.04 VMs and must have clang-17 install. You can do this by running `apt install llvm-toolchain-17`. The script should output any unmet requirements and terminate (relatively) gracefully.

## Details
This repository contains the code for part 1 of the project, which includes branch point detection and instruction count. The branch point detection is done by the code in the `keypoints` directory. The instruction count portion is done by the code in the `instrcnt` direcctory. 

The instruction count is the simpler of the two and just requires an installation of valgrind and grep. If you wish to run it individually, you can pass your compiled executable as an argument to the `countinstrs.sh` script, e.g., `./countinstrs.sh a.out`. This will output the final instruction count.

The keypoints detection is much more complicated and I recommend using the provided scripts. The first step is to build the plugin itself, which can be done by running `keypoints/buildplugin.sh`. If you wish to run the build the plugin without using this script, you may refer to the contents of the script for individual steps.

Once the plugin is built, next you need to run Clang with the plugin. Since this is intended to be run on the VCL VMs, I've added the requirement that you must be using Clang 17. I have tested with this one; others may work, but I can't make any guarantees. Similarly, I strongly recommend running this with the `profile.sh` script. Do note that this script also runs the instruction count at the end. If you wish to run clang separately, you will need to make sure you clean up some of the external files to prevent cross-run contamination. I will return to what these are later.

Upon successful run of the plugin you should end up with two files `./branch_dictionary.txt.{pid}` and `./branch_trace.txt.{pid}` where `{pid}` is the pid of the execution of `profile.sh`. It keeps individual runs from cross-contaminating by creating a working directory qualified with the pid and performing all work in that directory. Once the run succeeds it deletes this directory after moving any necessary files out of it. If the run fails, it does not clean this up to help with diagnostics so you will need to delete this yourself. Additionally, if you call `profile.sh` from a directory where you do not have permission to create subdirectories, the run will fail as a result of this setup. The need for this directory is why I strongly recommend using the `profile.sh` script for all executions. It is very fragile and results in flexibility issues, but was necessary to achieve certain goals. 

The first was the ability to handle multiple source files properly. It does this by writing a temp file that tracks the last value for a branch counter. If the file is not present, such as on the first module, it uses a default value; otherwise, it resumes the branch counting where it left off. This is definitely not ideal, but I was unable to figure out a way to carry state across modules in another fashion while staying in this particular LLVM pipeline. Ideally, I would find some better way of performing analysis and instrumentation across modules purely within LLVM. One way to do this might be to pass a parameter in to LLVM from the command line, but I was unable to figure out how to do this with just the clang workflow.

The second issue was also related to multimodule support, specifically writing the branch dictionary. Modules after the first must append their contents to the branch dictionary rather than writing over it. Additionally, since I could not pass information between modules, I could not qualify the name within LLVM. This meant that I needed some stable name for the file. But since the file needed to have a stable name, that meant that if users did not delete the file between runs, it would end up appending the branch dictionary to the previous run's dictionary. Generating this file within the directory allowed for a stable name, and copying it out allowed me to qualify it so subsequent runs didn't accidentally append to the wrong dictionary file. 

A similar issue arose with the `countinstrs.sh` script. Valgrind could output a qualified file, but it became tricky to find the qualifier for this file in a stable way. Alternate approaches, like having an unqalified name or find the qualifier by checking the latest generated file with a certain prefix were prone to collisions between runs, such as two concurrent runs of the tool.

The final issue was having consistent naming across the dictionary and trace. Since LLVM ran as a different process than valgrind, and additionally since `profile.sh` ran in a different process than `countinstrs.sh`, I could not just qualify it with the pid of each process. Another approach, such as having `profile.sh` pass its pid as an argument to `countinstrs.sh` would have worked, but since the other issues made the temp working dir an appealing alternative, it was easy enough to just rename them during the copy out.

This approach does have many flaws, such as the aforementioned fragility, and other others such as potential directory name collisions, but it was the best approach I was able to design and implement in the time available.