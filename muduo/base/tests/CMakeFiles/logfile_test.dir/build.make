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
include CMakeFiles/logfile_test.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/logfile_test.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/logfile_test.dir/flags.make

CMakeFiles/logfile_test.dir/LogFile_test.o: CMakeFiles/logfile_test.dir/flags.make
CMakeFiles/logfile_test.dir/LogFile_test.o: LogFile_test.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/long/muduo/muduo/muduo/base/tests/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/logfile_test.dir/LogFile_test.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/logfile_test.dir/LogFile_test.o -c /home/long/muduo/muduo/muduo/base/tests/LogFile_test.cc

CMakeFiles/logfile_test.dir/LogFile_test.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/logfile_test.dir/LogFile_test.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/long/muduo/muduo/muduo/base/tests/LogFile_test.cc > CMakeFiles/logfile_test.dir/LogFile_test.i

CMakeFiles/logfile_test.dir/LogFile_test.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/logfile_test.dir/LogFile_test.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/long/muduo/muduo/muduo/base/tests/LogFile_test.cc -o CMakeFiles/logfile_test.dir/LogFile_test.s

CMakeFiles/logfile_test.dir/LogFile_test.o.requires:

.PHONY : CMakeFiles/logfile_test.dir/LogFile_test.o.requires

CMakeFiles/logfile_test.dir/LogFile_test.o.provides: CMakeFiles/logfile_test.dir/LogFile_test.o.requires
	$(MAKE) -f CMakeFiles/logfile_test.dir/build.make CMakeFiles/logfile_test.dir/LogFile_test.o.provides.build
.PHONY : CMakeFiles/logfile_test.dir/LogFile_test.o.provides

CMakeFiles/logfile_test.dir/LogFile_test.o.provides.build: CMakeFiles/logfile_test.dir/LogFile_test.o


# Object files for target logfile_test
logfile_test_OBJECTS = \
"CMakeFiles/logfile_test.dir/LogFile_test.o"

# External object files for target logfile_test
logfile_test_EXTERNAL_OBJECTS =

logfile_test: CMakeFiles/logfile_test.dir/LogFile_test.o
logfile_test: CMakeFiles/logfile_test.dir/build.make
logfile_test: CMakeFiles/logfile_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/long/muduo/muduo/muduo/base/tests/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable logfile_test"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/logfile_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/logfile_test.dir/build: logfile_test

.PHONY : CMakeFiles/logfile_test.dir/build

CMakeFiles/logfile_test.dir/requires: CMakeFiles/logfile_test.dir/LogFile_test.o.requires

.PHONY : CMakeFiles/logfile_test.dir/requires

CMakeFiles/logfile_test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/logfile_test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/logfile_test.dir/clean

CMakeFiles/logfile_test.dir/depend:
	cd /home/long/muduo/muduo/muduo/base/tests && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests /home/long/muduo/muduo/muduo/base/tests/CMakeFiles/logfile_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/logfile_test.dir/depend

