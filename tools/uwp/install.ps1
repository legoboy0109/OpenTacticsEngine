<#
.SYNOPSIS
  Installs the test-signed UWP package on THIS Windows 10/11 PC (to try the app on a desktop, no Xbox needed).

.DESCRIPTION
  Does three things, the first two once per PC and both need an administrator PowerShell:
    1. turns on "sideload apps" (registry AppModelUnlock, what Settings > For developers > Developer Mode sets),
    2. trusts the package's test certificate (it goes into LocalMachine\TrustedPeople: it can then vouch for packages signed
       with it, nothing else),
    3. Add-AppxPackage of the package with the Visual C++ framework package as its dependency.
  Run it from the logged-in desktop session: Windows refuses to install packages from a remote (ssh / network) session
  (error 0x80070005, "Failed to initialize PLM").

.PARAMETER OutDir
  The folder tools\uwp\build.ps1 wrote (the .appx, the dependency .appx, the .cer). Default C:\uwp\out.
#>
[CmdletBinding()]
param([string]$OutDir = 'C:\uwp\out')
$ErrorActionPreference = 'Stop'
$key = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock'
New-Item -Force $key | Out-Null
Set-ItemProperty $key -Name AllowDevelopmentWithoutDevLicense -Value 1 -Type DWord
Set-ItemProperty $key -Name AllowAllTrustedApps -Value 1 -Type DWord
Import-Certificate -FilePath (Join-Path $OutDir 'ote-test.cer') -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
$pkg = Get-ChildItem $OutDir -Filter 'OpenTacticsPlayer_*_x64.appx' | Sort-Object LastWriteTime | Select-Object -Last 1
$dep = Join-Path $OutDir 'Microsoft.VCLibs.x64.14.00.appx'
# Windows 10/11 PCs normally have the Visual C++ framework package already (the Store and other apps use it); installing
# the SDK's copy over it can fail with "apps need to be closed". Only bring ours when none that is new enough is there.
$have = Get-AppxPackage -Name 'Microsoft.VCLibs.140.00' | Where-Object { $_.Architecture -eq 'X64' -and $_.Version -ge [version]'14.0.30035.0' }

if ($have) { Add-AppxPackage -Path $pkg.FullName }
else { Add-AppxPackage -Path $pkg.FullName -DependencyPath $dep }
Get-AppxPackage -Name 'OpenTacticsEngine.Player' | Format-List Name, Version, PackageFullName, InstallLocation, PackageFamilyName
