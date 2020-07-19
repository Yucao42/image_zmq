import zmq
import numpy as np
import cv2

class image_server:
    def __init__(self, addr, port, mode=zmq.PUB, video_file=""):
        self.addr = addr
        self.port = port
        self.mode = mode
        if video_file:
            self.cap = cv2.VideoCapture(video_file)
        else:
            self.cap = None

        self.ctx = zmq.Context.instance()
        self.socket = self.ctx.socket(mode)
        self.socket.bind(addr + ':' + str(self.port))

    def sync(self):
        ctx = zmq.Context.instance()
        s = ctx.socket(zmq.REP)
        s.bind(self.addr + ':' + str(self.port + 1))
        print("Waiting for subscriber to connect...")
        s.recv()
        print("   Done.")
        s.send(b'GO')

    def streaming(self, do_sync=True):
        if do_sync:
            self.sync()
        if self.cap and self.cap.isOpened():
            ret = True
            while(ret):
                ret, image = self.cap.read()
                if ret:
                    self.socket.send(image.tostring())
                    print("sending")

        self.socket.send(b"DONE")

class image_client:
    def __init__(self, addr, port, mode=zmq.SUB):
        self.addr = addr
        self.port = port
        self.mode = mode

        self.ctx = zmq.Context.instance()
        self.socket = self.ctx.socket(mode)
        self.socket.connect(addr + ':' + str(self.port))
        self.socket.setsockopt(zmq.SUBSCRIBE,b'')

    def sync(self):
        ctx = zmq.Context.instance()
        s = ctx.socket(zmq.REQ)
        s.connect(self.addr + ':' + str(self.port + 1))
        s.send(b'READY')
        s.recv()

    def streaming_read(self, do_sync=True):
        if do_sync:
            self.sync()
        reply = self.socket.recv()
        count = 0
        while reply != b"DONE":
            image = np.fromstring(reply, np.uint8)
            print("receiving ", image.shape)
            count += 1
            if count < 4:
                suc = cv2.imwrite(f"{count}.jpg", image)
                print(suc)
            reply = self.socket.recv()
