# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/long/muduo/muduo/muduo/base/tests

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/long/muduo/muduo/muduo/base/tests

# Include any dependencies generated for this target.
include CMakeFiles/processinfo_test.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/processinfo_test.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/processinfo_test.dir/flags.make

CMakeFiles/processinfo_test.dir/ProcessInfo_test.o: CMakeFiles/processinfo_test.dir/flags.make
CMakeFiles/processinfo_test.dir/ProcessInfo_test.o: ProcessInfo_test.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/long/muduo/muduo/muduo/base/tests/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/processinfo_test.dir/ProcessInfo_test.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/processinfo_test.dir/ProcessInfo_test.o -c /home/long/muduo/muduo/muduo/base/tests/ProcessInfo_test.cc

CMakeFiles/processinfo_test.dir/ProcessInfo_test.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/processinfo_test.dir/ProcessInfo_test.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/long/muduo/muduo/muduo/base/tests/ProcessInfo_test.cc > CMakeFiles/processinfo_test.dir/ProcessInfo_test.i

CMakeFiles/processinfo_test.dir/ProcessInfo_test.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/processinfo_test.dir/ProcessInfo_test.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/long/muduo/muduo/muduo/base/tests/ProcessInfo_test.cc -o CMakeFiles/processinfo_test.dir/ProcessInfo_test.s

CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.requires:

.PHONY : CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.requires

CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.provides: CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.requires
	$(MAKE) -f CMakeFiles/processinfo_test.dir/build.make CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.provides.build
.PHONY : CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.provides

CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.provides.build: CMakeFiles/processinfo_test.dir/ProcessInfo_test.o


# Object files for target processinfo_test
processinfo_test_OBJECTS = \
"CMakeFiles/processinfo_test.dir/ProcessInfo_test.o"

# External object files for target processinfo_test
processinfo_test_EXTERNAL_OBJECTS =

processinfo_test: CMakeFiles/processinfo_test.dir/ProcessInfo_test.o
processinfo_test: CMakeFiles/processinfo_test.dir/build.make
processinfo_test: CMakeFiles/processinfo_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/long/muduo/muduo/muduo/base/tests/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable processinfo_test"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/processinfo_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/processinfo_test.dir/build: processinfo_test

.PHONY : CMakeFiles/processinfo_test.dir/build

CMakeFiles/processinfo_test.dir/requires: CMakeFiles/processinfo_test.dir/ProcessInfo_test.o.requires

.PHONY : CMakeFiles/processinfo_test.dir/requires

CMakeFiles/processinfo_test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/processinfo_test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/processinfo_test.dir/clean

CMakeFiles/processinfo_test.dir/depend:
	cd /home/long/muduo/muduo/muduo/base/tests && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests/CMakeFiles/processinfo_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/processinfo_test.dir/depend
