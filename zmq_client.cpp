#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "data_struct.hpp"
#include "zmq_service.hpp"

int main(int argc, char** argv) {
  std::string addr("tcp://localhost:50051");
  ZmqClient client(addr);

#ifdef TEST_LABEL
  ClassificationData label;
  if (client.initial_connection()) {
    std::cout << "Client connection built" << std::endl;
    // Client receive image
    client.recv(&label);
    client.mean_memcpy_time();
    client.mean_delivery_time();
    std::cout << "Received " << label.label << " " << label.score << std::endl;
  }
#endif
#ifdef TEST_VIDEO
  char str[30];
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;
  ImageData img(size_small, CV_8UC3);
  TimerMicroSecconds timer;

  int frame_count = 0;
  bool continue_=true;
  if (client.initial_connection()) {
    while (continue_) {
      // Client receive image
      continue_ = client.recv(&img);
      if (0 && ++frame_count < 20) {
         sprintf(str, "debug/img_%d.jpg", frame_count);
         cv::imwrite(str, img.frame);                
      }
    }
    client.mean_memcpy_time();
    client.mean_delivery_time();
  }
#endif
  return 0;
}
