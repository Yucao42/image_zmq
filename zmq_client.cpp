#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <stdio.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "timer.h"

#define LOGGER_LENGTH 30

std::ofstream myfile;

void signal_handler(int signum)
{
	myfile.close();
	exit(signum);
}

int main(int argc, char** argv) {
  // Client
  zmq::context_t ctx_cli;
  zmq::socket_t sock_cli(ctx_cli, zmq::socket_type::req);
  sock_cli.connect("tcp://localhost:50051");

  signal(SIGINT, signal_handler);
  myfile.open("client.txt");
  char *str = new char[LOGGER_LENGTH];
  long int time_elapsed;
  size_t num_pixels;
  int num_query, num_channels = 3;
  bool read_success;
  bool do_resize=true;

  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
  long duration = now_ms.time_since_epoch().count();

  cv::Size size_small, size;
  size_small.height = 368;
  size_small.width = 640;
  if (do_resize) {
    num_pixels = size_small.height * size_small.width * num_channels;
    size = size_small;
  }
  std::cout << "Number of pixels: " << num_pixels << std::endl;
  cv::Mat frame_rcv = cv::Mat(size, CV_8UC3);

  // Data messages
  zmq::message_t img_msg_cli(num_pixels);

  std::string reply_msg = "OK";
  zmq::message_t reply_msg_cli(2);
  memcpy(reply_msg_cli.data(), reply_msg.data(), 2);

  // Initialize connection
  sock_cli.send(reply_msg_cli);
  std::cout << "Connection built: " << 
      std::string(static_cast<char*>(reply_msg_cli.data()), 2) << std::endl; 

  TimerMicroSecconds timer;
  int frame_count = 0;
  bool continue_=true;
  long input_time;
  while (continue_) {
    // Client receive image
    timer.tick();
    sock_cli.recv(&img_msg_cli);
    now = std::chrono::system_clock::now();
    now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    duration = now_ms.time_since_epoch().count();
    memcpy(&input_time, (img_msg_cli.data()+num_pixels), sizeof(long));
    // std::cout << "Client receive image message time (us): " << timer.tock_count() << std::endl;
    std::cout << "Client receive image message time (us): " << duration - input_time << std::endl;

    timer.tick();
    memcpy((void*)(frame_rcv.data), (img_msg_cli.data()), num_pixels);
    if (0 && ++frame_count < 20) {
	    sprintf(str, "debug/img_%d.jpg", frame_count);
      cv::imwrite(str, frame_rcv);
    }
    std::cout << "Client memcpy image time (us): " << timer.tock_count() << std::endl;

    // Client send reply
    memcpy(reply_msg_cli.data(), reply_msg.data(), 2);
    sock_cli.send(reply_msg_cli);

  }
  delete[] str;
  myfile.close();
  return 0;
}
