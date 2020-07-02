cmake_minimum_required(VERSION 2.8)

project(zmq_image C CXX)
 
option(DO_VIDEO_TEST "Test video transmission" OFF)
option(DO_LABEL_TEST "Test classification result transmission" ON)
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
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fPIC -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized -Wno-sign-compare -Wno-pointer-arith -g -O3 -Wall -Wextra")
  set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DFINTEGER=int  -fopenmp -g ")
else()
  add_definitions(-D_WIN32_WINNT=0x600)
endif()

add_compile_options(${ZMQFLAGS})
if (DO_VIDEO_TEST)
  add_definitions(-DTEST_VIDEO)
endif()

if (DO_LABEL_TEST)
  add_definitions(-DTEST_LABEL)
endif()

# compile zmq database
add_executable(zmq_server zmq_server.cpp)
target_link_libraries(zmq_server zmq ${OpenCV_LIBS})     
add_executable(zmq_client zmq_client.cpp)
target_link_libraries(zmq_client zmq ${OpenCV_LIBS})     
