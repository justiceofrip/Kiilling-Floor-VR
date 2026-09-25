[CmdletBinding()]
param([ValidateSet('x86','x64')][string]$Architecture='x86',[ValidateRange(1,16)][int]$Jobs=4)
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$false
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vsInstall){throw 'Visual Studio C++ tools are required.'}
& (Join-Path $vsInstall 'Common7\Tools\Launch-VsDevShell.ps1') -VsInstallationPath $vsInstall -Arch $Architecture -HostArch amd64 -SkipAutomaticLocation | Out-Null
$cmake=(Get-Command cmake -ErrorAction Stop).Source
$ctest=Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$build=Join-Path $PSScriptRoot "build\$Architecture"
$logs=Join-Path $PSScriptRoot 'reports'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
function Step([string]$Name,[string]$Exe,[string[]]$Arguments){
    $log=Join-Path $logs "$Architecture-$Name.log"
    & $Exe @Arguments *> $log
    if($LASTEXITCODE -ne 0){Get-Content -LiteralPath $log -Tail 80;throw "$Name failed ($LASTEXITCODE)"}
    Get-Content -LiteralPath $log -Tail 4
}
Step 'configure' $cmake @('-S',$PSScriptRoot,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DBUILD_TESTING=ON')
Step 'build' $cmake @('--build',$build,'--parallel',"$Jobs")
Step 'tests' $ctest @('--test-dir',$build,'--output-on-failure')
[ordered]@{architecture=$Architecture;completed_utc=[DateTime]::UtcNow.ToString('o');built=$true;tests_passed=$true;native_kf_verified=$false;headset_tested=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logs "$Architecture-build.json")