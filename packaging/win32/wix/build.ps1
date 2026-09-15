# SPDX-License-Identifier: AGPL-3.0-or-later
# Windows native WiX MSI: <pkg>-<ver>.msi
# Shell: PowerShell (invoked by build.cmd).
#
# Usage:
#   .\build.cmd
#   .\build.ps1 -Name pidload -Version 1.2.3
#
# Prefers WiX 4 (`wix`), then WiX 3 (heat/candle/light).

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
$Wxs = Join-Path $Here "product.wxs"
$Files = Join-Path $Here "files.wxs"
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
$Artifact = Join-Path $OutDir ("{0}-{1}.msi" -f $Name, $Version)

Write-Host "wix: $Name $Version → $Artifact"

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

function Write-HarvestWxs {
    param([string]$StageDir, [string]$OutFile)
    $ns = "http://schemas.microsoft.com/wix/2006/wi"
    $comps = New-Object System.Collections.Generic.List[string]
    Get-ChildItem -Path $StageDir -Recurse -File | Sort-Object FullName | ForEach-Object {
        $rel = $_.FullName.Substring($StageDir.Length).TrimStart('\', '/')
        $relUnix = $rel -replace '\\', '/'
        $guidBytes = [System.Text.Encoding]::UTF8.GetBytes("wix:" + $relUnix)
        $md5 = [System.Security.Cryptography.MD5]::Create().ComputeHash($guidBytes)
        $cid = "c" + ([BitConverter]::ToString($md5) -replace '-', '').Substring(0, 16).ToLowerInvariant()
        $src = '$(var.StageDir)\' + ($rel -replace '/', '\')
        $comps.Add(@"
    <Component Id="$cid" Directory="INSTALLDIR" Guid="*">
      <File Id="${cid}f" Source="$src" KeyPath="yes" />
    </Component>
"@)
    }
    if ($comps.Count -eq 0) {
        $comps.Add(@"
    <Component Id="Placeholder" Directory="INSTALLDIR" Guid="*">
      <CreateFolder />
    </Component>
"@)
    }
    $body = ($comps -join "`n")
    @"
<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="$ns">
  <Fragment>
    <ComponentGroup Id="AppFiles">
$body
    </ComponentGroup>
  </Fragment>
</Wix>
"@ | Set-Content -Path $OutFile -Encoding UTF8
}

$heat = Get-Command heat -ErrorAction SilentlyContinue
if ($heat) {
    & heat dir $Stage -gg -sfrag -srd -cg AppFiles -dr INSTALLDIR `
        -var var.StageDir -out $Files
    if ($LASTEXITCODE -ne 0) { throw "heat failed" }
} else {
    Write-HarvestWxs -StageDir $Stage -OutFile $Files
}

$wix = Get-Command wix -ErrorAction SilentlyContinue
$candle = Get-Command candle -ErrorAction SilentlyContinue
$light = Get-Command light -ErrorAction SilentlyContinue

if ($wix) {
    & wix build -d "PkgName=$Name" -d "PkgVersion=$Version" `
        -d "StageDir=$Stage" -o $Artifact $Wxs $Files
    if ($LASTEXITCODE -ne 0) { throw "wix build failed" }
} elseif ($candle -and $light) {
    & candle "-dPkgName=$Name" "-dPkgVersion=$Version" `
        "-dStageDir=$Stage" -out "$Here\" $Wxs $Files
    if ($LASTEXITCODE -ne 0) { throw "candle failed" }
    & light -o $Artifact (Join-Path $Here "product.wixobj") (Join-Path $Here "files.wixobj")
    if ($LASTEXITCODE -ne 0) { throw "light failed" }
} else {
    throw "wix: need WiX 4 (wix) or WiX 3 (candle+light) on PATH"
}

if (-not (Test-Path $Artifact)) {
    throw "wix: missing artifact $Artifact"
}
Write-Host $Artifact
