param(
    [string]$SeedPath
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\pass-store-console"
$exe = Join-Path $outDir "pass_store_console_$PID.exe"
$arduinoJson = Join-Path $root ".pio\libdeps\esp32-c3-devkitm-1\ArduinoJson\src"

Clear-Host
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$sources = @(
    (Join-Path $root "test\pass_store_console.cpp"),
    (Join-Path $root "src\pass_store.cpp")
)

& g++ -std=c++17 -DBOX_SIMULATION "-I$(Join-Path $root 'test\stubs')" "-I$(Join-Path $root 'include')" "-I$arduinoJson" @sources -o $exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($SeedPath) {
    & $exe $SeedPath
} else {
    & $exe
}
