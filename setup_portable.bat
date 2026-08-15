@echo off
REM ============================================
REM Q-POD MINI - CONFIGURADOR PORTÁTIL
REM Setup para Windows (incluye verificación de dependencias)
REM ============================================

setlocal enabledelayedexpansion

REM Obtener ruta actual (donde está el script)
set "SCRIPT_DIR=%~dp0"
set "PYTHON_CMD=python"

echo.
echo ======================================
echo   Q-POD MINI - INSTALADOR PORTÁTIL
echo ======================================
echo.

REM Verificar si Python está instalado
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python no está instalado o no está en el PATH
    echo.
    echo Soluciones:
    echo 1. Descarga e instala Python 3.8+:
    echo    https://www.python.org/downloads/
    echo    
    echo 2. Asegúrate de marcar "Add Python to PATH" durante la instalación
    echo.
    pause
    exit /b 1
)

REM Mostrar versión de Python
echo [OK] Python encontrado:
python --version
echo.

REM Verificar si pip está disponible
pip --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] pip no está disponible
    pause
    exit /b 1
)

echo [OK] pip encontrado
echo.

REM Instalar dependencias
echo Instalando dependencias necesarias...
echo.
pip install -r "%SCRIPT_DIR%requirements.txt"

if errorlevel 1 (
    echo [ERROR] Error al instalar dependencias
    pause
    exit /b 1
)

echo.
echo Regenerando recursos visuales (icono y logo)...
python "%SCRIPT_DIR%generate_assets.py"
if errorlevel 1 (
    echo [ADVERTENCIA] No se pudieron regenerar los recursos
    echo.
)

echo.
echo ======================================
echo [OK] ¡Instalación completada!
echo ======================================
echo.
echo Puedes ejecutar:
echo   - gui_qpod_config.py (Interfaz completa recomendada)
echo   - quickstart_config.py (Carga rápida)
echo.
echo.
pause
