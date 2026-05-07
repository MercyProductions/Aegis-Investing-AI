$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

& (Join-Path $PSScriptRoot "build.ps1") -Configuration Release

$exe = Join-Path $root "x64\Release\AegisStockBettingAI.exe"
if (-not (Test-Path $exe)) {
    throw "Release executable not found: $exe"
}

$coreExe = Join-Path $root "x64\Release\AuralithCore.exe"
if (-not (Test-Path $coreExe)) {
    throw "Core executable not found: $coreExe"
}

& $exe --self-test
if ($LASTEXITCODE -ne 0) {
    throw "Self-test failed with exit code $LASTEXITCODE."
}

& $coreExe --self-test
if ($LASTEXITCODE -ne 0) {
    throw "Core self-test failed with exit code $LASTEXITCODE."
}

& $coreExe --health | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Core health check failed with exit code $LASTEXITCODE."
}

$p = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 4
if ($p.HasExited) {
    throw "Smoke test failed: app exited early with code $($p.ExitCode)."
}
Stop-Process -Id $p.Id
Write-Host "Self-test and smoke test passed."
