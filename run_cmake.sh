#!/bin/sh

set -eu

cmake -S . -B build
cmake --build build -j
./build/pathtracing_2d
