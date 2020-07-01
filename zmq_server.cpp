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
#define TIMER_EXPR(timer_, expr_, desc_)                         \
    do {                                                         \
       timer_.tick();                                            \
       expr_;                                                    \
       std::cout << desc_ << timer_.tock_count() << std::endl;   \
    } while(0)

std::ofstream myfile;

void signal_handler(int signum)
{
	myfile.close();
	exit(signum);
}

int main(int argc, char** argv) {
  // Zmq initialization
  // Server
  zmq::context_t ctx_srv;
  zmq::socket_t sock_srv(ctx_srv, zmq::socket_type::rep);
  sock_srv.bind("tcp://*:50051");

  signal(SIGINT, signal_handler);
  myfile.open("server.txt");
  char *str = new char[LOGGER_LENGTH];
  long int time_elapsed;
  size_t num_pixels;
  int num_query;
  bool read_success;
  bool do_resize=true;
  cv::VideoCapture cap("../sample_1080p_h264.mp4");
  cv::Mat frame;
  read_success = cap.read(frame);

  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
  long duration = now_ms.time_since_epoch().count();
  cv::Size size = frame.size();
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;
  if (do_resize) {
    num_pixels = size_small.height * size_small.width * frame.channels();
    size = size_small;
  } else {
    num_pixels = frame.total() * frame.channels();
  }

  std::cout << "Number of pixels: " << num_pixels << std::endl;

  // Data messages
  zmq::message_t img_msg_srv(num_pixels);
  zmq::message_t reply_msg_rcv(2);

  // Server receive reply
  sock_srv.recv(&reply_msg_rcv);
  std::cout << "Connection built: " << 
      std::string(static_cast<char*>(reply_msg_rcv.data()), 2) << std::endl; 

  TimerMicroSecconds timer;
  int frame_count = 0;
  while (read_success) {
    if (do_resize) {
      timer.tick();
      cv::resize(frame, frame, size_small);
      std::cout << "Image resize time (us): " << timer.tock_count() << std::endl;
    }

    timer.tick();
    // The image and the input time
    zmq::message_t img_msg_srv(num_pixels+sizeof(long));

    // Server send image
    memcpy(img_msg_srv.data(), (void*)(frame.data), num_pixels);
    std::cout << "Server copy image to message time (us): " << timer.tock_count() << std::endl;

    timer.tick();
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long duration = now_ms.time_since_epoch().count();
    memcpy(img_msg_srv.data()+num_pixels, (void*)(&duration), sizeof(long));
    sock_srv.send(img_msg_srv);
    std::cout << "Server send image message time (us): " << timer.tock_count() << std::endl;

    // Server receive reply
    timer.tick();
    sock_srv.recv(&reply_msg_rcv);
    std::cout << "Server received reply time (us): " << timer.tock_count() << std::endl;
    std::cout << std::string(static_cast<char*>(reply_msg_rcv.data()), 2) + "\n" << std::endl; 

    // Read a frame from video stream
    timer.tick();
    read_success = cap.read(frame);
    std::cout << "Image read time (us): " << timer.tock_count() << std::endl;
  }
  delete[] str;
  myfile.close();
  return 0;
}
