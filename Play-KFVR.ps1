[CmdletBinding()]
param(
 [string]$Game='F:\SteamLibrary\steamapps\common\KillingFloor',
 [ValidateRange(25,100)][int]$Scale=100,
 [ValidateRange(15,180)][int]$Seconds=120,
 [ValidateRange(15,90)][int]$SnapDegrees=30,
 [switch]$Weapons,
 [switch]$Mirror
)
$ErrorActionPreference='Stop'
$kfExe=[IO.Path]::GetFullPath((Join-Path $Game 'System\KillingFloor.exe'))
$kfProcesses=@(Get-Process KillingFloor -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $kfExe })
if($kfProcesses.Count -ne 1){throw 'Start Killing Floor and join a solo match before launching KFVR.'}
$kfScript=Join-Path $PSScriptRoot 'tools\native_headset_trial.py'
Write-Host "KFVR: left-stick movement, $SnapDegrees degree right-stick snaps, $Scale% headset resolution, $Seconds seconds."
$kfWeaponArgs=@()
if($Weapons){$kfWeaponArgs=@('--weapons');Write-Host 'Tracked 9mm/dual 9mm/Deagle/Bullpup/knife/syringe/welder/grenade, capacitive hands and inventory holsters enabled; the KFVR development mutator must be loaded.';Write-Host 'Right A: use / open trader. In trader: point + trigger; A or B closes. Right B: eject magazine in physical mode. Left grip at chest: replacement magazine; guide it into the well. Left grip on pistol slide: pull and release. Left grip at rifle magazine: pull it out. Left X: cycle weapon. Left Y with 9mm/Deagle: physical reload mode. Left grip at Bullpup foregrip: support hand. Knife: left shoulder draw, then physical swings. Dual 9mm: each trigger fires its own pistol.';Write-Host 'Chest slots: syringe left, welder right. Syringe: touch chest and squeeze. Welder: right trigger at closed door; hold left trigger before pressing right to unweld.';Write-Host 'Grenade: rear-right waist draw, hold grip and press trigger to arm, release grip to throw. Trader includes grenade refills.'}
$kfRendererArgs=@()
if($Mirror){$kfRendererArgs=@('--suppress-desktop');Write-Host 'Experimental eye mirror enabled: successful stereo frames replace the extra desktop world render.'}
& py -3.12 $kfScript --pid $kfProcesses[0].Id --game $Game --movement --scale $Scale --seconds $Seconds --snap-degrees $SnapDegrees @kfWeaponArgs @kfRendererArgs
if($LASTEXITCODE -ne 0){throw 'KFVR session ended with an error. See the latest reports/native-headset and native-trace folders.'}
