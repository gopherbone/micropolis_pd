#!/bin/sh
# Headless screenshot harness (macOS, Xcode clang): build the game against a
# fake Playdate API and run a script of button presses.
#   tools/harness/run.sh tools/harness/tour.txt [outdir]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SDK="${PLAYDATE_SDK_PATH:-$HOME/Developer/PlaydateSDK}"
XC=/Applications/Xcode.app/Contents/Developer
CC="${CC:-$XC/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang}"
SYSROOT="$XC/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
BUILD="$HERE/build"
mkdir -p "$BUILD"

# Rebuild raw assets when the prepared Source/assets are newer
if [ ! -f "$BUILD/raw.stamp" ] || [ -n "$(find "$ROOT/Source/assets" -newer "$BUILD/raw.stamp" -name '*.png' | head -1)" ]; then
  rm -rf "$BUILD/raw" && python3 "$HERE/to_raw.py" "$ROOT/Source" "$BUILD/raw" && touch "$BUILD/raw.stamp"
fi

SCRIPT="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
cd "$ROOT"
SRCS="$(ls engine/core/*.c engine/platform/*.c src/micropolis/*.c src/*.c | grep -v test_sim.c)"
# shellcheck disable=SC2086
"$CC" -isysroot "$SYSROOT" -std=gnu99 -O1 -g -w $EXTRA_CFLAGS -DPLAYDATE=1 -DTARGET_EXTENSION=1 \
  -I"$SDK/C_API" -Iengine/core -Iengine/platform -Isrc -Isrc/micropolis/headers \
  -o "$BUILD/harness" tools/harness/harness.c $SRCS -lm

OUT="${2:-$HERE/out}"
DATA="$BUILD/data"
rm -rf "$OUT" && mkdir -p "$OUT" "$DATA"
"$BUILD/harness" "$ROOT/Source" "$BUILD/raw" "$DATA" "$SCRIPT" "$OUT"
python3 "$HERE/to_png.py" "$OUT"
