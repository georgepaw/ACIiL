cmake_minimum_required(VERSION 3.8)

set(MAIN_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}  ) # --src-root
set(MAIN_INCLUDE_DIR ${MAIN_SRC_DIR}/include ) # --includedir

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(src)
