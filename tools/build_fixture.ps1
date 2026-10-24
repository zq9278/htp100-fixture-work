$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
# The bundled ST CMake crashes when its working directory contains Chinese.
# A junction provides an ASCII build path without copying the source tree.
$driveRoot = [IO.Path]::GetPathRoot($workspace)
$buildRoot = Join-Path $driveRoot 'htp100-fixture-work'
if (Test-Path -LiteralPath $buildRoot) {
    $existing = Get-Item -LiteralPath $buildRoot
    if ($existing.LinkType -ne 'Junction' -or
        [IO.Path]::GetFullPath([string]$existing.Target).TrimEnd('\') -ne $workspace.TrimEnd('\')) {
        throw "Build junction already exists and points elsewhere: $buildRoot"
    }
} else {
    New-Item -ItemType Junction -Path $buildRoot -Target $workspace | Out-Null
}
$gcc = (Get-Command arm-none-eabi-gcc).Source
$gxx = (Get-Command arm-none-eabi-g++).Source
$objcopy = (Get-Command arm-none-eabi-objcopy).Source
$size = (Get-Command arm-none-eabi-size).Source
$main = Join-Path $buildRoot '9C-U_HTP100_Main_vscode-200'
$screen = Join-Path $buildRoot '9C-U_HTP100_SCREEN_Picture -100'
Push-Location $driveRoot
try {
    & cmake -S $main -B "$main/build/Fixture" -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DARM_NONE_EABI_GCC=$gcc" "-DARM_NONE_EABI_GXX=$gxx" "-DARM_NONE_EABI_OBJCOPY=$objcopy" "-DARM_NONE_EABI_SIZE=$size"
    if ($LASTEXITCODE) { throw 'Main configuration failed' }
    & cmake --build "$main/build/Fixture" -j 8
    if ($LASTEXITCODE) { throw 'Main build failed' }
    & cmake -S $screen -B "$screen/build/Fixture" -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake'
    if ($LASTEXITCODE) { throw 'Screen configuration failed' }
    & cmake --build "$screen/build/Fixture" -j 8
    if ($LASTEXITCODE) { throw 'Screen build failed' }
} finally { Pop-Location }
$delivery = Join-Path $workspace '交付'
New-Item -ItemType Directory -Force -Path $delivery | Out-Null
foreach ($extension in 'hex','bin','elf') {
    Copy-Item -LiteralPath "$main/build/Fixture/9C-HTP100-main-v2.$extension" -Destination "$delivery/HTP100_Fixture_Main.$extension"
    Copy-Item -LiteralPath "$screen/build/Fixture/9C-HTP100-screen.$extension" -Destination "$delivery/HTP100_Fixture_Screen_New.$extension"
    Copy-Item -LiteralPath "$screen/build/Fixture/9C-HTP100-screen-old.$extension" -Destination "$delivery/HTP100_Fixture_Screen_Old.$extension"
    # Retire the ambiguous single-screen filename only after both copies succeed.
    $obsolete = Join-Path $delivery "HTP100_Fixture_Screen.$extension"
    if (Test-Path -LiteralPath $obsolete -PathType Leaf) { Remove-Item -LiteralPath $obsolete }
}
Write-Host "Fixture firmware delivered to $delivery"
