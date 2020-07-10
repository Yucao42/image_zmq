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
#include <stdlib.h> 
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
  std::string input_video("../../one_sec.mp4");
  int base_port = 8999;
  int num_streams = 1;
  int width = 640;
  int height = 368;
  bool lossless = false;

  std::string size_cmd = " -vf scale=" + 
      std::to_string(width) + "x" + std::to_string(height);
  std::string input_cmd = "ffmpeg -re -i " + input_video + " ";
  std::string encoding_flags("");
  if (lossless)
    encoding_flags = " -crf 0 -c:v libx264rgb ";
  std::string output_cmd = size_cmd + " -movflags faststart " + 
      encoding_flags + " -b 90000k -an -f mpegts tcp://localhost:";

  for (int i = 0; i < num_streams; ++i) {
    int port = base_port + i;
    std::string port_cmd = std::to_string(port) + "?listen";
    std::string cmd = input_cmd + output_cmd + port_cmd;
    if (fork() == 0) {
      std::cout << cmd << std::endl;
      system(cmd.c_str());
      return 0;
    }
  }

#ifdef SPAWN_THREADS
  std::vector<std::thread> threads;
  threads.reserve(num_streams);
  std::string port;
  for (int i = 0; i < num_streams; ++i) {
    int port_num = base_port + i;
    port = std::to_string(port_num);
    std::string addr = addr_prefix + port + addr_postfix;
    std::this_thread::sleep_for (std::chrono::milliseconds(100));
    threads.emplace_back(std::move(std::thread(read_stream, addr)));
  }
  for (int i = 0; i < threads.size();++i)
    threads[i].join();
#endif
#ifdef SPAWN_PROCESSES
  std::string port;
  for (int i = 0; i < num_streams; ++i) {
    port = std::to_string(base_port + i);
    std::string addr = addr_prefix + port + addr_postfix;
    std::this_thread::sleep_for (std::chrono::milliseconds(500));
    if (fork() == 0) {
      std::cout << addr <<" " << i << std::endl;
      read_stream(addr);
      return 0;
    } 
  }
  for (int i = 0; i < num_streams; ++i) 
    wait(NULL);
#endif
  // Wait input streams to finish
  for (int i = 0; i < num_streams; ++i) 
    wait(NULL);

  return 0;
}
