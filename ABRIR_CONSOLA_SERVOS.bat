@echo off
cd /d "%~dp0"
python -m tools.servo_mapping_console.console --port COM9 --baud 115200 --select 0
pause
