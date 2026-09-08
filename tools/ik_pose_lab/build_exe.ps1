$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
python -m PyInstaller --noconfirm --onefile --windowed --name Mini-QPOD-Pose-Lab --paths $repoRoot --distpath "$PSScriptRoot/dist" --workpath "$PSScriptRoot/build" --specpath $PSScriptRoot "$PSScriptRoot/launch.py"
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller build failed' }
