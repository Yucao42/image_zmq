# 
# ffmpeg -re -i ../sample_1080p_h264.mp4 \
#     -vf scale="640x368" -movflags faststart -b 90000k -f mpegts tcp://localhost:8999?listen \
#     -vf scale="640x368" -movflags faststart -b 90000k -f mpegts tcp://localhost:8999?listen 
# 
# ffmpeg -re -i ../sample_1080p_h264.mp4 \
#      -movflags faststart \
#      -vf scale="640x368"  \
#      -crf 17 \
#      -b:v 90000k \
#      -c:v libx264rgb \
#      -f mpegts \
#      tcp://localhost:8999?listen \
#     -vf scale="640x368" -movflags faststart -b 90000k -f mpegts tcp://localhost:8999?listen \
#   -vf scale="640x368" -movflags faststart -b 90000k -f mpegts tcp://localhost:9000?listen
ffmpeg -re -i ../sample_1080p_h264.mp4 \
    -vf scale="640x368" -movflags faststart -b:v 90000k -f mpegts tcp://localhost:8999?listen \
    -vf scale="640x368" -movflags faststart -b:v 90000k -f mpegts tcp://localhost:9000?listen

#      -c:v huffyuv \
#      -c:v libx264rgb \
