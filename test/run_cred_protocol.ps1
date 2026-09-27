$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\cred-protocol"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "cred_protocol.exe"
$arduinoJson = Join-Path $root ".pio\libdeps\esp32-s3-n16r16\ArduinoJson\src"
& g++ -std=c++17 -DBOX_SIMULATION "-I$root\test\stubs" "-I$root\include" "-I$arduinoJson" `
    "$root\test\cred_protocol.cpp" "$root\src\cred_protocol.cpp" "$root\src\pass_store.cpp" -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
