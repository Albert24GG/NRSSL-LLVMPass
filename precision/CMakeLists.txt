cmake_minimum_required(VERSION 3.28)

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/riscv64-clang.cmake")
endif()

project(caial C ASM)

set(CAIAL_FLOAT_TYPES ieee posit1 posit2 morris morrisHeb morrisUnaryHeb morrisBiasHeb)
set(CAIAL_FLOAT_TYPE "ieee" CACHE STRING "Float representation: ieee or one of the NRS pass types")
set_property(CACHE CAIAL_FLOAT_TYPE PROPERTY STRINGS ${CAIAL_FLOAT_TYPES})
if(NOT CAIAL_FLOAT_TYPE IN_LIST CAIAL_FLOAT_TYPES)
  message(FATAL_ERROR "CAIAL_FLOAT_TYPE must be one of: ${CAIAL_FLOAT_TYPES}")
endif()

set(CAIAL_OPT_LEVEL "2" CACHE STRING "Optimization level (0-3) for the test sources")
set(CAIAL_FP_BITS "32" CACHE STRING "Width of fp_t in the tests: 32 (float) or 64 (double)")
set_property(CACHE CAIAL_FP_BITS PROPERTY STRINGS 32 64)
if(NOT CAIAL_FLOAT_TYPE STREQUAL "ieee" AND NOT CAIAL_FP_BITS STREQUAL "32")
  message(FATAL_ERROR "the NRS pass only supports 32-bit floats")
endif()
set(CAIAL_TEST_TIMEOUT "300" CACHE STRING "Seconds before a test run on the emulator is killed")
option(CAIAL_TRACE "Print per-iteration values (CAIAL_TRACE_VALUE) in the tests" OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The pass and the NRSSL jars come from the build of the parent project.
set(PASS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build" CACHE PATH "Build directory of the NRS pass")
set(NRS_PASS_PLUGIN "${PASS_BUILD_DIR}/src/libNRSConversionPass.so" CACHE FILEPATH "NRS conversion pass plugin")
set(NRSSL_JARS "${PASS_BUILD_DIR}/jars" CACHE PATH "Directory with nrssl.jar and the Scala library")

set(ROCKET_CONFIG "DefaultConfig" CACHE STRING "rocket-chip emulator config")
set(EMULATOR "/workspace/shared/rocket-chip/out/emulator/freechips.rocketchip.system.TestHarness/freechips.rocketchip.system.${ROCKET_CONFIG}/verilator/elf.dest/emulator"
    CACHE FILEPATH "Verilator emulator binary")

set(OPT "${LLVM_BIN_DIR}/opt")
set(LLC "${LLVM_BIN_DIR}/llc")

include(cmake/caial.cmake)
enable_testing()

add_subdirectory(runtime)

caial_add_test(e tests/e.c)
caial_add_test(pi tests/pi.c)
caial_add_test(series tests/series.c)
caial_add_test(sequences tests/sequences.c)
caial_add_test(logistic tests/logistic.c)
caial_add_test(summation tests/summation.c)
caial_add_test(horner tests/horner.c)
caial_add_test(matmul tests/matmul.c)
caial_add_test(ops tests/ops.c)
caial_add_test(knn tests/knn.c)
caial_add_test(kmeans tests/kmeans.c)
caial_add_test(linreg tests/linreg.c)
caial_add_test(nb tests/nb.c)
caial_add_test(dnn tests/dnn.c)
caial_add_test(svm tests/svm.c)

caial_add_gold_target()
