# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.1

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
CMAKE_SOURCE_DIR = /home/noob/rtaudio

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/noob/rtaudio/build

# Include any dependencies generated for this target.
include tests/CMakeFiles/record.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/record.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/record.dir/flags.make

tests/CMakeFiles/record.dir/record.cpp.o: tests/CMakeFiles/record.dir/flags.make
tests/CMakeFiles/record.dir/record.cpp.o: ../tests/record.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/noob/rtaudio/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object tests/CMakeFiles/record.dir/record.cpp.o"
	cd /home/noob/rtaudio/build/tests && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/record.dir/record.cpp.o -c /home/noob/rtaudio/tests/record.cpp

tests/CMakeFiles/record.dir/record.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/record.dir/record.cpp.i"
	cd /home/noob/rtaudio/build/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/noob/rtaudio/tests/record.cpp > CMakeFiles/record.dir/record.cpp.i

tests/CMakeFiles/record.dir/record.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/record.dir/record.cpp.s"
	cd /home/noob/rtaudio/build/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/noob/rtaudio/tests/record.cpp -o CMakeFiles/record.dir/record.cpp.s

tests/CMakeFiles/record.dir/record.cpp.o.requires:
.PHONY : tests/CMakeFiles/record.dir/record.cpp.o.requires

tests/CMakeFiles/record.dir/record.cpp.o.provides: tests/CMakeFiles/record.dir/record.cpp.o.requires
	$(MAKE) -f tests/CMakeFiles/record.dir/build.make tests/CMakeFiles/record.dir/record.cpp.o.provides.build
.PHONY : tests/CMakeFiles/record.dir/record.cpp.o.provides

tests/CMakeFiles/record.dir/record.cpp.o.provides.build: tests/CMakeFiles/record.dir/record.cpp.o

# Object files for target record
record_OBJECTS = \
"CMakeFiles/record.dir/record.cpp.o"

# External object files for target record
record_EXTERNAL_OBJECTS =

tests/record: tests/CMakeFiles/record.dir/record.cpp.o
tests/record: tests/CMakeFiles/record.dir/build.make
tests/record: librtaudio_static.a
tests/record: /usr/lib64/libasound.so
tests/record: tests/CMakeFiles/record.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable record"
	cd /home/noob/rtaudio/build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/record.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/record.dir/build: tests/record
.PHONY : tests/CMakeFiles/record.dir/build

tests/CMakeFiles/record.dir/requires: tests/CMakeFiles/record.dir/record.cpp.o.requires
.PHONY : tests/CMakeFiles/record.dir/requires

tests/CMakeFiles/record.dir/clean:
	cd /home/noob/rtaudio/build/tests && $(CMAKE_COMMAND) -P CMakeFiles/record.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/record.dir/clean

tests/CMakeFiles/record.dir/depend:
	cd /home/noob/rtaudio/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/noob/rtaudio /home/noob/rtaudio/tests /home/noob/rtaudio/build /home/noob/rtaudio/build/tests /home/noob/rtaudio/build/tests/CMakeFiles/record.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/record.dir/depend

