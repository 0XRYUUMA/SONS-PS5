@echo off
cd /d "%~dp0"
set APS5_FRAME_DIAGNOSTICS=1
start "" /wait "%~dp0SonsOfSparta-PS5.exe"
echo.
echo Relatorio: "%~dp0frame-diagnostics.log"
pause
