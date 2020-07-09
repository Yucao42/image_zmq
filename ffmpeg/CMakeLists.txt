cmake_minimum_required(VERSION 2.8)

project(ffmpeg_image C CXX)
 
option(DO_SHM_TEST "Test shared memory transmission" OFF)

find_package(OpenCV)
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
include_directories(
   "./"
   "${OpenCV_INCLUDE_DIRS}"
   )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fPIC -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized -Wno-sign-compare -Wno-pointer-arith -g -O3 -Wall -Wextra")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DFINTEGER=int  -fopenmp -g ")

if (DO_SHM_TEST)
  add_definitions(-DTEST_SHM)
endif()

# compile zmq database
add_executable(ffmpeg_image ffmpeg_image.cpp)
target_link_libraries(ffmpeg_image ${OpenCV_LIBS})     
