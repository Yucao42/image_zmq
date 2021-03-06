cmake_minimum_required(VERSION 2.8)

project(zmq_image C CXX)
option(SPAWN_PROCESSES "Test using fork, otherwise using threads" ON)
option(DO_VIDEO_TEST "Test video transmission" ON)
option(DO_LABEL_TEST "Test classification result transmission" OFF)
find_package(OpenCV)
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
include_directories(
   "./"
   "${CMAKE_CURRENT_BINARY_DIR}"
   "$ENV{LOCAL}/usr/local/include"
   "${OpenCV_INCLUDE_DIRS}"
   )
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fPIC -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized -Wno-sign-compare -Wno-pointer-arith -g -O3 -Wall -Wextra")
  set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -DFINTEGER=int  -fopenmp -g ")
else()
  add_definitions(-D_WIN32_WINNT=0x600)
endif()

message(STATUS "ZMQ FLAGS ${ZMQFLAGS}")
add_compile_options(${ZMQFLAGS})
if (DO_VIDEO_TEST)
  add_definitions(-DTEST_VIDEO)
endif()

if (SPAWN_PROCESSES)
  add_definitions(-DSPAWN_PROCESSES)
endif()

if (DO_LABEL_TEST)
  add_definitions(-DTEST_LABEL)
endif()

# Protobuf
set(CMAKE_PREFIX_PATH $ENV{LOCAL}/usr/local)
find_package(Protobuf CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/image.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/image.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/image.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/image.grpc.pb.h")

# GRPC
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
get_filename_component(hw_proto "./image.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")


# compile zmq database
add_executable(zmq_server "${hw_proto_srcs}" zmq_server.cpp )
target_link_libraries(zmq_server zmq ${OpenCV_LIBS} ${_PROTOBUF_LIBPROTOBUF})      
add_executable(zmq_client "${hw_proto_srcs}" zmq_client.cpp )
target_link_libraries(zmq_client zmq ${OpenCV_LIBS} ${_PROTOBUF_LIBPROTOBUF})     
add_executable(test_service test_service.cpp "${hw_proto_srcs}")
target_link_libraries(test_service zmq ${OpenCV_LIBS} ${_PROTOBUF_LIBPROTOBUF})     
add_executable(grpc_streamer grpc_streamer.cpp "${hw_proto_srcs}" "${hw_grpc_srcs}")
target_link_libraries(grpc_streamer zmq ${OpenCV_LIBS} ${_PROTOBUF_LIBPROTOBUF} ${_GRPC_GRPCPP_UNSECURE})     
    
