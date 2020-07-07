// Copyright (c) 2013-2014 Sandstorm Development Group, Inc. and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "streamer.capnp.h"
#include <kj/debug.h>
#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <iostream>
#include <chrono>
#include <string>
#include <opencv2/opencv.hpp>

typedef unsigned int uint;

class StreamerImpl final: public Streamer::Server {

public:
  StreamerImpl(const std::string& video_file) {
    openVideo(video_file);
    setMat(640, 368);
  } 

  kj::Promise<void> evaluate(EvaluateContext context) override {
    
    auto params = context.getParams();
    auto img = context.getResults().getImg();
    bool ret = cap.read(mat);
    if (ret) {
      cv::resize(mat, mat, size);
      auto now = std::chrono::system_clock::now();
      auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
      long input_time = now_ms.time_since_epoch().count();
      img.setPixels(kj::ArrayPtr<unsigned char>(mat.data, num_pixels));
      img.setInputtime(input_time);
    }
    img.setIslast(!ret);

    return kj::READY_NOW;
  }
  
  bool openVideo(const std::string& video_file) {
    if (cap.isOpened())
      return false;
    cap.open(video_file);
    return cap.isOpened(); 
  }

  void setMat(size_t width, size_t height) {
    // 3-channel 8-bit image assumed
    mat = cv::Mat(height, width, CV_8UC3);
    num_pixels = width * height * 3;
    size.height = height;
    size.width = width;
  }

private:
  cv::VideoCapture cap;
  cv::Mat mat;
  cv::Size size;

  size_t width;
  size_t height;
  size_t num_pixels;
};

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " ADDRESS[:PORT]\n"
        "Runs the server bound to the given address/port.\n"
        "ADDRESS may be '*' to bind to all local addresses.\n"
        ":PORT may be omitted to choose a port automatically." << std::endl;
    return 1;
  }

  // Set up a server.
  std::string video("../../camera_detection_demo/data/deepstream_samplevideos/sample_1080p_h264.mp4");
  capnp::EzRpcServer server(kj::heap<StreamerImpl>(video), argv[1]);

  // Write the port number to stdout, in case it was chosen automatically.
  auto& waitScope = server.getWaitScope();
  uint port = server.getPort().wait(waitScope);
  if (port == 0) {
    // The address format "unix:/path/to/socket" opens a unix domain socket,
    // in which case the port will be zero.
    std::cout << "Listening on Unix socket..." << std::endl;
  } else {
    std::cout << "Listening on port " << port << "..." << std::endl;
  }

  // Run forever, accepting connections and handling requests.
  kj::NEVER_DONE.wait(waitScope);
}
