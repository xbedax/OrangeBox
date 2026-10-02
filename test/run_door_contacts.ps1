$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\door-contacts"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "door_contacts.exe"
& g++ -std=c++17 "-I$root\test\stubs" "-I$root\include" "$root\test\door_contacts.cpp" "$root\src\gpio_hal.cpp" "$root\src\timer.cpp" -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
