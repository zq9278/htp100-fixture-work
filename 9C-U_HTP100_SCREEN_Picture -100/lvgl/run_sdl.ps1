$gcc = "C:/Program Files/JetBrains/CLion 262.4852.39/bin/mingw/bin/gcc.exe"
$gxx = "C:/Program Files/JetBrains/CLion 262.4852.39/bin/mingw/bin/g++.exe"

if (!(Test-Path $gcc) -or !(Test-Path $gxx)) {
    throw "MinGW toolchain not found at CLion bundled path."
}

$env:PATH = "C:\Program Files\JetBrains\CLion 262.4852.39\bin\mingw\bin;$env:PATH"

if (Test-Path build-sdl) {
    Remove-Item -Recurse -Force build-sdl
}

cmake -S . -B build-sdl -G Ninja -DCMAKE_C_COMPILER="$gcc" -DCMAKE_CXX_COMPILER="$gxx"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build-sdl
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Start-Process -FilePath ".\build-sdl\slk_sdl_sim.exe"
