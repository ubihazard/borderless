@echo off
cd /d "%~dp0"

:: Compile resources
rc borderless.rc > nul

:: Build the executable
clang -O2 -mwindows -municode %* borderless.c borderless.res -o borderless.exe -luser32 -lgdi32 -lshell32 -lole32 -Wno-deprecated-declarations

:: Embed manifest
mt -nologo -manifest borderless.exe.manifest -outputresource:"borderless.exe;1"
