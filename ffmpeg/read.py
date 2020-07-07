import cv2
a = cv2.VideoCapture()
a.open('tcp://localhost:8999?client', cv2.CAP_FFMPEG)

print(a.isOpened())
ret = True
show_image = False
while(ret):
    ret, image = a.read()
    if ret:
        print("Read one frame successfully")
        if show_image:
            cv2.imshow('output', image)
            cv2.waitKey(1)
