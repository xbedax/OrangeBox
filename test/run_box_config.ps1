$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\tests\box-config"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "box_config.exe"
$arduinoJson = Join-Path $root ".pio\libdeps\esp32-s3-n16r16\ArduinoJson\src"
& g++ -std=c++17 -DBOX_SIMULATION "-I$root\test\stubs" "-I$root\include" "-I$arduinoJson" `
    -include "$root\test\stubs\qr_stream.h" `
    "$root\test\box_config.cpp" "$root\src\box_config.cpp" "$root\src\box_config_store.cpp" `
    "$root\src\provisioning_payload.cpp" "$root\src\qr_scanner.cpp" -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Push-Location $root
try { & $exe; $result = $LASTEXITCODE } finally { Pop-Location }
exit $result
