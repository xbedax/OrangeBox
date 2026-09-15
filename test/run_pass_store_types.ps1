$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\pass-store-types"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "pass_store_types.exe"
& g++ -std=c++17 -DBOX_SIMULATION "-I$root\test\stubs" "-I$root\include" `
    "$root\test\pass_store_types.cpp" "$root\src\pass_store.cpp" -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
exit $LASTEXITCODE
