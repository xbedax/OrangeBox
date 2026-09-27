$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\timeout-rollover"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "timeout_rollover.exe"
& g++ -std=c++17 -DBOX_SIMULATION "-I$root\test\stubs" "-I$root\include" "$root\test\timeout_rollover.cpp" "$root\src\timer.cpp" -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$stateExe = Join-Path $outDir "state_timeout_rollover.exe"
& g++ -std=c++17 -DBOX_SIMULATION "-I$root\test\stubs" "-I$root\include" "$root\test\state_timeout_rollover.cpp" "$root\src\state_machine.cpp" "$root\src\timer.cpp" -o $stateExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $stateExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$qrExe = Join-Path $outDir "qr_timeout_rollover.exe"
& g++ -std=c++17 "-I$root\test\stubs" "-I$root\include" -include "$root\test\stubs\qr_stream.h" "$root\test\qr_timeout_rollover.cpp" "$root\src\qr_scanner.cpp" -o $qrExe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $qrExe
exit $LASTEXITCODE
