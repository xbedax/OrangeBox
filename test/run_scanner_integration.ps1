$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\tests\scanner-integration"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
foreach ($mode in @("consume", "keep")) {
    $exe = Join-Path $outDir "scanner_$mode.exe"
    $modeFlags = @()
    if ($mode -eq "keep") { $modeFlags += "-DTEST_SCAN_KEEP" }
    & g++ -std=c++17 -DBOX_SIMULATION @modeFlags "-I$root\test\stubs" "-I$root\include" `
        -include "$root\test\stubs\qr_stream.h" -include "$root\test\scanner_test_config.h" `
        "$root\test\scanner_integration.cpp" "$root\src\box_scanner.cpp" "$root\src\qr_scanner.cpp" `
        "$root\src\state_machine.cpp" "$root\src\keyboard.cpp" "$root\src\pass_store.cpp" "$root\src\timer.cpp" -o $exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
