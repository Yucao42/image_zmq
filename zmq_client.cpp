#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "zmq_service.hpp"

int main(int argc, char** argv) {
  std::string addr("tcp://localhost:50051");

  char str[30];
  ZmqClient client(addr);

  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;
  ImageData img(size_small, CV_8UC3);
  TimerMicroSecconds timer;

  int frame_count = 0;
  bool continue_=true;
  if (client.initial_connection())
    while (continue_) {
      std::cout << "Server connection built" << std::endl;
      // Client receive image
      continue_ = client.recv(&img);
      if (1 && ++frame_count < 20) {
         sprintf(str, "debug/img_%d.jpg", frame_count);
         cv::imwrite(str, img.frame);                
      }
    }
  return 0;
}
