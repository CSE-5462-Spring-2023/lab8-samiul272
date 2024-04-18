cmake_minimum_required(VERSION 3.10) # Adjust minimum version as necessary
project(lab5)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")

# Add the executable
add_executable(drone3 drone3.cpp Message.cpp Utility.cpp)

# If you have header files that need to be included in other directories, use include_directories()
# For this setup, it seems all files are in the same directory, so it's not used here.

# Optionally, you can define custom commands for cleaning or other purposes
# However, CMake's built-in "make clean" usually suffices for cleaning up build artifacts
