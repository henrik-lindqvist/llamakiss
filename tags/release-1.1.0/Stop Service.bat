@echo off
cd %~dp0
LlamaKISS.exe stop
if errorlevel 5 echo Please "Run as Administrator"
pause
