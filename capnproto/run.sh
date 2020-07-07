#! /usr/bin/env bash
#
# Quick script that compiles and runs the samples, then cleans up.
# Used for release testing.

set -exuo pipefail 
capnpc -oc++ streamer.capnp
c++ -std=c++14 -Wall streamer-client.c++ streamer.capnp.c++ \
    $(pkg-config --cflags --libs opencv) $(pkg-config --cflags --libs capnp-rpc) -o streamer-client
c++ -std=c++14 -Wall streamer-server.c++ streamer.capnp.c++ \
    $(pkg-config --cflags --libs opencv) $(pkg-config --cflags --libs capnp-rpc) -o streamer-server

./streamer-server "*:8090" &
sleep 1
./streamer-client "localhost:8090"
kill %+
