$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root ".pio\build\state-machine-console"
$exe = Join-Path $outDir "state_machine_console_$PID.exe"


clear
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$sources = @(
    (Join-Path $root "test\state_machine_console.cpp"),
    (Join-Path $root "test\stubs\pass_store_fake.cpp"),
    (Join-Path $root "src\keyboard.cpp"),
    (Join-Path $root "src\state_machine.cpp"),
    (Join-Path $root "src\box_display.cpp"),
    (Join-Path $root "src\gpio_hal.cpp"),
    (Join-Path $root "src\timer.cpp")
)

& g++ -std=c++17 -DBOX_SIMULATION "-I$(Join-Path $root 'test\stubs')" "-I$(Join-Path $root 'include')" @sources -o $exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $exe
