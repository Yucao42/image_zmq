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

static std::string make_address_plus_port(const std::string& addr, int port) {
  return addr + ":" + std::to_string(port);
}

/////////////////////////////////////////////////////
////Zmq Server to supply data
/////////////////////////////////////////////////////
class ZmqServer {
 public:
  ZmqServer() = delete;
  ZmqServer(ZmqServer& other) = delete;
  ZmqServer(ZmqServer&& other) = delete;
  
  ZmqServer(std::string address, int port, int expected_clients=1) {
    ctx_srv = std::make_unique<zmq::context_t>();
    sock_srv = std::make_unique<zmq::socket_t>(*ctx_srv, zmq::socket_type::pub);
    address_ = address;
    port_ = port;
    expected_clients_ = expected_clients;
    bind_to = make_address_plus_port(address, port);
    sock_srv->bind(bind_to);
    memcpy_time.reserve(10000);
  }

  // Synchronize with expected client to get connected
  bool sync_connection() {
    std::unique_lock<std::mutex> lk(mu);
    if (sync_connected)
      return false;

    // Build a server to initialize connection with every clients
    auto ctx_conn = std::make_unique<zmq::context_t>();
    auto sock_conn = std::make_unique<zmq::socket_t>(*ctx_conn, zmq::socket_type::rep);
    std::string bind_to_conn = make_address_plus_port(address_, port_ + 1);
    client_ids.reserve(expected_clients_);
    sock_conn->bind(bind_to_conn);

    // Make sure all expected clients are connected
    while (client_ids.size() < expected_clients_) {
      // Receive connection request
      zmq::message_t msg_conn;
      sock_conn->recv(&msg_conn);
      std::string client_id(static_cast<char*>(msg_conn.data()), msg_conn.size());
      client_ids.push_back(client_id);

      // Reply
      std::string reply = "OK";
      zmq::message_t msg_reply((void*)(reply.data()), reply.length());
      sock_conn->send(msg_reply);
    }
    sync_connected = true;
    return true;
  }

  bool send(Data* data) {
    if (data->data_size == 0)
      return false;

    // data to message
    timer.tick();
    zmq::message_t data_msg(data->data_size + HEADER_OFFSET);

    // add input time in the header field
    data->to_bytes((char*)(data_msg.data()) + HEADER_OFFSET);
    std::cout << "Server memcpy time (us): " << timer.tock_count() << std::endl;
    memcpy_time.emplace_back(timer.tock_count());

    // send data
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long input_time = now_ms.time_since_epoch().count();
    memcpy(data_msg.data(), (void*)(&input_time), sizeof(long));
    sock_srv->send(data_msg);
    return true;
  }

  void end_connection() {
    // Send a message to tell the client(s) to end communications
    zmq::message_t abort_msg_srv(4);
    std::string abort_msg = "DONE";
    memcpy(abort_msg_srv.data(), abort_msg.data(), 4);
    sock_srv->send(abort_msg_srv);
    get_average(memcpy_time, "memcpy");
    sync_connected = false;
  }
  
  // Start the server
  void start() {
  }
  
 private:
  std::unique_ptr<zmq::context_t> ctx_srv;
  std::unique_ptr<zmq::socket_t> sock_srv;

  std::mutex mu;
  bool sync_connected=false;

  // Binding address and port
  std::string address_;
  int port_;
  std::string bind_to;

  // Client information
  int expected_clients_;
  std::vector<std::string> client_ids;

  // Statistics
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
  
  ZmqClient(std::string address, int port, const std::string& client_id) {
    addresses_.push_back(address);
    ports_.push_back(port);
    id_ = client_id;
    connect_tos.push_back(address + ":" + std::to_string(port));

    // Make socket client
    ctx_cli = std::make_unique<zmq::context_t>();
    sock_cli = std::make_unique<zmq::socket_t>(*ctx_cli, zmq::socket_type::sub);
    sock_cli->connect(connect_tos[0]);
    sock_cli->setsockopt(ZMQ_SUBSCRIBE, "", 0);
    memcpy_time.reserve(10000);
    delivery_time.reserve(10000);
  }

  ZmqClient(std::vector<std::string> addresses, std::vector<int> ports, const std::string& client_id) {
    // Connected to addresses and self information
    addresses_ = addresses;
    ports_ = ports;
    id_ = client_id;
    connect_tos.reserve(addresses_.size());
    for (int i = 0; i < addresses_.size(); ++i) {
      connect_tos.emplace_back(make_address_plus_port(addresses_[i], ports_[i]));
    }

    // Make a subscriber socket
    ctx_cli = std::make_unique<zmq::context_t>();
    sock_cli = std::make_unique<zmq::socket_t>(*ctx_cli, zmq::socket_type::sub);
    for (int i = 0; i < connect_tos.size();++i) {
      sock_cli->connect(connect_tos[i]);
    }
    sock_cli->setsockopt(ZMQ_SUBSCRIBE, "", 0);

    // Statitics
    memcpy_time.reserve(10000);
    delivery_time.reserve(10000);
  }

  // Connect to server
  bool sync_connection() {
    std::unique_lock<std::mutex> lk(mu);
    if (sync_connected)
      return false;

    // Synchonize to connect to all of the servers
    for (int i = 0; i < addresses_.size(); ++i) {
      // Build a server to initialize connection with every clients
      auto ctx_conn = std::make_unique<zmq::context_t>();
      auto sock_conn = std::make_unique<zmq::socket_t>(*ctx_conn, zmq::socket_type::req);

      std::string bind_to_conn = make_address_plus_port(addresses_[i], ports_[i] + 1);
      sock_conn->connect(bind_to_conn);

      zmq::message_t msg_conn((void*)(id_.data()), id_.length());
      sock_conn->send(msg_conn);
      zmq::message_t msg_reply;
      sock_conn->recv(&msg_reply);
      std::cout << "Client connected to " << bind_to_conn << std::endl;
    }
    sync_connected = true;
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
        sync_connected = false;
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
    bool valid_data = data->from_bytes((char*)(msg_cli.data()) + HEADER_OFFSET, data_size);
    std::cout << "Client memcpy time (us): " << timer.tock_count() << std::endl;
    memcpy_time.emplace_back(timer.tock_count());

    return valid_data;
  }

  // Start to work as client
  void start() {
  }

  void end_connection() {sync_connected = false;}

  long mean_delivery_time() {return get_average(delivery_time, "delivery");}

  long mean_memcpy_time() {return get_average(memcpy_time, "memcpy");}

 private:
  std::string id_;
  std::unique_ptr<zmq::context_t> ctx_cli;
  std::unique_ptr<zmq::socket_t> sock_cli;

  std::vector<std::string> addresses_;
  std::vector<int> ports_;
  std::vector<std::string> connect_tos;

  std::mutex mu;
  bool sync_connected=false;
  bool finished=false;
  TimerMicroSecconds timer;
  std::vector<long> memcpy_time;
  std::vector<long> delivery_time;
};

#endif
