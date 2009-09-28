@echo off
cd %~dp0
LlamaKISS.exe start
if errorlevel 5 echo Please "Run as Administrator"
pause
