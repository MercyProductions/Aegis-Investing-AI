param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-Date -Format "yyyyMMdd-HHmmss"
}

& (Join-Path $PSScriptRoot "test.ps1")

$distRoot = Join-Path $root "dist"
$dist = Join-Path $distRoot "AegisStockBettingAI-$Version"
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Copy-Item (Join-Path $root "x64\Release\AegisStockBettingAI.exe") $dist -Force
Copy-Item (Join-Path $root "AegisStockBettingAI.config.ini") $dist -Force
Copy-Item (Join-Path $root "README.md") $dist -Force
Copy-Item (Join-Path $root "docs") (Join-Path $dist "docs") -Recurse -Force

$manifest = Join-Path $dist "release-manifest.txt"
@(
    "Aegis Stock Betting AI",
    "Version: $Version",
    "Built: $(Get-Date -Format s)",
    "Configuration: Release x64",
    "Research only. Paper mode default. No live trading module included."
) | Set-Content -Path $manifest -Encoding UTF8

Write-Host "Release artifact created: $dist"
