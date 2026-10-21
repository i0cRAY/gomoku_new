#!/usr/bin/env bash
# 從奶龍大笑素材產生霸道禁區的循環動畫與音效（spec U14）。
# 用法：scripts/make_laugh_frames.sh <素材.mp4>
# 取一個完整的大笑週期（9.8～13.73 秒，頭尾畫面幾乎相同，可以無縫循環），
# 裁掉底部地板、去掉白色背景，以 15 fps 縮成 96×96 的 PNG，輸出到 resources/laugh/；
# 同一段的聲音存成 resources/laugh.wav（頭尾各淡入淡出一點，循環時才不會爆音）；最後重新產生 resources/laugh.qrc。
set -euo pipefail

src=${1:?用法：$0 <素材.mp4>}
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/resources/laugh"
start=9.8
end=13.733
fadeOutStart=13.713
fadeSeconds=0.02
fps=15
keyColor=0xF0F2EF
size=96

rm -f "$out"/laugh_*.png
mkdir -p "$out"
ffmpeg -v error -y -i "$src" \
    -vf "trim=$start:$end,setpts=PTS-STARTPTS,fps=$fps,crop=720:720:0:0,colorkey=$keyColor:0.12:0.05,scale=$size:$size:flags=lanczos,format=rgba" \
    -start_number 0 "$out/laugh_%02d.png"

ffmpeg -v error -y -i "$src" -vn \
    -af "atrim=$start:$end,afade=t=in:st=$start:d=$fadeSeconds,afade=t=out:st=$fadeOutStart:d=$fadeSeconds" \
    -ac 1 -ar 44100 -c:a pcm_s16le "$root/resources/laugh.wav"

{
    echo '<RCC>'
    echo '    <qresource prefix="/">'
    for f in "$out"/laugh_*.png; do
        echo "        <file>laugh/$(basename "$f")</file>"
    done
    echo '        <file>laugh.wav</file>'
    echo '    </qresource>'
    echo '</RCC>'
} > "$root/resources/laugh.qrc"

echo "產生 $(ls "$out"/laugh_*.png | wc -l) 張影格與 laugh.wav"
