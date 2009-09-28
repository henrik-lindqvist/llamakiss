@echo off
cd %~dp0
LlamaKISS.exe install
if errorlevel 5 echo Please "Run as Administrator"
pause
