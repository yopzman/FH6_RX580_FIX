# Build script for CPU fix (version.dll proxy)
# Requires: Visual Studio 2022 Build Tools

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Root "build"
$Src = Join-Path $Root "src\VersionProxy.cpp"
$Def = Join-Path $Root "src\version_proxy.def"
$Out = Join-Path $OutDir "version.dll"
$Obj = Join-Path $OutDir "VersionProxy.obj"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$vswhereCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
)

$vcvars = $null
$vswhere = $vswhereCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $candidate) {
            $vcvars = $candidate
        }
    }
}

if ($vcvars) {
    cmd /c "`"$vcvars`" >nul && cl.exe /nologo /EHsc /std:c++17 /LD `"$Src`" /Fe:`"$Out`" /Fo:`"$Obj`" /link /DEF:`"$Def`""
    exit $LASTEXITCODE
}

if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    & cl.exe /nologo /EHsc /std:c++17 /LD $Src /Fe:$Out /Fo:$Obj /link /DEF:$Def
    exit $LASTEXITCODE
}

Write-Error "cl.exe not found. Install Visual Studio 2022 Build Tools."
