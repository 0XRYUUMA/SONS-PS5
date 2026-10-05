@echo off
cd /d "%~dp0"
set APS5_PRECOMPILE_PIPELINES=1
set APS5_FRAME_DIAGNOSTICS=1
echo Preparando os pipelines conhecidos antes de jogar...
echo Se a textura saltar ou o FPS oscilar, pressione F9 uma vez logo apos o evento.
echo Envie frame-diagnostics.log e game.err para analise.
"%~dp0SonsOfSparta-PS5.exe"
