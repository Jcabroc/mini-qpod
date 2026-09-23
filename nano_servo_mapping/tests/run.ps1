$ErrorActionPreference = 'Stop'
$testBinary = Join-Path ([IO.Path]::GetTempPath()) ('qpod-mapping-' + [Guid]::NewGuid().ToString() + '.exe')
python -m ziglang c++ -std=c++11 -O0 -nostdlib++ "$PSScriptRoot/test_core.cpp" -o $testBinary
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
& $testBinary
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
