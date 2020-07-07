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
#include <capnp/ez-rpc.h>
#include <kj/debug.h>
#include <math.h>
#include <string>
#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " HOST:PORT\n"
        "Connects to the Calculator server at the given address and "
        "does some RPCs." << std::endl;
    return 1;
  }

  capnp::EzRpcClient client(argv[1]);
  Streamer::Client streamer = client.getMain<Streamer>();

  // Keep an eye on `waitScope`.  Whenever you see it used is a place where we
  // stop and wait for the server to respond.  If a line of code does not use
  // `waitScope`, then it does not block!
  auto& waitScope = client.getWaitScope();

  size_t height = 368;
  size_t width = 640;
  size_t num_pixels = height * width * 3;
  std::cout << "Evaluating a request ";
  std::cout.flush();

  bool continue_ = true;
  int count = 0;
  long sum = 0;
  long time_passed;
  cv::Mat image(height, width, CV_8UC3);
  while (continue_) {
    // Set up the request.
    auto request = streamer.evaluateRequest();
    request.getReq().setWidth(width);
    request.getReq().setHeight(height);
    request.getReq().setChannels(3);

    // Send it, which returns a promise for the result (without blocking).
    auto evalPromise = request.send();

    auto response = evalPromise.wait(waitScope);

    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long cur_time = now_ms.time_since_epoch().count();


    auto img = response.getImg();
    continue_ = !img.getIslast();
    long input_time = img.getInputtime();
    time_passed = cur_time - input_time;
    std::cout << "Time passed (ns): " << time_passed<< std::endl;
    if (time_passed < 10000) {
      sum += time_passed;
      ++count;
    }

    // OpenCV does not allow wrap to const void*
    // It can not be const 
    // cv::Mat image(height, width, CV_8UC3, img.getPixels().begin());
    // memcpy(image.data, img.getPixels().begin(), num_pixels);
    // cv::imwrite("test.jpg", image);
  }
  std::cout << "Average delivery time(ns): " << sum * 1.0 / count << std::endl;
  return 0;
}
