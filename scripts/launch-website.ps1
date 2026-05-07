param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$website = Join-Path $root "website"
$launcher = Join-Path $website "launch.ps1"

if (-not (Test-Path -LiteralPath $launcher)) {
    throw "Website launcher was not found at $launcher"
}

& powershell -ExecutionPolicy Bypass -File $launcher
exit $LASTEXITCODE
