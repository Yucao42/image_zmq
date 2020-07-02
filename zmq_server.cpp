#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "data_struct.hpp"
#include "zmq_service.hpp"

int main(int argc, char** argv) {

  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;

  std::string addr("tcp://*:50051");
  ZmqServer server(addr);

#ifdef TEST_LABEL
  ClassificationData label("Car", 0.88);
  if (server.initial_connection()) {
    std::cout << "Server connection built" << std::endl;
    server.send(&label);
    server.end_connection();
  }
#endif

#ifdef TEST_VIDEO
  // Prepare data
  ImageData img;
  img.set_resize(size_small);
  cv::VideoCapture cap("../sample_1080p_h264.mp4");

  bool read_success = img.read_video(cap);

  if (server.initial_connection()) {
    std::cout << "Server connection built" << std::endl;
    while (read_success) {
      server.send(&img);
      read_success = img.read_video(cap);
    }
    server.end_connection();
  }
  cap.release();
#endif
  return 0;
}
