$env:IDF_PATH='C:\Espressif\v5.4.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.4.4\venv'
$env:PATH='C:\Espressif\tools\python\v5.4.4\venv\Scripts;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;'+$env:PATH
$taskBuildSource='C:\Espressif\cam-usb-test'
New-Item -ItemType Directory -Force $taskBuildSource | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'CMakeLists.txt'),(Join-Path $PSScriptRoot 'sdkconfig.defaults') $taskBuildSource -Force
Copy-Item (Join-Path $PSScriptRoot 'main'),(Join-Path $PSScriptRoot 'components') $taskBuildSource -Recurse -Force
Push-Location $taskBuildSource
try {
    python C:\Espressif\v5.4.4\esp-idf\tools\idf.py -B C:\Espressif\cam-usb-test-build build
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
} finally { Pop-Location }
