@echo off
setlocal
title HTP100 Combined - Flash Screen
echo HTP100 COMBINED firmware - update BOTH Main and Screen.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_combined_entry.ps1" -Board Screen %*
set "FLASH_RESULT=%ERRORLEVEL%"
echo.
if not "%FLASH_RESULT%"=="0" echo FLASH FAILED. Check the message and log above.
pause
exit /b %FLASH_RESULT%
