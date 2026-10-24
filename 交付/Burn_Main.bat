@echo off
setlocal
title HTP100 Fixture - Flash Main
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_fixture.ps1" -Board Main %*
set "FLASH_RESULT=%ERRORLEVEL%"
echo.
if not "%FLASH_RESULT%"=="0" echo FLASH FAILED. Check the message and log above.
pause
exit /b %FLASH_RESULT%
