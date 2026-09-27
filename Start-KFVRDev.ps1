[CmdletBinding()]
param(
 [string]$Game='F:\SteamLibrary\steamapps\common\KillingFloor',
 [ValidatePattern('^KF-[A-Za-z0-9_-]+$')][string]$Map='KF-Offices',
 [switch]$Unattended,
 [switch]$MeleeTest
)
$ErrorActionPreference='Stop'
if($MeleeTest -and !$Unattended){throw 'MeleeTest requires explicit Unattended mode.'}
$kfExe=[IO.Path]::GetFullPath((Join-Path $Game 'System\KillingFloor.exe'))
if(@(Get-Process KillingFloor -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $kfExe}).Count){throw 'Close the existing KF process before loading the development package.'}
if(!(Test-Path -LiteralPath (Join-Path $Game ('Maps\'+$Map+'.rom')))){throw 'Selected map is not installed.'}
& py -3.12 (Join-Path $PSScriptRoot 'tools\stage_dev_package.py') --game $Game
if($LASTEXITCODE -ne 0){throw 'Development package staging failed.'}
$kfUrl=$Map+'.rom?Game=KFMod.KFGameType?Mutator=KFVRGameplay.KFVRDevMutator?Difficulty=1'
if($Unattended){$kfUrl+='?KFVRUnattended=1'}
if($MeleeTest){$kfUrl+='?KFVRMeleeTest=1'}
$kfSteam=(Get-ItemProperty HKCU:\Software\Valve\Steam).SteamExe
Start-Process -FilePath $kfSteam -ArgumentList @('-applaunch','1250',$kfUrl,'-windowed') -WindowStyle Hidden
Write-Host 'Started the standalone KFVR development match. Once spawned, run Play-KFVR.ps1 -Weapons.'
if($Unattended){Write-Host 'Unattended test mode: auto-ready, god mode, zed spawning held, progression marked as using cheats.'}
