@echo off
cd %~dp0
LlamaKISS.exe uninstall
if errorlevel 5 echo Please "Run as Administrator"
pause
