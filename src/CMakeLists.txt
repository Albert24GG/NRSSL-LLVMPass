cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)

project(NRSConversionPass LANGUAGES CXX C)

SET(CMAKE_CXX_FLAGS "-Wall -fno-rtti -fPIC")

SET(NRSSL_PATH "${CMAKE_SOURCE_DIR}/jars")

# Find LLVM

set(LLVM_DIR /racheta/llvm-project/build/lib/cmake/llvm)
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Find JNI
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Add the LLVM pass
add_library(NRSConversionPass MODULE NRSConversionPass.cpp NRSSL.cpp)

# Generate LLVM IR from C file
add_custom_command(
    OUTPUT sample.ll
    COMMAND ${CMAKE_C_COMPILER} -S -emit-llvm ${CMAKE_SOURCE_DIR}/test/sample.c -o sample.ll
    DEPENDS ${CMAKE_SOURCE_DIR}/test/sample.c
    COMMENT "Generating LLVM IR from sample.c"
)

# Run LLVM pass on sample.ll
add_custom_command(
    OUTPUT processed.ll
    COMMAND "NRSSL_JARS=${NRSSL_PATH}"
            "LD_PRELOAD=$ENV{JAVA_HOME}/lib/server/libjvm.so"
            "${LLVM_TOOLS_BINARY_DIR}/opt"
            "-load-pass-plugin=${CMAKE_CURRENT_BINARY_DIR}/libNRSConversionPass.so"
            "-passes=ieee-to-posit"
            "-S" "sample.ll"
            "-o" "processed.ll"
    DEPENDS sample.ll NRSConversionPass
    COMMENT "Running LLVM pass on sample.ll"
)

# Compile processed.ll to an object file
add_custom_command(
    OUTPUT processed.o
    COMMAND llc -filetype=obj --relocation-model=pic processed.ll -o processed.o
    DEPENDS processed.ll
    COMMENT "Compiling processed.ll to object file"
)

# Create the final executable
add_executable(TestPass TestPass.cpp NRSSL.cpp processed.o)

set_target_properties(TestPass PROPERTIES COMPILE_FLAGS "-fPIE")
set_target_properties(TestPass PROPERTIES LINK_FLAGS "-fPIE -pie")

target_link_libraries(TestPass ${JNI_LIBRARIES})
