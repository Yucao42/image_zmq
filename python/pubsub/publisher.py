import sys

import zmq
import numpy as np
import cv2

from zmq_service import image_client, image_server

if __name__ == "__main__":
    if len (sys.argv) != 3:
        print('usage: publisher.py address port video_file')
        address = "tcp://127.0.0.1"
        port = 50060
        video_file = "../../one_sec.mp4"
    else:
        address = sys.argv[1]
        port = sys.argv[2]
        video_file = sys.argv[3]

    server = image_server(address, port, zmq.PUB, video_file)
    server.streaming()
