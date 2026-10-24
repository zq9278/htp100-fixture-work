param([switch]$BuildHost)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$driveRoot = [IO.Path]::GetPathRoot($workspace)
$buildRoot = Join-Path $driveRoot 'htp100-fixture-work'
if (Test-Path -LiteralPath $buildRoot) {
    $existing = Get-Item -LiteralPath $buildRoot
    if ($existing.LinkType -ne 'Junction' -or
        [IO.Path]::GetFullPath([string]$existing.Target).TrimEnd('\') -ne $workspace.TrimEnd('\')) {
        throw "Build junction points elsewhere: $buildRoot"
    }
} else {
    New-Item -ItemType Junction -Path $buildRoot -Target $workspace | Out-Null
}
$project = Join-Path $buildRoot '9C-U_HTP100_Main_vscode-200'
$screen = Join-Path $buildRoot '9C-U_HTP100_SCREEN_Picture -100'
$delivery = Join-Path $workspace '交付/压力传感器检测'
New-Item -ItemType Directory -Force -Path $delivery | Out-Null
Push-Location $driveRoot
try {
    & cmake -S $project -B "$project/build/PressureMonitor" -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DARM_NONE_EABI_GCC=$((Get-Command arm-none-eabi-gcc).Source)" "-DARM_NONE_EABI_GXX=$((Get-Command arm-none-eabi-g++).Source)" "-DARM_NONE_EABI_OBJCOPY=$((Get-Command arm-none-eabi-objcopy).Source)" "-DARM_NONE_EABI_SIZE=$((Get-Command arm-none-eabi-size).Source)"
    if ($LASTEXITCODE) { throw 'Pressure monitor configuration failed' }
    & cmake --build "$project/build/PressureMonitor" -j 8
    if ($LASTEXITCODE) { throw 'Pressure monitor build failed' }
    & cmake -S $screen -B "$screen/build/PressureMonitor" -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake'
    if ($LASTEXITCODE) { throw 'H7 configuration failed' }
    & cmake --build "$screen/build/PressureMonitor" -j 8
    if ($LASTEXITCODE) { throw 'H7 build failed' }
} finally { Pop-Location }
foreach ($extension in 'hex','bin','elf') {
    Copy-Item -LiteralPath "$project/build/PressureMonitor/HTP100-Pressure-Monitor.$extension" -Destination $delivery
    Copy-Item -LiteralPath "$screen/build/PressureMonitor/9C-HTP100-screen.$extension" -Destination "$delivery/HTP100-Pressure-H7-New.$extension"
    Copy-Item -LiteralPath "$screen/build/PressureMonitor/9C-HTP100-screen-old.$extension" -Destination "$delivery/HTP100-Pressure-H7-Old.$extension"
}
$flashSource = [IO.File]::ReadAllText((Join-Path $workspace '交付/flash_fixture.ps1'))
$flashSource = $flashSource.Replace('HTP100_Fixture_Main.hex','HTP100-Pressure-Monitor.hex').Replace('HTP100_Fixture_Screen_','HTP100-Pressure-H7-')
[IO.File]::WriteAllText((Join-Path $delivery 'flash_pressure.ps1'),$flashSource,[Text.UTF8Encoding]::new($true))
if ($BuildHost) {
    $hostDir = Join-Path $workspace 'tools/pressure_sensor_host'
    Push-Location $hostDir
    try {
        & python -m PyInstaller --noconfirm --clean --onefile --windowed --name HTP100_Pressure_Sensor_Host pressure_sensor_host.py
        if ($LASTEXITCODE) { throw 'Host packaging failed' }
        Copy-Item -LiteralPath "$hostDir/dist/HTP100_Pressure_Sensor_Host.exe" -Destination $delivery
    } finally { Pop-Location }
}
$artifactHashes = Get-ChildItem -File -Recurse -LiteralPath $delivery |
    Where-Object { $_.Extension -in '.hex','.bin','.elf','.exe','.png' } |
    Sort-Object Name | ForEach-Object {
        (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.FullName.Substring($delivery.Length+1).Replace('\','/')
    }
[IO.File]::WriteAllLines((Join-Path $delivery 'SHA256SUMS.txt'), [string[]]$artifactHashes, [Text.UTF8Encoding]::new($false))
Write-Host "Pressure monitor delivered to $delivery"
