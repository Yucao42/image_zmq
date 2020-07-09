INPUT_VIDEO=../sample_1080p_h264.mp4
BASE_PORT=8998
NUM_THREADS=1
STREAMING_CMD="ffmpeg -re -i ${INPUT_VIDEO} "
SIZE=640x368
LOSSLESS=""
if ! [[ -z ${LOSSLESS} ]];then
    ENCODING_FLAGS="-crf 0 -c:v libx264rgb "
else
    ENCODING_FLAGS=""
fi
    
OUTPUT_CMD="-vf scale=${SIZE} -movflags faststart ${ENCODING_FLAGS} -b 90000k -an -f mpegts tcp://localhost:"
CMD=$STREAMING_CMD

for THREAD_ID in `seq 1 $NUM_THREADS`;
do
    PORT=$(($BASE_PORT + $THREAD_ID))
    CMD="${CMD} ${OUTPUT_CMD}${PORT}?listen "
done

echo ${CMD}
${CMD}
