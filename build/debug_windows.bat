@echo off

if not exist bin (
    mkdir bin
)

zig c++ .\build\build.cpp ^
    -o bin\editor.exe ^
    -O0 ^
    -g3 ^
    -std=c++11 ^
    -lopengl32 ^
    -lgdi32