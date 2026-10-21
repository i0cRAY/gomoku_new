#!/usr/bin/env bash
# 從綠幕爆炸素材產生摧毀動畫的影格與音效（spec U13）。
# 用法：scripts/make_explosion_frames.sh <素材.mp4>
# 只取第一次爆炸（原片 30 fps 的第 6～49 格），去背後縮成 240×240 的 PNG，輸出到 resources/explosion/；
# 同一段的聲音（音效比畫面早一點開始）存成 resources/explosion.wav；最後重新產生 resources/explosion.qrc。
set -euo pipefail

src=${1:?用法：$0 <素材.mp4>}
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/resources/explosion"
firstFrame=6
lastFrame=49
keyColor=0x49FA00
size=240
audioStart=0.15
audioEnd=1.2
audioFadeStart=1.1

rm -f "$out"/explosion_*.png
mkdir -p "$out"
ffmpeg -v error -y -i "$src" \
    -vf "select='between(n,$firstFrame,$lastFrame)',chromakey=$keyColor:0.18:0.08,despill=type=green:mix=0.6:expand=0.1,scale=$size:$size:flags=lanczos,format=rgba" \
    -fps_mode passthrough -start_number 0 "$out/explosion_%02d.png"

ffmpeg -v error -y -i "$src" -vn -af "atrim=$audioStart:$audioEnd,afade=t=out:st=$audioFadeStart:d=0.1" \
    -ac 1 -ar 44100 -c:a pcm_s16le "$root/resources/explosion.wav"

{
    echo '<RCC>'
    echo '    <qresource prefix="/">'
    for f in "$out"/explosion_*.png; do
        echo "        <file>explosion/$(basename "$f")</file>"
    done
    echo '        <file>explosion.wav</file>'
    echo '    </qresource>'
    echo '</RCC>'
} > "$root/resources/explosion.qrc"

echo "產生 $(ls "$out"/explosion_*.png | wc -l) 張影格與 explosion.wav"
