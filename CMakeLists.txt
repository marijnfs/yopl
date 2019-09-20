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


find_path(LIBYAGLL yagll.h HINTS ../yagll REQUIRED)
find_library(LIBYAGLL_LIBRARY NAMES yagll HINTS ../yagll REQUIRED)
include_directories(${LIBYAGLL} ${LIBYAGLL}/inc)

add_executable(yopl ${SRC})

