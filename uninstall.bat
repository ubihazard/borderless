@echo off
cd /d "%~dp0"

:: Remove BORDERless directory after running this script
:: to completely uninstall it from your system.

set lnkpath=%APPDATA%\Microsoft\Windows\Start Menu\Programs\BORDERless.lnk
del /q /f "%lnkpath%"
