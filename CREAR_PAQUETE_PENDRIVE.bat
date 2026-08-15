@echo off
REM ============================================
REM CREAR PAQUETE PORTATIL PARA PENDRIVE
REM ============================================

setlocal enabledelayedexpansion

set "OUTPUT_FOLDER=QPOD_PORTABLE"

if exist "%OUTPUT_FOLDER%" (
    echo.
    echo [!] La carpeta "%OUTPUT_FOLDER%" ya existe
    echo Eliminando carpeta anterior...
    rmdir /s /q "%OUTPUT_FOLDER%"
)

echo.
echo Creando paquete portatil en: %OUTPUT_FOLDER%
echo.

REM Crear carpeta
mkdir "%OUTPUT_FOLDER%"

REM Copiar archivos Python
copy /y "gui_qpod_config.py" "%OUTPUT_FOLDER%\" >nul
copy /y "quickstart_config.py" "%OUTPUT_FOLDER%\" >nul
copy /y "serial_protocol.cpp" "%OUTPUT_FOLDER%\" >nul

REM Copiar configuración
copy /y "default_config.json" "%OUTPUT_FOLDER%\" >nul
copy /y "*.json" "%OUTPUT_FOLDER%\" >nul 2>&1

REM Copiar scripts
copy /y "setup_portable.bat" "%OUTPUT_FOLDER%\" >nul
copy /y "INICIAR_GUI.bat" "%OUTPUT_FOLDER%\" >nul
copy /y "INICIAR_GUI_SILENCIOSO.vbs" "%OUTPUT_FOLDER%\" >nul
copy /y "VERIFICAR_SETUP.bat" "%OUTPUT_FOLDER%\" >nul
copy /y "requirements.txt" "%OUTPUT_FOLDER%\" >nul

REM Copiar documentación
copy /y "README_PORTÁTIL.md" "%OUTPUT_FOLDER%\" >nul
copy /y "LEE_PRIMERO.txt" "%OUTPUT_FOLDER%\" >nul
copy /y "QUICK_REFERENCE.md" "%OUTPUT_FOLDER%\" >nul

echo.
echo OK: Paquete creado exitosamente en: %OUTPUT_FOLDER%
echo.
echo Instrucciones:
echo   1. Abre la carpeta %OUTPUT_FOLDER%
echo   2. Lee LEE_PRIMERO.txt
echo   3. Copia esta carpeta a tu pendrive
echo   4. En otro PC: ejecuta setup_portable.bat
echo.
pause
