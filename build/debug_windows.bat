@echo off

if not exist bin mkdir bin

cl ^
    /Fe:.\bin\editor.exe ^
    /Zi ^
    /Od ^
    /std:c++14 ^
    .\build\build.cpp ^
    gdi32.lib ^
    opengl32.lib ^
    /link ^
    /SUBSYSTEM:WINDOWS
