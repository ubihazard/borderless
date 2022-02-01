@echo off
cd /d "%~dp0"

:: Compile resources
rc borderless.rc > nul

:: Build the executable
clang -O2 -m32 -mwindows -municode %* borderless.c borderless.res -o BORDERless.exe -luser32 -lgdi32 -lshell32 -lole32 -Wno-deprecated-declarations

:: Embed manifest
mt -nologo -manifest BORDERless.exe.manifest -outputresource:"BORDERless.exe;1"
