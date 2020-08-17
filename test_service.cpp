#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <zmq.hpp>

#include "data_struct.hpp"
#include "zmq_service.hpp"

#define NUM_TRANSMISSION_TEST (500)

static bool using_proto = false;

void run_client(const std::vector<int> &ports, const std::string &client_id,
                bool use_poller = false) {
  // Client connection infos
  std::string addr("tcp://localhost");
  std::vector<std::string> addres(ports.size(), addr);
  ZmqClient client(addres, ports, client_id, use_poller);

  // Image size parameter
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;
  ImageData img(size_small, CV_8UC3);
  size_t size = 368 * 640 * 3;
  TimerMicroSecconds timer;

  // Statistics - FPS
  long init_time = 0, cur_time, time_passed;
  int total_received=0, frame_count = 0;
  double fps;
  bool continue_ = true;
  if (client.sync_connection()) {
    while (continue_) {
      // Client receive image
      if (using_proto)
        continue_ = client.recv(&img);
      else 
        continue_ = client.recv((void*)img.frame.data, size);
      {
        cur_time = get_time_since_epoch_count();
        if (init_time == 0) {
          init_time = cur_time;
        } else {
          time_passed = cur_time - init_time;
          fps = (++total_received) * 1.0 / time_passed * 1000000;
          std::cout  << ports[0]<< "Port " << total_received << " FPS: " << fps << std::endl;
        }
      }
    }
    // Printout FPS and time infos
    std::cout << "Port " << ports[0] << " FINAL FPS: " << fps << std::endl;
    client.mean_memcpy_time();
    client.mean_delivery_time();
  }
}

void run_server(int port = 50051,
                const std::string video_file = "../data/video_1.mp4") {
  // Image size parameter
  cv::Size size_small;
  size_small.height = 368;
  size_small.width = 640;

  // Prepare data
  ImageData img;
  img.set_resize(size_small);
  cv::VideoCapture cap(video_file);
  cv::Mat image_mat;

  // Make a server
  std::string addr("tcp://*");
  ZmqServer server(addr, port);

  // TImer
  TimerMicroSecconds timer;
  bool read_success = img.read_video(cap);
  cap.read(image_mat);
  cv::resize(image_mat, image_mat, size_small);
  long total_time = 0, count = 0;
  size_t size = 368 * 640 * 3;

  if (server.sync_connection()) {
    server.send(&img);
    // while (count < NUM_TRANSMISSION_TEST) {
    while (read_success) {
      if (using_proto)
        server.send((void*)img.frame.data, size);
      else {
        cv::resize(image_mat, image_mat, size_small);
        server.send((void*)image_mat.data, size);
      }

      timer.tick();
      ++count;
      if (using_proto)
        read_success = img.read_video(cap);
      else
        read_success = cap.read(image_mat);
      std::cout<< "Video reading time " << timer.tock_count() <<std::endl;
      total_time += timer.tock_count();
    }
    std::cout << "Average Video reading time " << total_time / count
              << std::endl;
    server.end_connection();
  }
  cap.release();
}

int main(int argc, char **argv) {
  int num_streams = 1, base_port = 50060, num_to_wait = 0;
  bool test_single_client(false);
  bool use_poller(false);
  std::vector<std::string> addresses;
  std::vector<int> ports;

  // Parse arguments
  if (argc < 3) {
    std::cout
        << "\nUsage: ./ffmpeg_image num_streams test_single_client use_poller\n"
        << std::endl;
    return 0;
  } else {
    num_streams = atoi(argv[1]);
    test_single_client = (atoi(argv[2]) > 0);
    if (argc > 3)
      use_poller = (atoi(argv[3]) > 0);
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

      if (!test_single_client && !use_poller) {
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

  // Fork only one client process to connect to every server
  if (test_single_client | use_poller) {
    if (fork() == 0) {
      run_client(ports, "Client", use_poller);
      return 0;
    }
  }

  num_to_wait =
      num_streams + ((test_single_client | use_poller) ? 1 : num_streams);
  // Wait for all the server/client processes to finish
  for (int i = 0; i < num_to_wait; ++i)
    wait(NULL);

  return 0;
}
