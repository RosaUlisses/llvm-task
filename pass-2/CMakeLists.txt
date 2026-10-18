
cmake_minimum_required(VERSION 3.20)

project(LLVMTask LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(SYSTEM
  ${LLVM_INCLUDE_DIRS}
  "${LLVM_BINARY_DIR}/include"
)

add_definitions(${LLVM_DEFINITIONS})

# Keep the legacy source outside the plugin target.
set(LLVM_OPTIONAL_SOURCES MyPasses.cpp)

add_llvm_pass_plugin(BasicBlockSimplification
  BasicBlockSimplification.cpp
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
