<#
.SYNOPSIS
  Builds the Windows desktop player (Release, x64, MSVC) and packs OpenTacticsEngine-windows-x64.zip.

.DESCRIPTION
  Needs: Visual Studio 2022 (or its Build Tools) with the C++ workload, and the SDL3 development package for Visual C++
  ("SDL3-devel-<version>-VC.zip" from github.com/libsdl-org/SDL/releases) unpacked somewhere. Ninja and CMake come with the
  Build Tools (found automatically). Run from any folder:

    powershell -ExecutionPolicy Bypass -File tools\windows\package.ps1 -Sdl3Dir C:\work\sdl3\SDL3-3.4.20

  The zip holds: ote-player.exe, SDL3.dll, assets\, games\ (index.json and the three demo games listed in it: demo-srpg,
  demo-economy, demo-hex), licenses\, README.txt. Nothing else from games\ is ever copied (the owner's own games are
  private); the list of games is fixed in this script and checked against games\index.json.

  Runtime library: static (/MT) by default, so the zip runs on a clean Windows with nothing installed. -DynamicRuntime
  builds with /MD instead (what UWP needs); that variant is not meant for the zip (it needs the Visual C++
  redistributable) and gets a different file name.

.PARAMETER Sdl3Dir
  The unpacked SDL3 VC package (the folder holding cmake\, include\, lib\). Default: environment variable OTE_SDL3_DIR.
.PARAMETER BuildDir
  CMake build folder. Default: <repo>\build-windows-release (git-ignored).
.PARAMETER OutDir
  Where the staging folder and the zip are written. Default: <repo>\build-windows-release\package.
.PARAMETER DynamicRuntime
  Link the C++ runtime as a DLL (/MD) instead of statically (/MT).
#>
[CmdletBinding()]
param(
  [string]$Sdl3Dir = $env:OTE_SDL3_DIR,
  [string]$BuildDir,
  [string]$OutDir,
  [switch]$DynamicRuntime
)
$ErrorActionPreference = 'Stop'

$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDir) { $BuildDir = Join-Path $Repo 'build-windows-release' }
if (-not $OutDir) { $OutDir = Join-Path $BuildDir 'package' }
if (-not $Sdl3Dir) { throw 'Give -Sdl3Dir (the unpacked SDL3-devel-*-VC package) or set OTE_SDL3_DIR.' }
$Sdl3Dir = (Resolve-Path $Sdl3Dir).Path
if (-not (Test-Path (Join-Path $Sdl3Dir 'cmake\SDL3Config.cmake'))) { throw "$Sdl3Dir is not an SDL3 VC package (no cmake\SDL3Config.cmake)." }

# The games that ship. Must match games\index.json exactly, so a new game never lands in a download by accident.
$Games = @('demo-srpg', 'demo-economy', 'demo-hex')
$index = Get-Content -Raw (Join-Path $Repo 'games\index.json') | ConvertFrom-Json
if ((@($index.games) -join ',') -ne ($Games -join ',')) {
  throw "games\index.json lists '$(@($index.games) -join ', ')' but this script ships '$($Games -join ', ')'. Update both on purpose."
}

# --- the Visual C++ environment (x64) into this PowerShell session ---
function Import-VcEnvironment {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  $vsPath = $null
  if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  }
  if (-not $vsPath) { $vsPath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools' }
  $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
  if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found under $vsPath (install the 'Desktop development with C++' workload)." }
  cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    $i = $_.IndexOf('=')
    if ($i -gt 0) { Set-Item -Path ("Env:" + $_.Substring(0, $i)) -Value $_.Substring($i + 1) }
  }
  # CMake and Ninja that come with Visual Studio (vcvars adds them to PATH on newer versions; be sure).
  $ext = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
  foreach ($sub in 'CMake\bin', 'Ninja') {
    $p = Join-Path $ext $sub
    if (Test-Path $p) { $env:PATH = "$p;$env:PATH" }
  }
}
function Invoke-Checked {
  param([string]$What, [scriptblock]$Command)
  & $Command
  if ($LASTEXITCODE -ne 0) { throw "$What failed (exit code $LASTEXITCODE)." }
}

