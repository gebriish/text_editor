#!/bin/bash

mkdir -p ./bin

time clang++ main.cpp -o bin/editor -std=c++11 -O2 -g -lX11 -lGL -lXrandr -Wall
