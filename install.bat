@echo off
cd /d "%~dp0"

:: Installer script to automatically create
:: the Start menu shortcut for BORDERless.

set lnkhome=%~dp0
set lnkhome=%lnkhome:~0,-1%
set lnkpath=%APPDATA%\Microsoft\Windows\Start Menu\Programs\BORDERless.lnk
del /q /f "%lnkpath%" > nul

set lnkhome=%lnkhome:\=\\%
set lnkpath=%lnkpath:\=\\%
set jsepath=%TEMP%\borderless%RANDOM%%RANDOM%%RANDOM%.jse
echo var shell = WScript.CreateObject("WScript.Shell"); > "%jsepath%"
echo var lnk = shell.CreateShortcut("%lnkpath%"); >> "%jsepath%"
echo lnk.TargetPath = "%lnkhome%\\BORDERless.exe"; >> "%jsepath%"
echo lnk.WorkingDirectory = "%lnkhome%"; >> "%jsepath%"
echo lnk.Description = "BORDERless"; >> "%jsepath%"
echo lnk.IconLocation = "%lnkhome%\\BORDERless.exe"; >> "%jsepath%"
echo lnk.Save(); >> "%jsepath%"
cscript.exe //E:JScript //NoLogo //B "%jsepath%"
del /q /f "%jsepath%"