Import-VcEnvironment
$static = if ($DynamicRuntime) { 'OFF' } else { 'ON' }
$zipName = if ($DynamicRuntime) { 'OpenTacticsEngine-windows-x64-dynamic-runtime.zip' } else { 'OpenTacticsEngine-windows-x64.zip' }

Write-Host "== configure ($BuildDir, static runtime: $static)"
Invoke-Checked 'cmake configure' {
  cmake -S $Repo -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DSDL3_DIR=$Sdl3Dir\cmake" `
    "-DOTE_MSVC_STATIC_RUNTIME=$static" -DOTE_BUILD_TESTS=OFF -DOTE_BUILD_EDITOR=OFF
}
Write-Host '== build ote-player'
Invoke-Checked 'cmake build' { cmake --build $BuildDir --target ote-player }

# --- stage ---
$exe = Join-Path $BuildDir 'engine\player\ote-player.exe'
if (-not (Test-Path $exe)) { throw "$exe was not built." }
$name = 'OpenTacticsEngine-windows-x64'
$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage 'licenses') | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $Sdl3Dir 'lib\x64\SDL3.dll') $stage
Copy-Item -Recurse (Join-Path $Repo 'assets') (Join-Path $stage 'assets')
New-Item -ItemType Directory -Force (Join-Path $stage 'games') | Out-Null
Copy-Item (Join-Path $Repo 'games\index.json') (Join-Path $stage 'games')
foreach ($g in $Games) {
  if (-not (Test-Path (Join-Path $Repo "games\$g\game.json"))) { throw "games\$g has no game.json." }
  Copy-Item -Recurse (Join-Path $Repo "games\$g") (Join-Path $stage "games\$g")
}
Copy-Item (Join-Path $PSScriptRoot 'README.txt') $stage

# licences: GPL-3.0 (the engine), SDL (zlib), Lua (MIT, cut out of lua.h where the authors put it), notices for the rest.
$lic = Join-Path $stage 'licenses'
Copy-Item (Join-Path $Repo 'LICENSE') (Join-Path $lic 'GPL-3.0.txt')
Copy-Item (Join-Path $Sdl3Dir 'LICENSE.txt') (Join-Path $lic 'SDL3-zlib.txt')
$luaH = Get-Content (Join-Path $Repo 'third_party\lua\src\lua.h')
$start = ($luaH | Select-String -Pattern '^\* Copyright \(C\) 1994-' | Select-Object -First 1).LineNumber - 1
$end = ($luaH | Select-String -Pattern '^\*{20,}/' | Select-Object -Last 1).LineNumber - 1
($luaH[$start..$end] | ForEach-Object { $_.TrimStart('*').TrimStart(' ').TrimEnd('/').TrimEnd('*') }) |
  Set-Content -Encoding UTF8 (Join-Path $lic 'Lua-MIT.txt')
Copy-Item (Join-Path $PSScriptRoot 'THIRD-PARTY-NOTICES.txt') $lic
Copy-Item (Join-Path $Repo 'games\demo-srpg\CREDITS.md') (Join-Path $lic 'demo-srpg-CREDITS.md')

# --- zip (tar writes a proper zip with forward-slash entry names; Compress-Archive in Windows PowerShell 5.1 does not) ---
$zip = Join-Path $OutDir $zipName
if (Test-Path $zip) { Remove-Item -Force $zip }
Invoke-Checked 'zip' { tar -a -c -f $zip -C $OutDir $name }
$size = (Get-Item $zip).Length
Write-Host ("== {0}: {1:N1} MB, {2} files" -f $zip, ($size / 1MB), (Get-ChildItem -Recurse -File $stage).Count)
