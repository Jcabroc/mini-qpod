@echo off
cd /d "%~dp0\..\.."
py -3 -m tools.ik_walk_simulator.app
if errorlevel 1 pause
