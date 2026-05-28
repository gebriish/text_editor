#!/bin/bash

set -e

mkdir -p ./bin

clang++ entrypoint.cpp -o bin/editor -O0 -g3 -std=c++11 -lX11 -lGL -lXrandr
