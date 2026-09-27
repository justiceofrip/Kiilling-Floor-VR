[CmdletBinding()]
param(
 [string]$Game='F:\SteamLibrary\steamapps\common\KillingFloor',
 [ValidateRange(25,100)][int]$Scale=100,
 [ValidateRange(15,180)][int]$Seconds=120,
 [ValidateRange(15,90)][int]$SnapDegrees=30
)
$ErrorActionPreference='Stop'
$kfExe=[IO.Path]::GetFullPath((Join-Path $Game 'System\KillingFloor.exe'))
$kfProcesses=@(Get-Process KillingFloor -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $kfExe })
if($kfProcesses.Count -ne 1){throw 'Start Killing Floor and join a solo match before launching KFVR.'}
$kfScript=Join-Path $PSScriptRoot 'tools\native_headset_trial.py'
Write-Host "KFVR: left-stick movement, $SnapDegrees degree right-stick snaps, $Scale% headset resolution, $Seconds seconds."
& py -3.12 $kfScript --pid $kfProcesses[0].Id --game $Game --movement --scale $Scale --seconds $Seconds --snap-degrees $SnapDegrees
if($LASTEXITCODE -ne 0){throw 'KFVR session ended with an error. See the latest reports/native-headset and native-trace folders.'}
