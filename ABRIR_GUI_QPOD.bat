@echo off
REM ============================================
REM Q-POD MINI - ABRIR GUI DE CONFIGURACIÓN
REM ============================================

cd /d "f:\Documentos\Jota\jRobot\mini Q-Pod"

REM Instalar pyserial si no está instalado
pip install pyserial >nul 2>&1

REM Ejecutar la interfaz gráfica
python gui_qpod_config.py

pause
