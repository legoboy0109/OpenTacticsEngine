<#
.SYNOPSIS
  Builds the UWP player (x64, Release, /MD, SDL2 backend, software 3D) and packs a test-signed .appx.

.DESCRIPTION
  One command, from a clean work folder (default C:\uwp):
    1. downloads the pinned SDL2 release source (sha256 checked) and builds it with its own VisualC-WinRT project,
    2. configures and builds the engine with CMake/Ninja for the "WindowsStore" system (UWP) in the same x64 UWP
       compiler environment,
    3. makes the placeholder logos, stages the package folder (program, SDL2.dll, assets, the three demo games,
       licences, manifest),
    4. creates a local TEST certificate if there is none (<Work>\cert, never committed), packs with makeappx and signs
       with signtool,
    5. copies the Visual C++ UWP framework package (the dependency) and the certificate's public part next to it.
  Result: <Work>\out\ with the .appx, the dependency .appx and the .cer. docs/XBOX.md tells how to install them.

  Needs: Visual Studio 2022 Build Tools with the C++ UWP build tools and the Windows 10/11 SDK (10.0.26100), curl and tar
  (both in Windows 10/11). The source tree is the one this script lives in (tools\uwp\..\..).

  Content: assets\, games\index.json and the explicit list demo-srpg, demo-economy, demo-hex. As in
  tools\windows\package.ps1 the script refuses to run when games\index.json lists anything else, so a private game can
  never end up in a package.

.PARAMETER Work
  Work folder for downloads, the SDL2 build, the engine build, the certificate and the output. Default C:\uwp.
.PARAMETER Version
  Package version (four numbers). Default 0.1.MMdd.HHmm of the build time: Windows refuses to replace an installed
  package whose version is the same but whose contents differ, and a newer version updates it in place and keeps its
  saves. (Numbers are at most 65535; MMdd and HHmm always fit.)
.PARAMETER IdentityName
  Package identity name. PLACEHOLDER: the owner picks the real one (docs/XBOX.md "Names").
.PARAMETER Publisher
  Publisher subject; must equal the signing certificate's subject. PLACEHOLDER, see above.
.PARAMETER SkipSdl
  Do not rebuild SDL2 when its DLL is already there.
#>
[CmdletBinding()]
param(
  [string]$Work = 'C:\uwp',
  [string]$Version = (Get-Date -Format '0.1.MMdd.HHmm'),
  [string]$IdentityName = 'OpenTacticsEngine.Player',
  [string]$Publisher = 'CN=OpenTacticsEngine Test',
  [string]$SdkVersion = '10.0.26100.0',
  [switch]$SkipSdl
)
$ErrorActionPreference = 'Stop'

$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$SdlVersion = '2.32.10'
$SdlSha256 = '12b2dc2eb8f2836100a7916b5d394a0c82f1f7e32693f95f98305403af242f08'  # SDL2-2.32.10.zip, from the release's published digest
$SdlUrl = "https://github.com/libsdl-org/SDL/releases/download/release-$SdlVersion/SDL2-$SdlVersion.zip"

# The games that ship (same rule as tools\windows\package.ps1).
$Games = @('demo-srpg', 'demo-economy', 'demo-hex')
$index = Get-Content -Raw (Join-Path $Repo 'games\index.json') | ConvertFrom-Json
if ((@($index.games) -join ',') -ne ($Games -join ',')) {
  throw "games\index.json lists '$(@($index.games) -join ', ')' but this script ships '$($Games -join ', ')'. Update both on purpose."
}

function Invoke-Checked {
  param([string]$What, [scriptblock]$Command)
  & $Command
  if ($LASTEXITCODE -ne 0) { throw "$What failed (exit code $LASTEXITCODE)." }
}

