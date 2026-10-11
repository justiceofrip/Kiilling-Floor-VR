[CmdletBinding()]
param(
 [string]$Game='F:\SteamLibrary\steamapps\common\KillingFloor',
 [ValidatePattern('^KF-[A-Za-z0-9_-]+$')][string]$Map='KF-Offices',
 [switch]$Unattended,
 [switch]$VRProfile,
 [switch]$GpuRendererTrial,
 [switch]$RendererTest,
 [ValidateRange(15,180)][int]$GpuTrialSeconds=180,
 [ValidateRange(60,240)][int]$VRFrameLimit=144,
 [switch]$MeleeTest,
 [switch]$HealTest,
 [switch]$DoorTest,
 [switch]$ArmTest,
 [switch]$NativeHands,
 [switch]$TraderTest,
 [switch]$GrenadeTest,
 [switch]$DualTest,
 [switch]$MK23Test,
 [switch]$SCARTest,
 [switch]$ReloadTimeTest,
 [switch]$PhysicalReloads
)
$ErrorActionPreference='Stop'
if($GpuRendererTrial -and (!$VRProfile -or !$Unattended)){throw 'GpuRendererTrial requires VRProfile and Unattended for its bounded standalone launch.'}
if($RendererTest -and (!$Unattended -or !$VRProfile -or $Map -cne 'KF-Offices')){throw 'RendererTest requires Unattended, VRProfile and Map exactly KF-Offices.'}
if($ArmTest -and $NativeHands){throw 'Choose NativeHands or the legacy ArmTest, not both.'}
if($GrenadeTest -and !$Unattended){throw 'GrenadeTest requires explicit Unattended mode.'}
if($TraderTest -and !$Unattended){throw 'TraderTest requires explicit Unattended mode.'}
if($ArmTest -and !$Unattended){throw 'ArmTest requires explicit Unattended mode.'}
if($DoorTest -and !$Unattended){throw 'DoorTest requires explicit Unattended mode.'}
if($HealTest -and !$Unattended){throw 'HealTest requires explicit Unattended mode.'}
if($DualTest -and !$Unattended){throw 'DualTest requires explicit Unattended mode.'}
if($SCARTest -and !$Unattended){throw 'SCARTest requires explicit Unattended mode.'}
if($MK23Test -and !$Unattended){throw 'MK23Test requires explicit Unattended mode.'}
if($ReloadTimeTest -and !$Unattended){throw 'ReloadTimeTest requires explicit Unattended mode.'}
if($PhysicalReloads -and !$Unattended){throw 'PhysicalReloads requires explicit Unattended mode.'}
if($MeleeTest -and !$Unattended){throw 'MeleeTest requires explicit Unattended mode.'}
$kfExe=[IO.Path]::GetFullPath((Join-Path $Game 'System\KillingFloor.exe'))
if(@(Get-Process KillingFloor -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $kfExe}).Count){throw 'Close the existing KF process before loading the development package.'}
if(!(Test-Path -LiteralPath (Join-Path $Game ('Maps\'+$Map+'.rom')))){throw 'Selected map is not installed.'}
$kfStageArgs=@('--game',$Game)
if($NativeHands){$kfStageArgs+='--native-hands'}
& py -3.12 (Join-Path $PSScriptRoot 'tools\stage_dev_package.py') @kfStageArgs
if($LASTEXITCODE -ne 0){throw 'Development package staging failed.'}
$kfUrl=$Map+'.rom?Game=KFMod.KFGameType?Mutator=KFVRGameplay.KFVRDevMutator?Difficulty=1'
if($Unattended){$kfUrl+='?KFVRUnattended=1'}
if($RendererTest){$kfUrl+='?KFVRRendererTest=1'}
if($MeleeTest){$kfUrl+='?KFVRMeleeTest=1'}
if($HealTest){$kfUrl+='?KFVRHealTest=1'}
if($DoorTest){$kfUrl+='?KFVRDoorTest=1'}
if($ArmTest){$kfUrl+='?KFVRArmTest=1'}
if($NativeHands){$kfUrl+='?KFVRNativeHands=1'}
if($TraderTest){$kfUrl+='?KFVRTraderTest=1'}
if($GrenadeTest){$kfUrl+='?KFVRGrenadeTest=1'}
if($DualTest){$kfUrl+='?KFVRDualTest=1'}
if($MK23Test){$kfUrl+='?KFVRMK23Test=1'}
if($SCARTest){$kfUrl+='?KFVRSCARTest=1'}
if($ReloadTimeTest){$kfUrl+='?KFVRReloadTimeTest=1'}
if($PhysicalReloads){$kfUrl+='?KFVRPhysicalReloads=1'}
$kfSteam=(Get-ItemProperty HKCU:\Software\Valve\Steam).SteamExe
$kfLaunchArgs=@('-applaunch','1250',$kfUrl,'-windowed')
if($VRProfile){
 $kfProfileOptions=@()
 if($RendererTest){$kfProfileOptions=@('--renderer-test')}
 $kfProfileJson=& py -3.12 (Join-Path $PSScriptRoot 'tools\prepare_vr_profile.py') --game $Game --output (Join-Path $PSScriptRoot 'local\play-profile') --max-fps $VRFrameLimit @kfProfileOptions
 if($LASTEXITCODE -ne 0){throw 'Separate VR configuration could not be prepared.'}
 $kfProfile=$kfProfileJson | ConvertFrom-Json
 # UE2's legacy INI parser retains Steam's closing quote; use unquoted paths.
 if($kfProfile.engine_ini -match '[\s"]' -or $kfProfile.user_ini -match '[\s"]'){throw 'The separate VR profile requires a project path without spaces or quotes.'}
 $kfLaunchArgs+=@(('ini='+$kfProfile.engine_ini),('userini='+$kfProfile.user_ini))
 Write-Host "Separate VR profile: desktop VSync off, $VRFrameLimit FPS limit."
}
if($VRProfile){
 # Steam's -applaunch re-quotes name=value tokens even with no spaces. The
 # legacy engine then treats the trailing quote as part of the INI filename.
 if($GpuRendererTrial){
  $kfGpuLauncher=Join-Path $PSScriptRoot 'build\x86\KFVRD3D9On12Launch.exe'
  $kfGpuBootstrap=Join-Path $PSScriptRoot 'build\x86\KFVRD3D9On12Bootstrap.dll'
  if(!(Test-Path -LiteralPath $kfGpuLauncher) -or !(Test-Path -LiteralPath $kfGpuBootstrap)){throw 'Build the x86 On12 launcher and bootstrap first.'}
  $kfGpuReports=Join-Path $PSScriptRoot ('reports\on12-launch-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
  New-Item -ItemType Directory -Path $kfGpuReports | Out-Null
  $kfGpuReport=Join-Path $kfGpuReports 'startup.json'
  $kfGpuArgs='--game "{0}" --dll "{1}" --url "{2}" --engine-ini "{3}" --user-ini "{4}" --report "{5}" --seconds {6}' -f ([IO.Path]::GetFullPath($Game).TrimEnd('\')),$kfGpuBootstrap,$kfUrl,$kfProfile.engine_ini,$kfProfile.user_ini,$kfGpuReport,$GpuTrialSeconds
  Start-Process -FilePath $kfGpuLauncher -ArgumentList $kfGpuArgs -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $kfGpuReports 'output.txt') -RedirectStandardError (Join-Path $kfGpuReports 'errors.txt')
  Write-Host "Optional GPU renderer trial: up to $GpuTrialSeconds seconds. Startup receipt: $kfGpuReport"
 }else{
 Start-Process -FilePath $kfExe -ArgumentList ($kfLaunchArgs[2..($kfLaunchArgs.Count-1)] -join ' ') -WorkingDirectory (Split-Path $kfExe) -WindowStyle Hidden
 }
}else{
 Start-Process -FilePath $kfSteam -ArgumentList $kfLaunchArgs -WindowStyle Hidden
}
Write-Host 'Started the standalone KFVR development match. Once spawned, run Play-KFVR.ps1 -Weapons.'
if($Unattended){Write-Host 'Unattended test mode: auto-ready, god mode, zed spawning held, progression marked as using cheats.'}
