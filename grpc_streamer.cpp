#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <numeric>

#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <glog/logging.h>

#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <opencv2/opencv.hpp>

#include "timer.h"
#include "image.grpc.pb.h"
#include "image.pb.h"

#define NUM_TRANSMISSION_TEST (500)

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using image_proto::FrameData;
using image_proto::StreamRequest;
using image_proto::StreamService;


class StreamServiceImpl final : public StreamService::Service {
 public:
  StreamServiceImpl(const std::string& video_file) {
    cap = cv::VideoCapture(video_file);
    size.height = 368;
    size.width = 640;
  }

  Status VideoStreaming(ServerContext* context,
                      const StreamRequest* request,
                      ServerWriter<FrameData>* writer) override {
    cv::Mat image;
    TimerMicroSecconds timer;
    bool ret = cap.read(image);
    cv::resize(image, image, size);
    size_t num_pixels = 368 * 640 * 3;
    auto frame_bytes = framedata.mutable_image();
    int count = 0;
    frame_bytes->resize(num_pixels);
    // while (++count < NUM_TRANSMISSION_TEST) {
    while (ret) {
      cv::resize(image, image, size);
      std::cout << "Reading video time(ns): " << timer.tock_count() <<std::endl;
      memcpy((void*)(frame_bytes->data()), (void*)(image.data), num_pixels);
      framedata.set_time_point(get_time_since_epoch_count());
      writer->Write(framedata);
      
      timer.tick();
      bool ret = cap.read(image);
    }
    return Status::OK;
  }

 protected:
  // Image related
  cv::Size size;
  cv::VideoCapture cap;

  FrameData framedata;
};

class StreamServiceClient {
 public:
  StreamServiceClient() = default;

  StreamServiceClient(std::shared_ptr<Channel> channel)
      : stub_(StreamService::NewStub(channel)) {};

  void VideoStreaming(const StreamRequest& req,
                      FrameData& framedata) {
    ClientContext context;
    std::unique_ptr<ClientReader<FrameData> > reader(
        stub_->VideoStreaming(&context, req));

    // FIXME: currently hardcoded
    cv::Size img_size;
    img_size.height = 368;
    img_size.width = 640;
    size_t num_pixels = 368 * 640 * 3;

    cv::Mat dst(img_size, CV_8UC3, cv::Scalar::all(0));
    while (reader->Read(&framedata)) {
      cur_time = get_time_since_epoch_count();
      if (init_time == 0) {
        init_time = cur_time;
      } else {
        time_passed = cur_time - init_time;
        fps = (++total_received) * 1.0 / time_passed * 1000000;
        std::cout << cur_time - framedata.time_point() << "Received number of frames: " << total_received << " FPS: " << fps << std::endl;
      }
    }
    std::cout << "FINAL FPS " << fps << std::endl;
  }
 protected:
  std::unique_ptr<StreamService::Stub> stub_;
  long init_time = 0, cur_time, time_passed;
  int total_received = 0;
  double fps;
};

std::shared_ptr<Server> make_server(const std::string& server_addr,
                                    const std::string& video_file) {
  StreamServiceImpl* service = new StreamServiceImpl(video_file);
  ServerBuilder builder;
  builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(service);
  std::shared_ptr<Server> server(builder.BuildAndStart());
  return server;
}

int main(int argc, char** argv) {
  std::string video_file("../sample_1080p_h264.mp4");
  std::string server_addr("127.0.0.1:500051");
  auto server = make_server(server_addr, video_file);
  StreamServiceClient client(grpc::CreateChannel(
      server_addr, grpc::InsecureChannelCredentials()));
  StreamRequest req;
  FrameData framedata;
  client.VideoStreaming(req, framedata);
  return 0;
}
