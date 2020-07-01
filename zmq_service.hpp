#ifndef ZMQ_SERVICE_HPP
#define ZMQ_SERVICE_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "timer.h"

#define HEADER_OFFSET (sizeof(long))

/////////////////////////////////////////////////////
////Zmq Data structure
/////////////////////////////////////////////////////
struct Data {
  Data()=default;

  Data(size_t ds):data_size(ds) {}

  // Transfer data to zmq message for transimission
  virtual bool to_bytes(void* buffer)=0;

  // Transfer zmq message to data
  virtual bool from_bytes(void* buffer, size_t size)=0;

  size_t data_size=0;
};

struct ImageData : public Data {
 public:
  ImageData() = default;

  ImageData(cv::Size size, int data_type) {
    frame = cv::Mat(size, data_type);
    data_size = frame.total() * frame.channels();
  }

  virtual bool to_bytes(void* buffer) {
    if (data_size == 0)
      return false;
    memcpy(buffer, (void*)(frame.data), data_size);
    return true;
  }

  virtual bool from_bytes(void* buffer, size_t size) {
    // Expect constant input image
    if (data_size != size)
      return false;
    memcpy((void*)(frame.data), buffer, size);
    return true;
  }

  // Set the resize shape
  void set_resize(cv::Size size, int num_channels=3) {
    target_size = size;
    data_size = size.height * size.width * num_channels;
    do_resize = true;
  }

  // Read a frame from a video 
  bool read_video(cv::VideoCapture& cap) {
    bool ret = cap.read(frame);
    if (ret) {
      if (do_resize)
        cv::resize(frame, frame, target_size);
      else
        data_size = frame.total() * frame.channels();
    }
    return ret;
  }

  cv::Mat frame;
  bool do_resize=false;
  cv::Size target_size;
};

/////////////////////////////////////////////////////
////Zmq Server to supply data
/////////////////////////////////////////////////////
class ZmqServer {
 public:
  ZmqServer() = delete;
  ZmqServer(ZmqServer& other) = delete;
  ZmqServer(ZmqServer&& other) = delete;
  
  ZmqServer(std::string address) {
    ctx_srv = std::make_unique<zmq::context_t>();
    sock_srv = std::make_unique<zmq::socket_t>(*ctx_srv, zmq::socket_type::rep);
    sock_srv->bind(address);
  }

  // Accept connection from client
  bool initial_connection() {
    std::unique_lock<std::mutex> lk(mu);
    // Currently only do 1-to-1 connection
    // If already connected, abort
    if (connected)
      return false;
    zmq::message_t msg_conn;
    sock_srv->recv(&msg_conn);
    std::string conn_req(static_cast<char*>(msg_conn.data()), msg_conn.size());

    if (conn_req == "ConnectionRequest")
        connected = true;
    // std::string reply = connected ? "OK" : "FAILED";
    // zmq::message_t msg_reply((void*)(reply.data()), reply.length());
    // sock_srv->send(msg_reply);
    return true;
  }

  bool send(Data* data) {
    if (data->data_size == 0)
      return false;
    // data to message
    zmq::message_t data_msg(data->data_size + HEADER_OFFSET);

    // add input time in the header field
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long input_time = now_ms.time_since_epoch().count();
    memcpy(data_msg.data(), (void*)(&input_time), sizeof(long));
    data->to_bytes(data_msg.data() + HEADER_OFFSET);

    // send data
    sock_srv->send(data_msg);

    // wait for reply
    zmq::message_t msg_reply;
    sock_srv->recv(&msg_reply);
    std::string reply(static_cast<char*>(msg_reply.data()), msg_reply.size());
    return reply == "OK";
  }

  void end_connection() {
    zmq::message_t abort_msg_srv(4);
    std::string abort_msg = "DONE";
    memcpy(abort_msg_srv.data(), abort_msg.data(), 4);
    sock_srv->send(abort_msg_srv);
  }

  // Start the server
  void start() {
  }
  
 private:
  std::unique_ptr<zmq::context_t> ctx_srv;
  std::unique_ptr<zmq::socket_t> sock_srv;

  std::mutex mu;
  bool connected=false;
};

/////////////////////////////////////////////////////
////Zmq Client to request data
/////////////////////////////////////////////////////
class ZmqClient {
 public:
  ZmqClient(ZmqClient& other) = delete;
  ZmqClient(ZmqClient&& other) = delete;
  
  ZmqClient(std::string address) {
    ctx_cli = std::make_unique<zmq::context_t>();
    sock_cli = std::make_unique<zmq::socket_t>(*ctx_cli, zmq::socket_type::req);
    sock_cli->connect(address);
  }

  // Connect to server
  bool initial_connection() {
    std::unique_lock<std::mutex> lk(mu);
    if (connected)
      return false;
    std::string connection_req = "ConnectionRequest";
    zmq::message_t msg_conn((void*)(connection_req.data()), connection_req.length());
    sock_cli->send(msg_conn);

    // zmq::message_t msg_conn_reply;
    // sock_cli->recv(&msg_conn_reply);
    // std::string conn_rep(static_cast<char*>(msg_conn_reply.data()), msg_conn_reply.size());
    // connected = (conn_rep == "OK");
    return true;
  }

  // Receiving data
  // return if valid data is received
  bool recv(Data* data) {
    zmq::message_t msg_cli;
    sock_cli->recv(&msg_cli);
    if (msg_cli.size() == 4)
      if (std::string(static_cast<char*>(msg_cli.data()), 4) == "DONE") {
        finished = true;
        return false;
      }

    // Read input time in the header field
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long cur_time = now_ms.time_since_epoch().count();
    long input_time;
    memcpy(&input_time, (msg_cli.data()), sizeof(long));
    std::cout << "Client receive image message time (us): " << cur_time - input_time << std::endl;

    size_t data_size = msg_cli.size() - HEADER_OFFSET;
    bool valid_data = data->from_bytes(msg_cli.data() + HEADER_OFFSET, data_size);
    std::string reply = valid_data ? "OK" : "FAILED";
    zmq::message_t msg_reply((void*)(reply.data()), reply.length());
    sock_cli->send(msg_reply);
    return valid_data;
  }

  // Start to work as client
  void start() {
  }

 private:
  std::unique_ptr<zmq::context_t> ctx_cli;
  std::unique_ptr<zmq::socket_t> sock_cli;

  std::mutex mu;
  bool connected=false;
  bool finished=false;
};

#endif
