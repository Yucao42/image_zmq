#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <sys/wait.h> 
#include "../timer.h"

const std::string addr_prefix("tcp://localhost:");
const std::string addr_postfix("?client");

void read_stream(const std::string& addr) {
  cv::VideoCapture cap(addr, cv::CAP_FFMPEG);
  cv::Mat image;
  TimerMicroSecconds timer;
  int count = 0;
  TimerMicroSecconds open_timer;
  open_timer.tick();

  // Allow one second for re-connection
  while (!cap.isOpened() && open_timer.tock_count() < 1000000)
    cap.open(addr, cv::CAP_FFMPEG);
  if (cap.isOpened()) {
    std::cout<< "Starting to listen to " << addr << std::endl;
    bool continue_ = true;
    while(continue_) {
      continue_ = cap.read(image);
      std::cout<< "Read one image " << addr << " count: " << ++count << std::endl;
      timer.tick();
      cv::Mat wrapper = cv::Mat(image.size(), image.type(), image.data);
    }
  }
}

int main(int argc, char** argv) {

#ifdef SPAWN_THREADS
  std::vector<std::thread> threads;
  if (argc > 1) {
    threads.reserve(argc - 1);
    std::string port;
    for (int i = 1; i < argc; ++i) {
      port = std::string(argv[i]);
      std::string addr = addr_prefix + port + addr_postfix;
      std::this_thread::sleep_for (std::chrono::milliseconds(100));
      threads.emplace_back(std::move(std::thread(read_stream, addr)));
    }
  } else {
    std::cout << "usage: ./ffmpeg_image [list of ports to connect]" << std::endl;
    return 0;
  }
  for (int i = 0; i < threads.size();++i)
    threads[i].join();
#endif
#ifdef SPAWN_PROCESSES
  if (argc > 1) {
    std::string port;
    for (int i = 1; i < argc; ++i) {
      port = std::string(argv[i]);
      std::string addr = addr_prefix + port + addr_postfix;
      if (fork() == 0) {
        read_stream(addr);
      } 
      std::this_thread::sleep_for (std::chrono::milliseconds(100));
    }
    for (int i = 1; i < argc; ++i) 
      wait(NULL);
  } else {
    std::cout << "usage: ./ffmpeg_image [list of ports to connect]" << std::endl;
    return 0;
  }
#endif

  return 0;
}
