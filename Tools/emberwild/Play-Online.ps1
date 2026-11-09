param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9.-]+$')][string]$Server,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]{16,128}$')][string]$Room,
    [Parameter(Mandatory=$true)][ValidateSet('host','guest')][string]$Role,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$game = Join-Path (Split-Path -Parent $PSScriptRoot) 'Game.exe'
if (!(Test-Path -LiteralPath $game)) { throw 'Run this script from the exported game Online folder.' }
$relay = Join-Path $PSScriptRoot 'relay.py'
$logs = Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$stdout = Join-Path $logs 'relay-output.log'
$stderr = Join-Path $logs 'relay-error.log'
$helper = Start-Process -FilePath $Python -ArgumentList @('-u',('"'+$relay+'"'),'connect','--server',$Server,'--room',$Room,'--role',$Role) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
try {
    $ready = $false
    for ($attempt=0; $attempt -lt 100; $attempt++) {
        if ($helper.HasExited) { throw (Get-Content -LiteralPath $stderr -Raw) }
        if ((Test-Path -LiteralPath $stdout) -and ((Get-Content -LiteralPath $stdout -Raw) -match 'Relay connected')) { $ready=$true; break }
        Start-Sleep -Milliseconds 100
    }
    if (!$ready) { throw 'Relay did not connect within 10 seconds. Check the server and Saved/relay-error.log.' }
    Write-Host "Connected. In the game choose $(if ($Role -eq 'host') {'HOST CAMP'} else {'JOIN VIA RELAY'})."
    & $game
} finally {
    if (!$helper.HasExited) { Stop-Process -Id $helper.Id }
}

