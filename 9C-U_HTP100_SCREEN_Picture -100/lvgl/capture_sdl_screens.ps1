$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $projectRoot 'build-sdl\slk_sdl_sim.exe'
$outputRoot = Join-Path $projectRoot 'captures'

$screens = @(
    'ui_Power_on_interface',
    'ui_home_page',
    'ui_hot_count',
    'ui_hotting',
    'ui_pulsation_set',
    'ui_auto_count',
    'ui_pulsationning',
    'ui_autoing',
    'ui_pulsation_set1',
    'ui_dev_model',
    'ui_count_model',
    'ui_test',
    'ui_setting',
    'ui_pre_setting',
    'ui_set_value',
    'ui_bee_setting',
    'ui_count',
    'ui_Factorymode'
)

$langs = @('zh', 'en')

Add-Type -AssemblyName System.Drawing

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

foreach($lang in $langs) {
    $langDir = Join-Path $outputRoot $lang
    New-Item -ItemType Directory -Force -Path $langDir | Out-Null

    foreach($screen in $screens) {
        Get-Process slk_sdl_sim -ErrorAction SilentlyContinue | Stop-Process -Force

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $exePath
        $psi.WorkingDirectory = (Split-Path $exePath -Parent)
        $psi.UseShellExecute = $false
        $psi.EnvironmentVariables['SLK_LANG'] = $lang
        $psi.EnvironmentVariables['SLK_SCREEN'] = $screen
        $bmpPath = Join-Path $langDir ($screen + '.bmp')
        $outPath = Join-Path $langDir ($screen + '.png')
        $psi.EnvironmentVariables['SLK_CAPTURE_BMP'] = $bmpPath
        $psi.EnvironmentVariables['SLK_CAPTURE_MS'] = '700'
        $proc = [System.Diagnostics.Process]::Start($psi)

        $proc.WaitForExit(5000) | Out-Null
        if(-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
            Write-Warning "$lang/$screen timed out before capture"
            continue
        }
        if(-not (Test-Path $bmpPath)) {
            Write-Warning "$lang/$screen exited before capture"
            continue
        }

        $bmp = [System.Drawing.Bitmap]::FromFile($bmpPath)
        $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Remove-Item $bmpPath -Force
    }
}
