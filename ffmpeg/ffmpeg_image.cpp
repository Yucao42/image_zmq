#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <opencv2/opencv.hpp>
#include <sys/wait.h> 
#include <stdlib.h> 
#include "../timer.h"

const std::string addr_prefix("tcp://localhost:");
const std::string addr_postfix("?client");
auto now = std::chrono::system_clock::now();
auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
long input_time = now_ms.time_since_epoch().count();
long init_time = 0;
int total = 0;
double fps;
std::mutex mu;

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
    std::cout<< "Starting connection to " << addr << std::endl;
    bool continue_ = true;
    while(continue_) {
      continue_ = cap.read(image);
      {
        std::unique_lock<std::mutex> lk(mu);
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
        if (init_time == 0) {
          init_time = now_ms.time_since_epoch().count();
        } else {
          long time_passed = now_ms.time_since_epoch().count() - init_time; 
          fps = (++total) * 1.0 / time_passed * 1000000; 
          std::cout << "FPS: " << fps << std::endl;
        }
      }
      std::cout<< "Read one image " << addr << " count: " << total << std::endl;
      timer.tick();
      // cv::Mat wrapper = cv::Mat(image.size(), image.type(), image.data);
    }
  }
}

int main(int argc, char** argv) {
  int num_streams = 8;
  if (argc < 2) {
    std::cout << "\nUsage: ./ffmpeg_image num_streams\n please input number of streams" <<std::endl;
    return 0;
  } else {
    num_streams = atoi(argv[1]);
  }
  // std::string input_video("../../one_sec.mp4");
  std::string input_video("../../sample_1080p_h264.mp4");
  int base_port = 8999;
  int width = 640;
  int height = 368;
  bool lossless = false;

  std::string size_cmd = " -vf scale=" + 
      std::to_string(width) + "x" + std::to_string(height);
  std::string input_cmd = "ffmpeg -loglevel 32 -i " + input_video + " ";
  std::string encoding_flags("");
  if (lossless)
    encoding_flags = " -crf 0 -c:v libx264rgb ";
  std::string output_cmd = size_cmd + " -movflags faststart " + 
      encoding_flags + " -b:v 90000k -an -f mpegts tcp://localhost:";

  for (int i = 0; i < num_streams; ++i) {
    int port = base_port + i;
    std::string port_cmd = std::to_string(port) + "?listen";
    std::string cmd = input_cmd + output_cmd + port_cmd;
    if (fork() == 0) {
      std::cout << cmd << std::endl;
      system(cmd.c_str());
      system("stty sane");
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
    // std::this_thread::sleep_for (std::chrono::milliseconds(100));
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
