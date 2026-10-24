@echo off
cd /d "%~dp0"
python pressure_sensor_host.py
if errorlevel 1 pause
