cmake_minimum_required (VERSION 3.2)
set (CMAKE_CXX_STANDARD 20)

project (ypopl)
file(GLOB_RECURSE SRC
    src/*.cc
)
file(GLOB_RECURSE HDR
    inc/*.h
)
include_directories(inc)

#find_package(LLVM REQUIRED CONFIG)
#list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(HandleLLVMOptions) # load additional config
#include(AddLLVM) # used to add our own modules

#include_directories(${LLVM_INCLUDE_DIRS})
#add_definitions(${LLVM_DEFINITIONS})
#llvm_map_components_to_libnames(llvm_libs core)
#message()

find_path(LIBYAGLL yagll.h HINTS ../yagll REQUIRED)
find_library(LIBYAGLL_LIBRARY NAMES yagll HINTS ../yagll PATH_SUFFIXES build REQUIRED)
include_directories(${LIBYAGLL} ${LIBYAGLL}/inc)

add_executable(yopl ${SRC})
target_link_libraries(yopl LLVM)
