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

#define NUM_TRANSMISSION_TEST (500)

auto now = std::chrono::system_clock::now();
auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
long input_time = now_ms.time_since_epoch().count();
long init_time = 0;
int total;
double fps;
std::mutex mu;

void run_client(const std::vector<int>& ports,
                const std::string& client_id) {
  std::string addr("tcp://localhost");
  std::vector<std::string> addres(ports.size(), addr);
  ZmqClient client(addres, ports, client_id);

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
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
        if (init_time == 0) {
          init_time = now_ms.time_since_epoch().count();
        } else {
          long time_passed = now_ms.time_since_epoch().count() - init_time; 
          fps = (++total) * 1.0 / time_passed * 1000000; 
          std::cout << "Port " << ports[0] << " FPS: " << fps << std::endl;
        }
      }

      /* Correctness check */
      // if (0 && ++frame_count < 20) {
      //    sprintf(str, "debug/img_%d.jpg", frame_count);
      //    cv::imwrite(str, img.frame);                
      // }
    }
    std::cout << "Port " << ports[0] << " FINAL FPS: " << fps << std::endl;
    client.mean_memcpy_time();
    client.mean_delivery_time();
  }
}

void run_server(int port=50051, const std::string video_file="../data/video_1.mp4") {
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;

  std::string addr("tcp://*");
  ZmqServer server(addr, port);

  // Prepare data
  ImageData img;
  img.set_resize(size_small);
  cv::VideoCapture cap(video_file);

  TimerMicroSecconds timer;
  bool read_success = img.read_video(cap);
  long total_time=0, count=0;

  if (server.sync_connection()) {
    std::cout << "Server connection built" << std::endl;
    server.send(&img);
    while (count < NUM_TRANSMISSION_TEST) {
      timer.tick();
      // read_success = img.read_video(cap);
      // std::cout<< "Video reading time " << timer.tock_count() <<std::endl;
      server.send(&img);
      ++count;
      total_time += timer.tock_count();
    }
    std::cout<< "Average Video reading time " << total_time / count <<std::endl;
    server.end_connection();
  }
  cap.release();
}

int main(int argc, char** argv) {
  int num_streams = 1;
  int base_port = 50060;
  int num_to_wait = 0;
  bool test_single_client(false);
  if (argc < 3) {
    std::cout << "\nUsage: ./ffmpeg_image num_streams test_single_client \n" <<std::endl;
    return 0;
  } else {
    num_streams = atoi(argv[1]);
    test_single_client = (atoi(argv[2]) > 0);
  }

  std::vector<std::string> addresses;
  std::vector<int> ports;
  addresses.reserve(num_streams);
  ports.reserve(num_streams);
  for (int i = 0; i < num_streams; ++i) {
    {
      // Fork server processes
      if (fork() == 0) {
        std::string video_file = "../data/video_" + std::to_string(i) + ".mp4";
        std::cout << "READING VIDEO " << video_file << std::endl;
        run_server(base_port + i * 2, video_file);
        return 0;
      }  

      if (!test_single_client) {
        // Fork client processes
        if (fork() == 0) {
          std::vector<int> single_port(1, base_port + i * 2);
          run_client(single_port, std::to_string(base_port + i * 2));
          return 0;
        }
      } else {
        ports.emplace_back(base_port + i * 2);
      }
    } 
  }

  if (test_single_client) {
    // Fork client processes
    if (fork() == 0) {
      run_client(ports, "Client");
      return 0;
    }
  }
  num_to_wait = num_streams + (test_single_client? 1 : num_streams);
  // Wait for all the server/client processes to finish
  for (int i = 0; i < num_to_wait; ++i)
    wait(NULL);

  return 0;
}
