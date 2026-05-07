$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Logs = Join-Path $Root "logs"
$Out = Join-Path $Logs "dev.out.log"
$Err = Join-Path $Logs "dev.err.log"

Write-Host ""
Write-Host "Auralith Markets launcher" -ForegroundColor Cyan
Write-Host "Project: $Root"
Write-Host ""

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "Node.js was not found on PATH."
}

if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
    throw "npm was not found on PATH."
}

if (-not (Test-Path $Logs)) {
    New-Item -ItemType Directory -Path $Logs | Out-Null
}

if (-not (Test-Path (Join-Path $Root ".env")) -and (Test-Path (Join-Path $Root ".env.example"))) {
    Copy-Item (Join-Path $Root ".env.example") (Join-Path $Root ".env")
}

if (-not (Test-Path (Join-Path $Root "node_modules"))) {
    Write-Host "Installing dependencies..."
    npm install --prefix $Root
}

$AllProcesses = Get-CimInstance Win32_Process
$RootProcesses = $AllProcesses | Where-Object {
    $_.Name -in @("node.exe", "cmd.exe", "powershell.exe") -and (
        $_.CommandLine -like "*$Root*" -or
        $_.CommandLine -like "*--port 5176*" -or
        ($_.CommandLine -like "*concurrently*" -and $_.CommandLine -like "*API,WEB*")
    )
}

$TargetIds = New-Object 'System.Collections.Generic.HashSet[int]'
function Add-ProcessTree([int] $ProcessId) {
    if ($TargetIds.Contains($ProcessId)) {
        return
    }
    [void] $TargetIds.Add($ProcessId)
    foreach ($Child in ($AllProcesses | Where-Object { $_.ParentProcessId -eq $ProcessId })) {
        Add-ProcessTree $Child.ProcessId
    }
}

foreach ($Process in $RootProcesses) {
    Add-ProcessTree $Process.ProcessId
}

$Existing = $AllProcesses | Where-Object {
    $TargetIds.Contains($_.ProcessId) -and
    $_.ProcessId -ne $PID -and
    $_.Name -in @("node.exe", "cmd.exe")
}

if ($Existing) {
    Write-Host "Stopping existing Auralith Markets node processes..."
    foreach ($Process in $Existing) {
        Stop-Process -Id $Process.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
}

Write-Host "Starting Auralith Markets..."
Start-Process -FilePath "powershell.exe" `
    -WorkingDirectory $Root `
    -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", "npm run dev") `
    -WindowStyle Hidden `
    -RedirectStandardOutput $Out `
    -RedirectStandardError $Err | Out-Null

$deadline = (Get-Date).AddSeconds(45)
do {
    try {
        Invoke-WebRequest -UseBasicParsing "http://127.0.0.1:5176" | Out-Null
        Invoke-WebRequest -UseBasicParsing "http://127.0.0.1:4280/api/health" | Out-Null
        Write-Host "Auralith Markets is ready at http://127.0.0.1:5176" -ForegroundColor Green
        Start-Process "http://127.0.0.1:5176"
        exit 0
    } catch {
        Start-Sleep -Milliseconds 800
    }
} while ((Get-Date) -lt $deadline)

Write-Warning "Auralith Markets did not become ready. Check $Out and $Err"
