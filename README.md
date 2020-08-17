# Image Zmq Transimission Experiments

Benchmarking image transmission using zmq and protobuf.

## Prerequisites

Installing zeromq and opencv and download sample test video. 

```bash
sudo apt install libzmq3-dev libopencv-dev
wget https://www.dropbox.com/s/pxi39ftper5jomb/sample_1080p_h264.mp4
```

Build [grpc](https://github.com/grpc/grpc/blob/master/BUILDING.md). 

## Build and run

```bash
mkdir build
cd build
cmake ..
make -j
./test_service
```
