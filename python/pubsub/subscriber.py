import sys

import zmq
import numpy as np
import cv2

from zmq_service import image_client, image_server

if __name__ == "__main__":
    if len (sys.argv) != 3:
        print('usage: subscriber.py address port')
        ports = [50060]
    else:
        ports = [int(port) for port in sys.argv[1:]]

    address = "tcp://127.0.0.1"
    server = image_client(address, ports)
    server.streaming_read()
