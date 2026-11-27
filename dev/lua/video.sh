#!/bin/sh
# Joins the raw files of record.lua into an MP4 (H.264 and AAC), the picture scaled 4 times with nearest-neighbour:  video.sh captures/intro [START_SECONDS [SECONDS]]
set -e
base=$1; start=${2:-0}; length=${3:-}
ffmpeg -y -hide_banner -loglevel error -f rawvideo -pixel_format rgb24 -video_size 320x200 -framerate 50 -i "$base.rgb" \
    -f s16le -ar 44100 -ac 2 -i "$base.pcm" -ss "$start" ${length:+-t "$length"} \
    -vf "scale=1280:800:flags=neighbor,format=yuv420p" -c:v libx264 -crf 18 -preset slow -c:a aac -b:a 160k -movflags +faststart "$base.mp4"
echo "$base.mp4"
