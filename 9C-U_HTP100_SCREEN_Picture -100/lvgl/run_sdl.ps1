param([switch]$ProductUi)
$ErrorActionPreference = 'Stop'
$gcc = (Get-Command gcc -ErrorAction SilentlyContinue).Source
if (!$gcc) {
    $gcc = Get-ChildItem 'C:/Program Files/JetBrains/CLion*/bin/mingw/bin/gcc.exe' | Select-Object -Last 1 -ExpandProperty FullName
}
if (!$gcc) { throw 'MinGW gcc not found. Add MinGW bin to PATH.' }
$env:PATH = (Split-Path $gcc) + ';' + $env:PATH
$build = Join-Path $PSScriptRoot 'build-combined-sdl'
$combined = if ($ProductUi) { 'OFF' } else { 'ON' }
cmake -S $PSScriptRoot -B $build -G Ninja "-DCMAKE_C_COMPILER=$gcc" "-DHTP_COMBINED_SIM=$combined"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $build -j 8
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Start-Process -FilePath (Join-Path $build 'slk_sdl_sim.exe') -WindowStyle Hidden
