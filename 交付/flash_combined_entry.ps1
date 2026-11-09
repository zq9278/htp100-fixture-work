# Resolve the Unicode delivery path from the ASCII BAT entry points.
& (Join-Path $PSScriptRoot '综合工装/flash_combined.ps1') @args
exit $LASTEXITCODE
