cmake_minimum_required (VERSION 3.2)
set (CMAKE_CXX_STANDARD 17)

project (ypopl)
file(GLOB_RECURSE SRC
    src/*.cc
)
file(GLOB_RECURSE HDR
    inc/*.h
)
include_directories(inc)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

find_path(LIBYAGLL yagll.h HINTS ../yagll REQUIRED)
find_library(LIBYAGLL_LIBRARY NAMES yagll HINTS ../yagll REQUIRED)
include_directories(${LIBYAGLL} ${LIBYAGLL}/inc)

llvm_map_components_to_libnames(llvm_libs core)
add_executable(yopl ${SRC})
target_link_libraries(yopl ${llvm_libs})
