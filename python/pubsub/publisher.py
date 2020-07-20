import sys

import zmq
import numpy as np
import cv2

from zmq_service import image_client, image_server

if __name__ == "__main__":
    if len (sys.argv) != 2:
        print('usage: publisher.py port')
        port = 50060
    else:
        port = int(sys.argv[1])

    address = "tcp://127.0.0.1"
    video_file = "../../one_sec.mp4"
    server = image_server(address, port, zmq.PUB, video_file)
    server.streaming()
