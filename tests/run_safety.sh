#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build=$(mktemp -d /tmp/chiton-safety-XXXXXX)
trap 'rm -rf "$build"' EXIT
# SANITIZERS=thread selects a separate TSan run; no fast-math in either build.
${CXX:-g++} -std=c++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize="${SANITIZERS:-address,undefined}" -Iinclude \
  tests/http_framing.cpp -pthread -o "$build/http_framing"
"$build/http_framing"
${CXX:-g++} -std=c++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize="${SANITIZERS:-address,undefined}" -ffunction-sections -fdata-sections \
  -Iinclude -Wl,--gc-sections tests/safety.cpp src/webserver/webserver.cpp \
  src/graphics/images.cpp src/system/utils.cpp src/system/camera.cpp src/system/rover.cpp \
  -pthread -o "$build/safety"
"$build/safety"
${CXX:-g++} -std=c++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize="${SANITIZERS:-address,undefined}" -ffunction-sections -fdata-sections \
  -Iinclude -Wl,--gc-sections tests/measurements.cpp src/data.cpp \
  src/system/camera.cpp src/system/utils.cpp src/graphics/images.cpp \
  -pthread -o "$build/measurements"
"$build/measurements"
${CXX:-g++} -std=c++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize="${SANITIZERS:-address,undefined}" -ffunction-sections -fdata-sections \
  -Iinclude -Wl,--gc-sections tests/wall_bounds.cpp src/graphics/images.cpp -pthread -o "$build/wall_bounds"
"$build/wall_bounds"
${CXX:-g++} -std=c++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize="${SANITIZERS:-address,undefined}" -ffunction-sections -fdata-sections \
  -Iinclude -Wl,--gc-sections tests/web_correctness.cpp src/system/calibration_io.cpp \
  src/graphics/shapes.cpp src/system/utils.cpp src/webserver/webserver.cpp \
  -pthread -lz -o "$build/web_correctness"
"$build/web_correctness"
