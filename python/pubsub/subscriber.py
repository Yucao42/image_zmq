import sys

import zmq
import numpy as np
import cv2

from zmq_service import image_client, image_server

if __name__ == "__main__":
    if len (sys.argv) != 3:
        print('usage: subscriber.py address port')
        address = "tcp://127.0.0.1"
        port = 50060
    else:
        address = sys.argv[1]
        port = sys.argv[2]

    server = image_client(address, port)
    server.streaming_read()
