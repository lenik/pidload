# SPDX-License-Identifier: AGPL-3.0-or-later
# Windows native Inno Setup build: <pkg>-<ver>.exe
# Shell: PowerShell (invoked by build.cmd).
#
# Usage (from this directory):
#   .\build.cmd
#   .\build.ps1 -Name pidload -Version 1.2.3
#
# Stages from ..\mingw\stage when present; otherwise meson install on Windows.

param(
    [string]$Name = "pidload",
    [string]$Version = "",
    [string]$SrcDir = ""
)

$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $SrcDir) {
    $SrcDir = (Resolve-Path (Join-Path $Here "..\..\..")).Path
}
$OutDir = Join-Path $Here "out"
$Stage = Join-Path $Here "stage"
$Iss = Join-Path $Here "setup.iss"
$MingwStage = Join-Path $Here "..\mingw\stage"

function Get-ProjectVersion {
    param([string]$Root)
    $zfr = Get-Command zfr -ErrorAction SilentlyContinue
    if ($zfr) {
        $v = & zfr version 2>$null
        if ($LASTEXITCODE -eq 0 -and $v) {
            return ($v -replace '^v', '').Trim()
        }
    }
    $vf = Join-Path $Root "VERSION"
    if (Test-Path $vf) {
        return ((Get-Content $vf -TotalCount 1) -replace '^v', '').Trim()
    }
    return "0.0.0"
}

if (-not $Version) {
    $Version = Get-ProjectVersion -Root $SrcDir
}
$Artifact = Join-Path $OutDir ("{0}-{1}.exe" -f $Name, $Version)

function Find-ISCC {
    $cmd = Get-Command iscc, ISCC -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        $env:INNOSETUP_DIR,
        "${env:ProgramFiles(x86)}\Inno Setup 6",
        "$env:ProgramFiles\Inno Setup 6",
        "${env:ProgramFiles(x86)}\Inno Setup 5",
        "$env:ProgramFiles\Inno Setup 5"
    ) | Where-Object { $_ }
    foreach ($d in $candidates) {
        $exe = Join-Path $d "ISCC.exe"
        if (Test-Path $exe) { return $exe }
    }
    throw "innosetup: ISCC.exe not found (install Inno Setup 6 or set INNOSETUP_DIR)"
}

Write-Host "innosetup: $Name $Version → $Artifact"

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$mingwReady = (Test-Path $MingwStage) -and (@(Get-ChildItem -Path $MingwStage -Recurse -File -ErrorAction SilentlyContinue).Count -gt 0)
if ($mingwReady) {
    Copy-Item -Path (Join-Path $MingwStage "*") -Destination $Stage -Recurse -Force
} else {
    $build = Join-Path $Here "build"
    if (Test-Path $build) { Remove-Item -Recurse -Force $build }
    Push-Location $SrcDir
    try {
        & meson setup $build --prefix=/ --buildtype=release
        if ($LASTEXITCODE -ne 0) { throw "meson setup failed" }
        & meson compile -C $build
        if ($LASTEXITCODE -ne 0) { throw "meson compile failed" }
        $env:DESTDIR = $Stage
        & meson install -C $build
        if ($LASTEXITCODE -ne 0) { throw "meson install failed" }
    } finally {
        Pop-Location
        Remove-Item Env:DESTDIR -ErrorAction SilentlyContinue
    }
}

$iscc = Find-ISCC
& $iscc $Iss "/DMyAppName=$Name" "/DMyAppVersion=$Version" "/DMyStage=$Stage"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
if (-not (Test-Path $Artifact)) {
    throw "innosetup: missing artifact $Artifact"
}
Write-Host $Artifact
