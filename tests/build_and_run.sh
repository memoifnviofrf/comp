#!/usr/bin/env bash
# Linux 上でのテストのビルドと実行(検証用)。JUCE の場所は環境変数 JUCE_DIR、
# 事前にコンパイルした juce_core/juce_audio_basics/juce_dsp のオブジェクトは JUCE_OBJ_DIR にある前提。
set -e
S=${S:-/tmp/claude-0/-home-claude/145c5186-c216-50fb-a3bc-a31a7a66b872/scratchpad}
J=${JUCE_DIR:-$S/juce_src/JUCE}/modules
O=${JUCE_OBJ_DIR:-$S/obj}
HERE="$(cd "$(dirname "$0")" && pwd)"; SRC="$HERE/../Source"; B=$S/vc
mkdir -p "$B"
FLAGS="-std=c++17 -O2 -DNDEBUG -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -I$J -I$SRC -I$S/x11stub"
for f in "$SRC/CompressorDetector.cpp" "$SRC/SidechainProcessor.cpp" "$SRC/Metering.cpp" "$SRC/CompressorDSP.cpp" "$SRC/PresetManager.cpp" "$HERE/dsp_tests.cpp"; do
  g++ $FLAGS -Wall -Wextra -Wno-mismatched-new-delete -c "$f" -o "$B/$(basename "$f" .cpp).o" 2>&1 | grep -E "error|warning" || true &
done; wait
g++ -O2 -no-pie "$B"/*.o "$O/juce_core.o" "$O/juce_audio_basics.o" "$O/juce_dsp.o" -o "$B/dsp_tests" -Wl,--unresolved-symbols=ignore-all -lpthread -ldl 2>&1 | grep -v warning || true
"$B/dsp_tests" > "$B/report.md" || true
tail -12 "$B/report.md"
