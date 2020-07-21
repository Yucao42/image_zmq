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


void run_client(const std::vector<int>& ports,
                const std::string& client_id) {
  // Client connection infos
  std::string addr("tcp://localhost");
  std::vector<std::string> addres(ports.size(), addr);
  ZmqClient client(addres, ports, client_id);

  // Image size parameter
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;
  ImageData img(size_small, CV_8UC3);
  TimerMicroSecconds timer;

  // Statistics - FPS
  long init_time = 0, cur_time, time_passed;
  int total_received, frame_count=0;
  double fps;
  bool continue_=true;
  if (client.sync_connection()) {
    while (continue_) {
      // Client receive image
      continue_ = client.recv(&img);
      {
        cur_time = get_time_since_epoch_count();
        if (init_time == 0) {
          init_time = cur_time;
        } else {
          time_passed = cur_time - init_time; 
          fps = (++total_received) * 1.0 / time_passed * 1000000; 
          std::cout << "Port " << ports[0] << " FPS: " << fps << std::endl;
        }
      }
    }
    // Printout FPS and time infos
    std::cout << "Port " << ports[0] << " FINAL FPS: " << fps << std::endl;
    client.mean_memcpy_time();
    client.mean_delivery_time();
  }
}


void run_server(int port=50051, 
                const std::string video_file="../data/video_1.mp4") {
  // Image size parameter
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;

  // Prepare data
  ImageData img;
  img.set_resize(size_small);
  cv::VideoCapture cap(video_file);

  // Make a server
  std::string addr("tcp://*");
  ZmqServer server(addr, port);

  // TImer
  TimerMicroSecconds timer;
  bool read_success = img.read_video(cap);
  long total_time=0, count=0;

  if (server.sync_connection()) {
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
  int num_streams = 1, base_port = 50060, num_to_wait = 0;
  bool test_single_client(false);
  std::vector<std::string> addresses;
  std::vector<int> ports;

  // Parse arguments
  if (argc < 3) {
    std::cout << "\nUsage: ./ffmpeg_image num_streams test_single_client \n" <<std::endl;
    return 0;
  } else {
    num_streams = atoi(argv[1]);
    test_single_client = (atoi(argv[2]) > 0);
  }

  // Launch clients/servers processes
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

      // Fork client processes
      if (!test_single_client) {
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

  // Fork only one client process to connect to every server
  if (test_single_client) {
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
