[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Main','Screen')][string]$Board,
    [ValidateSet('New','Old')][string]$ScreenVariant,
    [ValidateRange(100,4000)][int]$Speed = 4000,
    [ValidatePattern('^\d+$')][string]$ProbeSerial,
    [string]$JLinkPath,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
try {
    if ($Board -eq 'Screen') {
        if (!$ScreenVariant) {
            Write-Host '请选择屏幕类型：'
            Write-Host '  1 - 新屏幕 (New)'
            Write-Host '  2 - 旧屏幕 (Old)'
            Write-Host '  Q - 退出'
            while (!$ScreenVariant) {
                $selection = Read-Host '输入 1 或 2'
                switch ($selection.Trim().ToUpperInvariant()) {
                    '1' { $ScreenVariant = 'New' }
                    '2' { $ScreenVariant = 'Old' }
                    'Q' { Write-Host '已退出，未烧录。'; exit 0 }
                    default { Write-Host '输入无效，请选择 1、2 或 Q。' }
                }
            }
        }
        $firmware = Join-Path $PSScriptRoot "HTP100_Fixture_Screen_$ScreenVariant.hex"
    } else {
        if ($ScreenVariant) { throw 'ScreenVariant is only valid for the Screen board.' }
        $firmware = Join-Path $PSScriptRoot 'HTP100_Fixture_Main.hex'
    }
    if (!(Test-Path -LiteralPath $firmware -PathType Leaf)) { throw "Firmware not found: $firmware" }
    $base = if ($Board -eq 'Main') { [uint64]0x08000000 } else { [Convert]::ToUInt64('90000000',16) }
    $limit = if ($Board -eq 'Main') { [uint64]0x08020000 } else { [Convert]::ToUInt64('91000000',16) }
    $device = if ($Board -eq 'Main') { 'STM32G070CB' } else {
        'STM32H750XB?BankAddr=0x90000000&Loader=CLK@PF10_nCS@PB6_D0@PF8_D1@PF9_D2@PF7_D3@PF6'
    }

    # Reject wrong-board images, damaged HEX records and accidental internal BOOT data.
    [uint64]$upper = 0
    $eof = $false
    $hasVector = $false
    $dataBytes = 0
    foreach ($line in [IO.File]::ReadLines($firmware)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($eof -or $line -notmatch '^:[0-9A-Fa-f]+$' -or ($line.Length % 2) -ne 1) { throw 'Malformed HEX file' }
        $count = [Convert]::ToInt32($line.Substring(1,2),16)
        if ($line.Length -ne (11 + 2*$count)) { throw 'HEX record length mismatch' }
        $sum = 0
        for ($i=1; $i -lt $line.Length; $i+=2) { $sum += [Convert]::ToInt32($line.Substring($i,2),16) }
        if (($sum -band 255) -ne 0) { throw 'HEX checksum mismatch' }
        $offset = [Convert]::ToInt32($line.Substring(3,4),16)
        $type = [Convert]::ToInt32($line.Substring(7,2),16)
        switch ($type) {
            0 {
                [uint64]$address = $upper + $offset
                if ($count -eq 0 -or $address -lt $base -or ($address+$count) -gt $limit) {
                    throw ('Wrong firmware address for {0}: 0x{1:X8}' -f $Board,$address)
                }
                if ($address -eq $base -and $count -ge 8) { $hasVector = $true }
                $dataBytes += $count
            }
            1 { if ($count -ne 0) { throw 'Invalid HEX EOF' }; $eof = $true }
            2 { if ($count -ne 2) { throw 'Invalid HEX segment address' }; $upper = [uint64][Convert]::ToInt32($line.Substring(9,4),16)*16 }
            4 { if ($count -ne 2) { throw 'Invalid HEX linear address' }; $upper = [uint64][Convert]::ToInt32($line.Substring(9,4),16)*65536 }
            3 { if ($count -ne 4) { throw 'Invalid HEX entry point' } }
            5 { if ($count -ne 4) { throw 'Invalid HEX entry point' } }
            default { throw "Unsupported HEX record: $type" }
        }
    }
    if (!$eof -or !$hasVector -or $dataBytes -eq 0) { throw 'HEX image is incomplete or lacks its vector table' }

    if (!$JLinkPath) { $JLinkPath = $env:JLINK_EXE }
    if (!$JLinkPath) {
        $found = Get-Command JLink.exe -ErrorAction SilentlyContinue
        if ($found) { $JLinkPath = $found.Source }
    }
    if (!$JLinkPath) {
        $candidates = foreach ($programRoot in @($env:ProgramFiles,${env:ProgramFiles(x86)})) {
            if (!$programRoot) { continue }
            Get-ChildItem -Path (Join-Path $programRoot 'SEGGER/JLink*/JLink.exe') -File -ErrorAction SilentlyContinue
        }
        $latest = $candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($latest) { $JLinkPath = $latest.FullName }
    }
    if (!$JLinkPath -or !(Test-Path -LiteralPath $JLinkPath -PathType Leaf)) { throw 'JLink.exe not found. Install SEGGER J-Link or pass -JLinkPath.' }

    # ASCII staging also supports firmware stored in a Chinese directory.
    $work = Join-Path ([IO.Path]::GetTempPath()) ('HTP100_Flash_' + [guid]::NewGuid().ToString('N'))
    if ($work -match '[^\x00-\x7F]') { throw 'TEMP path must be ASCII for J-Link command files. Set TEMP to an ASCII directory.' }
    New-Item -ItemType Directory -Path $work | Out-Null
    $staged = Join-Path $work 'firmware.hex'
    Copy-Item -LiteralPath $firmware -Destination $staged
    $commands = Join-Path $work 'program.jlink'
    # LoadFile erases only required sectors, programs and verifies by default.
    # No global erase/unlock/option-byte commands; screen internal BOOT is untouched.
    @('ExitOnError 1','r','h',('loadfile "{0}"' -f $staged),'r','g','exit') |
        Set-Content -LiteralPath $commands -Encoding ASCII
    $arguments = @('-Device',$device,'-If','SWD','-Speed',"$Speed",'-AutoConnect','1','-ExitOnError','1','-NoGui','1','-CommandFile',$commands)
    if ($ProbeSerial) { $arguments += @('-USB',$ProbeSerial) }
    Write-Host "Board: $Board / SWD ${Speed}kHz"
    if ($Board -eq 'Screen') { Write-Host "Screen variant: $ScreenVariant" }
    Write-Host "Firmware: $firmware ($dataBytes data bytes)"
    Write-Host "J-Link: $JLinkPath"
    Write-Host ('SHA256: ' + (Get-FileHash -LiteralPath $firmware -Algorithm SHA256).Hash)
    Write-Host "Device: $device"
    if ($DryRun) {
        Write-Host "DRY RUN OK - no probe connection or programming performed."
        Write-Host "Command file: $commands"
        Get-Content -LiteralPath $commands
        exit 0
    }

    $logDir = Join-Path $PSScriptRoot 'flash_logs'
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $logBoard = if ($Board -eq 'Screen') { "Screen_$ScreenVariant" } else { $Board }
    $log = Join-Path $logDir ($logBoard+'_'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'.log')
    Push-Location $work
    try {
        & $JLinkPath @arguments 2>&1 | Tee-Object -FilePath $log
        $result = $LASTEXITCODE
    } finally { Pop-Location }
    Write-Host "Log: $log"
    if ($result -ne 0) { throw "J-Link failed (exit $result). Check power, SWD/NRST wiring, probe and log." }
    Write-Host 'SUCCESS - programmed, verified and reset to run.' -ForegroundColor Green
    exit 0
} catch {
    Write-Host ('FAILED: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
