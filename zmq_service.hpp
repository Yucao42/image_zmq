#ifndef ZMQ_SERVICE_HPP
#define ZMQ_SERVICE_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <zmq.hpp>

#include "timer.h"
#include "data_struct.hpp"

#define HEADER_OFFSET (sizeof(long))

long get_average(const std::vector<long>& stats, const std::string& name="memcpy") {
  long average;
  if (stats.size()) {
    average = std::accumulate(stats.begin(), stats.end(), 0.) / stats.size();
    std::cout << "Average " << name << " time(us): " << average << std::endl;
  }
  return average;
}

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
    memcpy_time.reserve(10000);
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
    timer.tick();
    zmq::message_t data_msg(data->data_size + HEADER_OFFSET);

    // add input time in the header field
    data->to_bytes(data_msg.data() + HEADER_OFFSET);
    std::cout << "Server memcpy time (us): " << timer.tock_count() << std::endl;
    memcpy_time.emplace_back(timer.tock_count());

    // send data
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long input_time = now_ms.time_since_epoch().count();
    memcpy(data_msg.data(), (void*)(&input_time), sizeof(long));
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
    get_average(memcpy_time, "memcpy");
    connected = false;
  }

  

  // Start the server
  void start() {
  }
  
 private:
  std::unique_ptr<zmq::context_t> ctx_srv;
  std::unique_ptr<zmq::socket_t> sock_srv;

  std::mutex mu;
  bool connected=false;
  TimerMicroSecconds timer;
  std::vector<long> memcpy_time;
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
    memcpy_time.reserve(10000);
    delivery_time.reserve(10000);
  }

  // Connect to server
  bool initial_connection() {
    std::unique_lock<std::mutex> lk(mu);
    if (connected)
      return false;
    std::string connection_req = "ConnectionRequest";
    zmq::message_t msg_conn((void*)(connection_req.data()), connection_req.length());
    sock_cli->send(msg_conn);
    connected = true;
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
        connected = false;
        return false;
      }

    // Read input time in the header field
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long cur_time = now_ms.time_since_epoch().count();
    long input_time;
    memcpy(&input_time, (msg_cli.data()), sizeof(long));
    std::cout << "Client receive image message time (us): " << cur_time - input_time << std::endl;
    delivery_time.emplace_back(cur_time - input_time);

    timer.tick();
    size_t data_size = msg_cli.size() - HEADER_OFFSET;
    bool valid_data = data->from_bytes(msg_cli.data() + HEADER_OFFSET, data_size);
    std::cout << "Client memcpy time (us): " << timer.tock_count() << std::endl;
    memcpy_time.emplace_back(timer.tock_count());
    std::string reply = valid_data ? "OK" : "FAILED";
    zmq::message_t msg_reply((void*)(reply.data()), reply.length());
    sock_cli->send(msg_reply);
    return valid_data;
  }

  // Start to work as client
  void start() {
  }
  void end_connection() {connected = false;}

  long mean_delivery_time() {return get_average(delivery_time, "delivery");}

  long mean_memcpy_time() {return get_average(memcpy_time, "memcpy");}

 private:
  std::unique_ptr<zmq::context_t> ctx_cli;
  std::unique_ptr<zmq::socket_t> sock_cli;

  std::mutex mu;
  bool connected=false;
  bool finished=false;
  TimerMicroSecconds timer;
  std::vector<long> memcpy_time;
  std::vector<long> delivery_time;
};

#endif
