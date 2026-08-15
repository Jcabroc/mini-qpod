@echo off
REM ============================================
REM Q-POD MINI - INICIAR GUI
REM Launcher automático para gui_qpod_config.py
REM ============================================

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"

REM Verificar si Python está instalado
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python no está instalado
    echo.
    echo Ejecuta primero: setup_portable.bat
    echo.
    pause
    exit /b 1
)

REM Verificar si pyserial está instalado
python -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo [ADVERTENCIA] pyserial no está instalado
    echo.
    echo Instalando dependencias...
    pip install -r "%SCRIPT_DIR%requirements.txt"
    echo.
)

REM Ejecutar la GUI
python "%SCRIPT_DIR%gui_qpod_config.py"

if errorlevel 1 (
    echo.
    echo [ERROR] Hubo un problema al ejecutar la aplicación
    echo.
    pause
)
