@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0bootstrap.ps1" -Mode Prompt %*
exit /b %ERRORLEVEL%
