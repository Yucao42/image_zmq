#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "data_struct.hpp"
#include "zmq_service.hpp"

auto now = std::chrono::system_clock::now();
auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
long input_time = now_ms.time_since_epoch().count();
long init_time = 0;
int total;
double fps;
std::mutex mu;

void client(int port=50051) {
  std::string addr("tcp://localhost");
  std::string client_id = std::to_string(port);
  ZmqClient client(addr, port, client_id);

#ifdef TEST_LABEL
  ClassificationData label;
  if (client.sync_connection()) {
    std::cout << "Client connection built" << std::endl;
    // Client receive image
    client.recv(&label);
    client.mean_memcpy_time();
    client.mean_delivery_time();
    std::cout << "Received " << label.label << " " << label.score << std::endl;
    // Receive the end connection message
    client.recv(&label);
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
  if (client.sync_connection()) {
    std::cout << "Client connection built" << std::endl;
    while (continue_) {
      // Client receive image
      continue_ = client.recv(&img);
      {
#ifndef SPAWN_PROCESSES
        std::unique_lock<std::mutex> lk(mu);
#endif
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
        if (init_time == 0) {
          init_time = now_ms.time_since_epoch().count();
        } else {
          long time_passed = now_ms.time_since_epoch().count() - init_time; 
          fps = (++total) * 1.0 / time_passed * 1000000; 
          std::cout << "Port " << port << " FPS: " << fps << std::endl;
        }
      }

      /* Correctness check */
      // if (0 && ++frame_count < 20) {
      //    sprintf(str, "debug/img_%d.jpg", frame_count);
      //    cv::imwrite(str, img.frame);                
      // }
    }
    client.mean_memcpy_time();
    client.mean_delivery_time();
  }
#endif
}

void server(int port=50051) {
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;

  std::string addr("tcp://*");
  ZmqServer server(addr, port);

#ifdef TEST_LABEL
  ClassificationData label("Car", 0.88);
  if (server.sync_connection()) {
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

  if (server.sync_connection()) {
    std::cout << "Server connection built" << std::endl;
    server.send(&img);
    while (read_success) {
      server.send(&img);
      read_success = img.read_video(cap);
    }
    server.end_connection();
  }
  cap.release();
#endif
}

int main(int argc, char** argv) {
  int num_streams = 1;
  int base_port = 50060;
  if (argc < 2) {
    std::cout << "\nUsage: ./ffmpeg_image num_streams\n please input number of streams" <<std::endl;
    return 0;
  } else {
    num_streams = atoi(argv[1]);
  }

  std::vector<std::thread> server_threads;
  std::vector<std::thread> client_threads;
  for (int i = 0; i < num_streams; ++i) {
#ifdef SPAWN_PROCESSES
    {
      // Fork server processes
      if (fork() == 0) {
        server(base_port + i * 2);
        return 0;
      }  

      // Fork client processes
      if (fork() == 0) {
        client(base_port + i * 2);
        return 0;
      }  
    } 
#else
    {
      server_threads.reserve(num_streams);
      client_threads.reserve(num_streams);
      server_threads.emplace_back(std::move(std::thread(server, base_port + i * 2)));
      client_threads.emplace_back(std::move(std::thread(client, base_port + i * 2)));
    }
#endif
  }

#ifdef SPAWN_PROCESSES
  {
    // Wait for all the server/client processes to finish
    for (int i = 0; i < 2 * num_streams; ++i)
      wait(NULL);
  } 
#else 
  {
    for (int i = 0; i < num_streams; ++i) {
      server_threads[i].join();
      client_threads[i].join();
    }
  }
#endif
  return 0;
}
