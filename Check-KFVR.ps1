[CmdletBinding()]
param([string]$Game='F:\SteamLibrary\steamapps\common\KillingFloor')
$ErrorActionPreference='Stop'
& py -3.12 (Join-Path $PSScriptRoot 'tools\preflight.py') --game $Game
if($LASTEXITCODE -ne 0){throw 'KFVR prerequisites or file verification failed. See the failed checks above.'}
Write-Host 'KFVR files and supported game build verified. Headset presentation is checked when you start Play-KFVR.'
