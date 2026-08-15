@echo off
REM ============================================
REM Q-POD MINI - VERIFICAR SETUP
REM Diagnostico de la instalación portátil
REM ============================================

setlocal enabledelayedexpansion

echo.
echo ======================================
echo   Q-POD MINI - DIAGNÓSTICO
echo ======================================
echo.

REM Verificar Python
echo [1/4] Verificando Python...
python --version >nul 2>&1
if errorlevel 1 (
    echo  [X] Python NO está instalado
    echo  Solución: https://www.python.org/downloads/
    goto :ERROR
) else (
    for /f "tokens=*" %%i in ('python --version') do set "PYTHON_VER=%%i"
    echo  [OK] !PYTHON_VER!
)

echo.
echo [2/4] Verificando pip...
pip --version >nul 2>&1
if errorlevel 1 (
    echo  [X] pip NO está disponible
    goto :ERROR
) else (
    echo  [OK] pip disponible
)

echo.
echo [3/4] Verificando pyserial...
python -c "import serial; print('  [OK] pyserial ' + serial.__version__)" 2>nul
if errorlevel 1 (
    echo  [X] pyserial NO está instalado
    echo  Instalando...
    pip install pyserial
    if errorlevel 1 goto :ERROR
    echo  [OK] pyserial instalado
)

echo.
echo [4/4] Verificando archivos necesarios...
set "ALL_OK=1"

if not exist "gui_qpod_config.py" (
    echo  [X] Falta gui_qpod_config.py
    set "ALL_OK=0"
)
if not exist "default_config.json" (
    echo  [X] Falta default_config.json
    set "ALL_OK=0"
)
if not exist "requirements.txt" (
    echo  [X] Falta requirements.txt
    set "ALL_OK=0"
)

if !ALL_OK! equ 1 (
    echo  [OK] Todos los archivos presentes
) else (
    goto :ERROR
)

echo.
echo ======================================
echo [OK] ¡TODO ESTÁ BIEN!
echo ======================================
echo.
echo Puedes ejecutar:
echo   - INICIAR_GUI.bat
echo   - INICIAR_GUI_SILENCIOSO.vbs
echo.
pause
exit /b 0

:ERROR
echo.
echo ======================================
echo [X] HAY PROBLEMAS
echo ======================================
echo.
echo Lee el README_PORTÁTIL.md para más info
echo.
pause
exit /b 1
