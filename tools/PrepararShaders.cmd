@echo off
cd /d "%~dp0"
set APS5_PRECOMPILE_PIPELINES=1
set APS5_FRAME_DIAGNOSTICS=1
echo Preparando os pipelines conhecidos antes de jogar...
"%~dp0SonsOfSparta-PS5.exe"
