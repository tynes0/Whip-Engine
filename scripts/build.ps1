#Requires -Version 5.1
param(
    [string]$Preset = 'vs2022',
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Config = 'Debug',
    [switch]$Bootstrap,
    [switch]$Yes,
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptRoot '..')
Set-Location $RepoRoot

if ($Bootstrap) {
    $bootstrapArgs = @('-ExecutionPolicy', 'Bypass', '-File', (Join-Path $ScriptRoot 'bootstrap.ps1'), '-Mode', 'Prompt')
    if ($Yes) { $bootstrapArgs += '-Yes' }
    & powershell @bootstrapArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Clean) {
    $buildDir = Join-Path $RepoRoot 'build'
    if (Test-Path $buildDir) {
        Write-Host "Removing $buildDir"
        Remove-Item -Recurse -Force $buildDir
    }
}

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cfgLower = $Config.ToLowerInvariant()
$buildPreset = "$Preset-$cfgLower"

# Main presets use names like vs2022-debug. Fallback to direct build when custom preset name is not present.
$presets = cmake --list-presets=build 2>$null
if ($presets -match [regex]::Escape($buildPreset)) {
    cmake --build --preset $buildPreset
} else {
    cmake --build "build/$Preset" --config $Config
}
exit $LASTEXITCODE