# --- the Visual C++ environment for x64 UWP (store libraries, WinRT metadata on LIBPATH) into this session ---
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = $null
if (Test-Path $vswhere) { $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
if (-not $vsPath) { $vsPath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools' }
$vcvarsall = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvarsall)) { throw "vcvarsall.bat not found under $vsPath." }
cmd /c "`"$vcvarsall`" x64 uwp $SdkVersion >nul && set" | ForEach-Object {
  $i = $_.IndexOf('=')
  if ($i -gt 0) { Set-Item -Path ("Env:" + $_.Substring(0, $i)) -Value $_.Substring($i + 1) }
}
$ext = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
foreach ($sub in 'CMake\bin', 'Ninja') { $p = Join-Path $ext $sub; if (Test-Path $p) { $env:PATH = "$p;$env:PATH" } }
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$kitBin = Join-Path $kits "bin\$SdkVersion\x64"
foreach ($t in 'makeappx.exe', 'signtool.exe') { if (-not (Test-Path (Join-Path $kitBin $t))) { throw "$t not found in $kitBin (install the Windows SDK $SdkVersion)." } }
$vclibs = Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx'
if (-not (Test-Path $vclibs)) { throw "VCLibs framework package not found: $vclibs (install the C++ UWP build tools)." }

New-Item -ItemType Directory -Force $Work | Out-Null
$Work = (Resolve-Path $Work).Path
$dl = Join-Path $Work 'dl'; $srcRoot = Join-Path $Work 'src'; $build = Join-Path $Work 'build'
$layout = Join-Path $Work 'layout'; $certDir = Join-Path $Work 'cert'; $out = Join-Path $Work 'out'
foreach ($d in $dl, $srcRoot, $certDir, $out) { New-Item -ItemType Directory -Force $d | Out-Null }

# --- 1. SDL2 for UWP -------------------------------------------------------------------------------------------------
$sdlSrc = Join-Path $srcRoot "SDL2-$SdlVersion"
$sdlLib = Join-Path $sdlSrc 'VisualC-WinRT\x64\Release\SDL-UWP'
if (-not (Test-Path (Join-Path $sdlSrc 'include\SDL.h'))) {
  $zip = Join-Path $dl "SDL2-$SdlVersion.zip"
  if (-not (Test-Path $zip)) { Write-Host "== download $SdlUrl"; Invoke-Checked 'download' { curl.exe -sSL -o $zip $SdlUrl } }
  $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
  if ($hash -ne $SdlSha256) { Remove-Item $zip; throw "SDL2 zip sha256 is $hash, expected $SdlSha256." }
  Invoke-Checked 'unpack' { tar -xf $zip -C $srcRoot }
}
if (-not ($SkipSdl -and (Test-Path (Join-Path $sdlLib 'SDL2.dll')))) {
  Write-Host '== build SDL2 (VisualC-WinRT, x64 Release, /MD)'
  # The project says v142 / SDK 10.0.16299: retarget on the command line (the source tree stays as released).
  Invoke-Checked 'SDL2 build' {
    & $msbuild (Join-Path $sdlSrc 'VisualC-WinRT\SDL-UWP.vcxproj') /p:Configuration=Release /p:Platform=x64 `
      /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=$SdkVersion /p:TargetPlatformVersion=$SdkVersion `
      /m /v:m /nologo
  }
}

# --- 2. the engine ---------------------------------------------------------------------------------------------------
Write-Host '== configure engine (WindowsStore / UWP, /MD)'
Invoke-Checked 'cmake configure' {
  cmake -S $Repo -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 `
    -DOTE_PLATFORM=sdl2 -DOTE_MSVC_STATIC_RUNTIME=OFF -DOTE_BUILD_TESTS=OFF -DOTE_BUILD_EDITOR=OFF `
    "-DOTE_UWP_SDL2_SOURCE=$sdlSrc" "-DOTE_UWP_SDL2_LIBDIR=$sdlLib"
}
Write-Host '== build ote-player'
Invoke-Checked 'cmake build' { cmake --build $build --target ote-player }
$exe = Join-Path $build 'engine\player\ote-player.exe'
if (-not (Test-Path $exe)) { throw "$exe was not built." }

# --- 3. stage the package folder -------------------------------------------------------------------------------------
Write-Host '== stage package folder'
if (Test-Path $layout) { Remove-Item -Recurse -Force $layout }
New-Item -ItemType Directory -Force $layout | Out-Null
Copy-Item $exe $layout
Copy-Item (Join-Path $sdlLib 'SDL2.dll') $layout
Copy-Item -Recurse (Join-Path $Repo 'assets') (Join-Path $layout 'assets')
New-Item -ItemType Directory -Force (Join-Path $layout 'games') | Out-Null
Copy-Item (Join-Path $Repo 'games\index.json') (Join-Path $layout 'games')
foreach ($g in $Games) {
  if (-not (Test-Path (Join-Path $Repo "games\$g\game.json"))) { throw "games\$g has no game.json." }
  Copy-Item -Recurse (Join-Path $Repo "games\$g") (Join-Path $layout "games\$g")
}
& (Join-Path $PSScriptRoot 'make-logos.ps1') -OutDir (Join-Path $layout 'Assets')
$lic = Join-Path $layout 'licenses'
New-Item -ItemType Directory -Force $lic | Out-Null
Copy-Item (Join-Path $Repo 'LICENSE') (Join-Path $lic 'GPL-3.0.txt')
Copy-Item (Join-Path $sdlSrc 'LICENSE.txt') (Join-Path $lic 'SDL2-zlib.txt')
$luaH = Get-Content (Join-Path $Repo 'third_party\lua\src\lua.h')
$start = ($luaH | Select-String -Pattern '^\* Copyright \(C\) 1994-' | Select-Object -First 1).LineNumber - 1
$end = ($luaH | Select-String -Pattern '^\*{20,}/' | Select-Object -Last 1).LineNumber - 1
($luaH[$start..$end] | ForEach-Object { $_.TrimStart('*').TrimStart(' ').TrimEnd('/').TrimEnd('*') }) |
  Set-Content -Encoding UTF8 (Join-Path $lic 'Lua-MIT.txt')
Copy-Item (Join-Path $Repo 'tools\windows\THIRD-PARTY-NOTICES.txt') $lic
Copy-Item (Join-Path $Repo 'games\demo-srpg\CREDITS.md') (Join-Path $lic 'demo-srpg-CREDITS.md')

# Last line of defence: nothing private in the package folder.
$bad = Get-ChildItem -Recurse $layout | Where-Object { $_.FullName -match 'vision-quest|practice-' }
if ($bad) { throw "private game content in the package folder: $($bad[0].FullName)" }

$manifest = (Get-Content -Raw (Join-Path $PSScriptRoot 'AppxManifest.xml.in')).
  Replace('@IDENTITY_NAME@', $IdentityName).Replace('@PUBLISHER@', $Publisher).Replace('@VERSION@', $Version)
[System.IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, (New-Object System.Text.UTF8Encoding($false)))

# --- 4. test certificate (made once; stays in <Work>\cert, never in the repository) ----------------------------------
$pfx = Join-Path $certDir 'ote-test.pfx'
$cer = Join-Path $certDir 'ote-test.cer'
$pwFile = Join-Path $certDir 'pfx-password.txt'
if (-not (Test-Path $pfx)) {
  Write-Host "== create the local test certificate ($Publisher) in $certDir"
  $cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -KeyUsage DigitalSignature `
    -FriendlyName 'OpenTacticsEngine UWP test certificate' -CertStoreLocation 'Cert:\CurrentUser\My' `
    -NotAfter (Get-Date).AddYears(3) `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
  $pw = [Convert]::ToBase64String((1..18 | ForEach-Object { Get-Random -Maximum 256 }) -as [byte[]])
  Set-Content -NoNewline -Encoding ascii $pwFile $pw
  Export-PfxCertificate -Cert $cert -FilePath $pfx -Password (ConvertTo-SecureString $pw -AsPlainText -Force) | Out-Null
  Export-Certificate -Cert $cert -FilePath $cer | Out-Null
  Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)"
}
$subject = (New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $cer).Subject
if ($subject -ne $Publisher) { throw "The certificate in $certDir has subject '$subject' but the manifest publisher is '$Publisher'. Delete the cert folder or pass -Publisher." }

# --- 5. pack and sign ------------------------------------------------------------------------------------------------
$pkgName = "OpenTacticsPlayer_${Version}_x64.appx"
$pkg = Join-Path $out $pkgName
Write-Host "== pack $pkgName"
Get-ChildItem $out -Filter 'OpenTacticsPlayer_*.appx' | Remove-Item  # older builds
Invoke-Checked 'makeappx' { & (Join-Path $kitBin 'makeappx.exe') pack /d $layout /p $pkg /o }
Write-Host '== sign (test certificate)'
$pw = Get-Content -Raw $pwFile
Invoke-Checked 'signtool' { & (Join-Path $kitBin 'signtool.exe') sign /fd SHA256 /f $pfx /p $pw $pkg }
Copy-Item $vclibs $out -Force
Copy-Item $cer $out -Force

Write-Host ''
Write-Host "== done: $out"
Get-ChildItem $out | ForEach-Object { Write-Host ("   {0}  {1:N1} MB" -f $_.Name, ($_.Length / 1MB)) }
Write-Host ("   {0} files in the package folder" -f (Get-ChildItem -Recurse -File $layout).Count)
