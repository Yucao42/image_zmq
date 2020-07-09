#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include "../timer.h"

const std::string addr_prefix("tcp://localhost:");
const std::string addr_postfix("?client");

void read_stream(const std::string& addr) {
  cv::VideoCapture cap(addr, cv::CAP_FFMPEG);
  cv::Mat image;
  TimerMicroSecconds timer;
  if (cap.isOpened()) {
    bool continue_ = true;
    while(continue_) {
      continue_ = cap.read(image);
      std::cout<< "Read one image " << addr << std::endl;
      timer.tick();
      cv::Mat wrapper = cv::Mat(image.size(), image.type(), image.data);
      std::cout<< "Time for wrapping an image(ns): " << timer.tock_count() << std::endl;
      cv::imwrite("image.jpg", wrapper);
    }
  }

}

int main(int argc, char** argv) {

  std::vector<std::thread> threads;
  if (argc > 1) {
    threads.reserve(argc - 1);
    std::string port;
    for (int i = 1; i < argc; ++i) {
      port = std::string(argv[i]);
      std::string addr = addr_prefix + port + addr_postfix;
      threads.emplace_back(std::move(std::thread(read_stream, addr)));
    }
  } else {
    std::cout << "usage: ./ffmpeg_image [list of ports to connect]" << std::endl;
    return 0;
  }
  for (int i = 0; i < threads.size();++i)
    threads[i].join();
  return 0;
}
