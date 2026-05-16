#!/bin/bash

mkdir -p .bin/

clang++ main.cpp -o .bin/editor -O0 -lX11 -lGL -lXrandr -g  -std=c++11
