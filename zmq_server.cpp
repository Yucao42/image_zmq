#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <stdio.h>
#include <signal.h>
#include <opencv2/opencv.hpp>

// Timer related
#include <sys/time.h>
#include <chrono>


#define LOGGER_LENGTH 30
#define TIMER_EXPR(timer_, expr_, desc_)                         \
    do {                                                         \
       timer_.tick();                                            \
       expr_;                                                    \
       std::cout << desc_ << timer_.tock_count() << std::endl;   \
    } while(0)

// It is a stopwatch which ticks and tocks
template<typename T>
class Timer {
 public:
  Timer() {
    tick();
  }

  void tick() {
    begin_time = std::chrono::steady_clock::now();
  }

  void tock() {
    time_elapsed = std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - begin_time);
  }

  long tock_count() {
    time_elapsed = std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - begin_time);
    return long(time_elapsed.count());
  }

  T get_duration() const {
    return time_elapsed;
  }

  std::chrono::time_point<std::chrono::steady_clock> get_starttime() const {
    return begin_time;
  }

 private:
  std::chrono::time_point<std::chrono::steady_clock> begin_time;

  T time_elapsed;
};

typedef Timer<std::chrono::microseconds> TimerMicroSecconds;

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

  // Client
  zmq::context_t ctx_cli;
  zmq::socket_t sock_cli(ctx_cli, zmq::socket_type::req);
  sock_cli.connect("tcp://localhost:50051");

  auto start = std::chrono::steady_clock::now();
  auto end =   std::chrono::steady_clock::now();

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
  cv::Mat frame_rcv = cv::Mat(size, CV_8UC3);

  // Data messages
  zmq::message_t img_msg_srv(num_pixels);
  zmq::message_t img_msg_cli(num_pixels);

  std::string reply_msg = "OK";
  zmq::message_t reply_msg_cli(2);
  memcpy(reply_msg_cli.data(), reply_msg.data(), 2);
  zmq::message_t reply_msg_rcv(2);

  start = std::chrono::steady_clock::now();
  end = std::chrono::steady_clock::now();

  // Initialize connection
  sock_cli.send(reply_msg_cli);

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
    zmq::message_t img_msg_srv(num_pixels);
    // Server send image
    memcpy(img_msg_srv.data(), (void*)(frame.data), num_pixels);
    std::cout << "Server copy image to message time (us): " << timer.tock_count() << std::endl;

    timer.tick();
    sock_srv.send(img_msg_srv);
    std::cout << "Server send image message time (us): " << timer.tock_count() << std::endl;

    // Client receive image
    timer.tick();
    sock_cli.recv(&img_msg_cli);
    std::cout << "Client receive image message time (us): " << timer.tock_count() << std::endl;

    timer.tick();
    memcpy((void*)(frame_rcv.data), (img_msg_cli.data()), num_pixels);
    if (++frame_count < 20) {
	    sprintf(str, "debug/img_%d.jpg", frame_count);
      cv::imwrite(str, frame_rcv);
    }
    std::cout << "Client memcpy image time (us): " << timer.tock_count() << std::endl;

    // Client send reply
    memcpy(reply_msg_cli.data(), reply_msg.data(), 2);
    sock_cli.send(reply_msg_cli);

    // Server receive reply
    timer.tick();
    sock_srv.recv(&reply_msg_rcv);
    std::cout << "Server received reply time (us): " << timer.tock_count() << std::endl;
    std::cout << std::string(static_cast<char*>(reply_msg_rcv.data()), 2) + "\n" << std::endl; 

    // timer.tick();
    // read_success = cap.read(frame);
    // std::cout << "Image read time (us): " << timer.tock_count() << std::endl;
  }
  delete[] str;
  myfile.close();
  return 0;
}
