#ifndef DATA_STRUCT_HPP
#define DATA_STRUCT_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "timer.h"
#include "image.pb.h"

#define HEADER_OFFSET (sizeof(long))


using image_proto::FrameData;

/////////////////////////////////////////////////////
////Data structure compatible with ZMQ
/////////////////////////////////////////////////////
struct Data {
  Data()=default;

  Data(size_t ds):data_size(ds) {}

  // Transfer data to ZMQ message for transimission
  virtual bool to_bytes(void* buffer)=0;

  // Transfer ZMQ message to data
  virtual bool from_bytes(void* buffer, size_t size)=0;

  size_t data_size=0;
};

struct ImageData : public Data {
 public:
  ImageData() = default;

  ImageData(cv::Size size, int data_type) {
    frame = cv::Mat(size, data_type);
    data_size = frame.total() * frame.channels();
  }

  virtual bool to_bytes(void* buffer) {
    if (data_size == 0)
      return false;
    
#ifdef PROTOBUF_SERIALIZE
    // TODO
    // auto image = framedata.mutable_image();
#else
    memcpy(buffer, (void*)(frame.data), data_size);
#endif
    return true;
  }

  virtual bool from_bytes(void* buffer, size_t size) {
    // Expect constant input image
    if (data_size != size)
      return false;
#ifdef PROTOBUF_SERIALIZE
    // TODO
    // auto image = framedata.mutable_image();
#else
    memcpy((void*)(frame.data), buffer, size);
#endif
    return true;
  }

  // Set the resize shape
  void set_resize(cv::Size size, int num_channels=3) {
    target_size = size;
    data_size = size.height * size.width * num_channels;
    do_resize = true;
  }

  // Read a frame from a video 
  bool read_video(cv::VideoCapture& cap) {
    bool ret = cap.read(frame);
    if (ret) {
      if (do_resize)
        cv::resize(frame, frame, target_size);
      else
        data_size = frame.total() * frame.channels();
    }
    return ret;
  }

  cv::Mat frame;
  bool do_resize=false;
  cv::Size target_size;
  bool protobuf_serialization=true;
  std::string serial_data;
  FrameData framedata;
};

struct ClassificationData : public Data {
 public:
  ClassificationData() = default;

  ClassificationData(const std::string& l, float s):
      label(l), score(s) {
        data_size = sizeof(float) + l.size();
      }

  virtual bool to_bytes(void* buffer) {
    if (data_size == 0)
      return false;
    memcpy(buffer, (void*)(&score), sizeof(float));
    memcpy((char*)(buffer) + sizeof(float), (void*)(label.data()), label.size());
    return true;
  }

  virtual bool from_bytes(void* buffer, size_t size) {
    if (size < sizeof(float))
      return false;
    memcpy((void*)(&score), buffer, sizeof(float));
    size_t string_size = size - sizeof(float);
    label.resize(string_size);
    memcpy((void*)(label.data()), (char*)(buffer) + sizeof(float), string_size);
    return true;
  }

  // Label with maximum score
  std::string label;
  
  float score;
};

#endif
