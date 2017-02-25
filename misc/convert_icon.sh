#!/bin/sh
set -e

inkscape -z -e _icon.png icon.svg >/dev/null
convert _icon.png icon.png  # reduce size

for n in 16 32 64 128; do
  rm -f icon_${s}.png
  convert icon.png -filter Lanczos -resize ${n}x${n} icon_${n}.png
done

gen_pngs="icon_16.png icon_32.png icon_64.png icon_128.png"
convert $gen_pngs icon.png icon.ico
rm $gen_pngs _icon.png
