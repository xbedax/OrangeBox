$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\tests\logging"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$arduinoJson = Join-Path $root ".pio\libdeps\esp32-s3-n16r16\ArduinoJson\src"
foreach ($minimum in @(1, 3)) {
    $exe = Join-Path $outDir "logging_$minimum.exe"
    & g++ -std=c++17 -DBOX_SIMULATION "-DLOGPRINT_MIN_SEVERITY=$minimum" `
        "-I$root\test\stubs" "-I$root\include" "-I$arduinoJson" `
        "$root\test\logging.cpp" "$root\src\logger.cpp" "$root\src\log_params.cpp" `
        "$root\src\log_messages.cpp" "$root\src\log_serialization.cpp" -o $exe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $logPath = Join-Path $outDir "logging_$minimum.txt"
    & $exe | Out-File -LiteralPath $logPath -Encoding utf8
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath $logPath
        exit $LASTEXITCODE
    }
    Get-Content -LiteralPath $logPath -Tail 1
}
