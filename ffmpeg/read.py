import cv2
a = cv2.VideoCapture()
a.open('tcp://localhost:8999?client', cv2.CAP_FFMPEG)

print(a.isOpened())
ret = True
show_image = False
cap = cv2.VideoCapture("../../camera_detection_demo/data/deepstream_samplevideos/sample_1080p_h264.mp4", cv2.CAP_FFMPEG)
while(ret):
    ret, image = a.read()
    r, i = cap.read()
    if ret:
        i = cv2.resize(i, (640, 368))
        print("Read one frame successfully",(i==image).mean() )
        if show_image:
            cv2.imshow('output', image)
            cv2.waitKey(1)
