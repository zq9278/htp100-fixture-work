@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_pressure.ps1" -Board Screen %*
set result=%errorlevel%
if not "%result%"=="0" echo Flash failed. See the message above.
pause
exit /b %result%
