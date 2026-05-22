#Requires -Version 5.1
param(
    [ValidateSet('Off', 'CheckOnly', 'Prompt', 'Auto')]
    [string]$Mode = 'Prompt',

    [switch]$Yes,
    [switch]$FromCMake,

    [string]$DepsRoot = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Normalize-WhipPathString([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return '' }

    $p = $Path.Trim()

    # CMake / shell forwarding mistakes can occasionally leave literal quotes
    # in cache/env path values. System.IO.Path.GetFullPath treats those quotes
    # as invalid path characters on Windows, so strip only surrounding quotes.
    while ($p.Length -ge 2 -and (
        (($p.StartsWith('"')) -and ($p.EndsWith('"'))) -or
        (($p.StartsWith("'")) -and ($p.EndsWith("'")))
    )) {
        $p = $p.Substring(1, $p.Length - 2).Trim()
    }

    return $p
}

function Get-WhipFullPath([string]$Path) {
    $p = Normalize-WhipPathString $Path
    if ([string]::IsNullOrWhiteSpace($p)) { return '' }
    return [System.IO.Path]::GetFullPath($p)
}

function Test-WhipPathSyntax([string]$Path) {
    $p = Normalize-WhipPathString $Path
    if ([string]::IsNullOrWhiteSpace($p)) { return $false }

    try {
        [void][System.IO.Path]::GetFullPath($p)
        return $true
    } catch {
        return $false
    }
}

function Split-WhipPathCandidates([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return @() }

    # Env vars sometimes get written as a PATH-like list by mistake. In that
    # case try each entry rather than letting one malformed string kill bootstrap.
    return ($Path -split ';' | ForEach-Object { Normalize-WhipPathString $_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Get-WhipFullPath (Join-Path $ScriptRoot '..')
if ([string]::IsNullOrWhiteSpace($DepsRoot)) {
    $DepsRoot = Join-Path $RepoRoot '.whip/deps'
}
$DepsRoot = Get-WhipFullPath $DepsRoot
$EnvFile = Join-Path $DepsRoot 'deps-env.cmake'
$SummaryFile = Join-Path $DepsRoot 'deps-summary.json'

function Write-Section([string]$Text) {
    Write-Host ""
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Write-Ok([string]$Text) {
    Write-Host "[OK] $Text" -ForegroundColor Green
}

function Write-Warn([string]$Text) {
    Write-Host "[WARN] $Text" -ForegroundColor Yellow
}

function Write-Fail([string]$Text) {
    Write-Host "[ERROR] $Text" -ForegroundColor Red
}

function Convert-ToCMakePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return '' }
    return (Get-WhipFullPath $Path).Replace('\', '/')
}

function Write-FileIfChanged {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Content,
        [Parameter(Mandatory=$true)][System.Text.Encoding]$Encoding
    )

    if ((Test-Path $Path) -and ([System.IO.File]::ReadAllText($Path) -eq $Content)) {
        return $false
    }

    [System.IO.File]::WriteAllText($Path, $Content, $Encoding)
    return $true
}

function Add-PathCandidate([System.Collections.Generic.List[string]]$List, [string]$Path) {
    foreach ($candidate in (Split-WhipPathCandidates $Path)) {
        if (Test-WhipPathSyntax $candidate) {
            $List.Add($candidate)
        } else {
            Write-Warn "Ignoring invalid path candidate: $candidate"
        }
    }
}

function Confirm-Install([string]$Name) {
    if ($Mode -eq 'Auto' -or $Yes) { return $true }
    if ($Mode -eq 'CheckOnly' -or $Mode -eq 'Off') { return $false }

    Write-Host ""
    $answer = Read-Host "Install missing dependency '$Name'? [y/N]"
    return ($answer -match '^[Yy]')
}

function Test-Command([string]$Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Require-Winget {
    if (-not (Test-Command 'winget')) {
        throw "winget is required for automatic installs. Install 'App Installer' from Microsoft Store or install prerequisites manually."
    }
}

function Invoke-WingetInstall {
    param(
        [Parameter(Mandatory=$true)][string]$Id,
        [Parameter(Mandatory=$true)][string]$Name,
        [string[]]$ExtraArgs = @(),
        [string]$Override = ''
    )

    if (-not (Confirm-Install $Name)) {
        throw "$Name is missing and installation was not approved."
    }

    Require-Winget

    $args = @('install', '--id', $Id, '--exact', '--accept-source-agreements', '--accept-package-agreements')
    if ($ExtraArgs.Count -gt 0) { $args += $ExtraArgs }
    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        $args += @('--override', $Override)
    }

    Write-Host "winget $($args -join ' ')"
    & winget @args
    if ($LASTEXITCODE -ne 0) {
        throw "winget install failed for $Name ($Id). Exit code: $LASTEXITCODE"
    }
}

function Ensure-Command {
    param(
        [Parameter(Mandatory=$true)][string]$Command,
        [Parameter(Mandatory=$true)][string]$WingetId,
        [Parameter(Mandatory=$true)][string]$Name
    )

    if (Test-Command $Command) {
        Write-Ok "$Name found: $((Get-Command $Command).Source)"
        return
    }

    Invoke-WingetInstall -Id $WingetId -Name $Name

    if (-not (Test-Command $Command)) {
        Write-Warn "$Name was installed, but this shell cannot see '$Command' yet. Restart terminal and run again if CMake cannot find it."
    }
}

function Get-VSWherePath {
    $p = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path $p) { return $p }
    return ''
}

function Find-VisualStudioNativeTools {
    $vswhere = Get-VSWherePath
    if ([string]::IsNullOrWhiteSpace($vswhere)) { return '' }

    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($path)) {
        return $path.Trim()
    }
    return ''
}

function Test-VcpkgRoot([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path $Root)) { return $false }
    return (Test-Path (Join-Path $Root 'scripts/buildsystems/vcpkg.cmake'))
}

function Find-VcpkgRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    Add-PathCandidate $candidates $env:WHP_VCPKG_ROOT
    Add-PathCandidate $candidates $env:VCPKG_ROOT
    Add-PathCandidate $candidates (Join-Path $DepsRoot 'vcpkg')
    Add-PathCandidate $candidates (Join-Path $RepoRoot 'vendor/vcpkg')
    Add-PathCandidate $candidates 'C:\vcpkg'

    foreach ($c in $candidates) {
        if (Test-VcpkgRoot $c) { return Get-WhipFullPath $c }
    }
    return ''
}

function Bootstrap-Vcpkg([string]$Root) {
    $bootstrap = Join-Path $Root 'bootstrap-vcpkg.bat'
    if (-not (Test-Path $bootstrap)) {
        throw "vcpkg bootstrap script not found: $bootstrap"
    }

    Write-Host "Bootstrapping vcpkg: $bootstrap -disableMetrics"
    & $bootstrap -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg bootstrap failed. Exit code: $LASTEXITCODE"
    }
}

function Ensure-Vcpkg {
    $root = Find-VcpkgRoot
    if (-not [string]::IsNullOrWhiteSpace($root)) {
        $exe = Join-Path $root 'vcpkg.exe'
        if (-not (Test-Path $exe)) {
            Bootstrap-Vcpkg $root
        }
        Write-Ok "vcpkg found: $root"
        return $root
    }

    if (-not (Confirm-Install 'vcpkg package manager')) {
        throw 'vcpkg is missing and installation was not approved.'
    }

    if (-not (Test-Command 'git')) {
        Invoke-WingetInstall -Id 'Git.Git' -Name 'Git for Windows'
    }
    if (-not (Test-Command 'git')) {
        throw 'Git is required to clone vcpkg, but git was not found after installation.'
    }

    $root = Join-Path $DepsRoot 'vcpkg'
    New-Item -ItemType Directory -Force -Path $DepsRoot | Out-Null

    if (-not (Test-Path $root)) {
        Write-Host "git clone https://github.com/microsoft/vcpkg.git `"$root`""
        & git clone https://github.com/microsoft/vcpkg.git $root
        if ($LASTEXITCODE -ne 0) {
            throw "git clone failed for vcpkg. Exit code: $LASTEXITCODE"
        }
    }

    Bootstrap-Vcpkg $root
    Write-Ok "vcpkg found: $root"
    return Get-WhipFullPath $root
}


function Get-VcpkgExe([string]$Root) {
    $exe = Join-Path $Root 'vcpkg.exe'
    if (Test-Path $exe) { return $exe }
    return ''
}

function Test-VcpkgPackageInstalled {
    param(
        [Parameter(Mandatory=$true)][string]$VcpkgRoot,
        [Parameter(Mandatory=$true)][string]$Package,
        [Parameter(Mandatory=$true)][string]$Triplet
    )

    $installedRoot = Join-Path $VcpkgRoot ("installed/{0}" -f $Triplet)

    switch ($Package) {
        'freetype' {
            return ((Test-Path (Join-Path $installedRoot 'include/ft2build.h')) -and
                    ((Test-Path (Join-Path $installedRoot 'lib/freetype.lib')) -or
                     (Test-Path (Join-Path $installedRoot 'debug/lib/freetyped.lib')) -or
                     (Test-Path (Join-Path $installedRoot 'debug/lib/freetype.lib'))))
        }
        default {
            $packagesDir = Join-Path $VcpkgRoot 'packages'
            return [bool](Get-ChildItem $packagesDir -Directory -Filter ("{0}_*" -f $Package) -ErrorAction SilentlyContinue)
        }
    }
}

function Ensure-VcpkgPackage {
    param(
        [Parameter(Mandatory=$true)][string]$VcpkgRoot,
        [Parameter(Mandatory=$true)][string]$Package,
        [string]$Triplet = 'x64-windows'
    )

    $exe = Get-VcpkgExe $VcpkgRoot
    if ([string]::IsNullOrWhiteSpace($exe)) {
        Bootstrap-Vcpkg $VcpkgRoot
        $exe = Get-VcpkgExe $VcpkgRoot
    }
    if ([string]::IsNullOrWhiteSpace($exe)) {
        throw "vcpkg.exe was not found under $VcpkgRoot."
    }

    if (Test-VcpkgPackageInstalled -VcpkgRoot $VcpkgRoot -Package $Package -Triplet $Triplet) {
        Write-Ok "vcpkg package found: ${Package}:${Triplet}"
        return
    }

    if (-not (Confirm-Install "vcpkg package ${Package}:${Triplet}")) {
        throw "vcpkg package ${Package}:${Triplet} is missing and installation was not approved."
    }

    Write-Host "vcpkg install ${Package}:${Triplet} --clean-after-build"
    & $exe install "${Package}:${Triplet}" --clean-after-build
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg install failed for ${Package}:${Triplet}. Exit code: $LASTEXITCODE"
    }

    if (-not (Test-VcpkgPackageInstalled -VcpkgRoot $VcpkgRoot -Package $Package -Triplet $Triplet)) {
        Write-Warn "vcpkg reported success, but ${Package}:${Triplet} was not detected by the bootstrap sanity check. CMake may still find it through vcpkg."
    } else {
        Write-Ok "vcpkg package installed: ${Package}:${Triplet}"
    }
}

function Ensure-VcpkgPackages {
    param(
        [Parameter(Mandatory=$true)][string]$VcpkgRoot
    )

    # msdf-atlas-gen/msdfgen calls find_package(Freetype). Since it is pulled
    # as source with FetchContent, vcpkg will not magically have Freetype unless
    # we install it first or use manifest mode. Bootstrap owns host setup, so do
    # the explicit classic-mode install here.
    $triplet = 'x64-windows'
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_DEFAULT_TRIPLET)) {
        $triplet = $env:VCPKG_DEFAULT_TRIPLET
    }

    Ensure-VcpkgPackage -VcpkgRoot $VcpkgRoot -Package 'freetype' -Triplet $triplet
}

function Ensure-VisualStudioBuildTools {
    $vs = Find-VisualStudioNativeTools
    if (-not [string]::IsNullOrWhiteSpace($vs)) {
        Write-Ok "Visual Studio C++ toolchain found: $vs"
        return
    }

    $override = '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Workload.ManagedDesktopBuildTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.Net.Component.4.7.2.TargetingPack --add Microsoft.Net.Component.4.7.2.SDK --includeRecommended'
    Invoke-WingetInstall -Id 'Microsoft.VisualStudio.2022.BuildTools' -Name 'Visual Studio 2022 Build Tools + C++/.NET Framework 4.7.2 workloads' -Override $override

    $vs = Find-VisualStudioNativeTools
    if ([string]::IsNullOrWhiteSpace($vs)) {
        Write-Warn "Visual Studio Build Tools installation finished, but vswhere cannot see C++ tools yet. Restart terminal or Visual Studio Installer if configure fails."
    } else {
        Write-Ok "Visual Studio C++ toolchain found: $vs"
    }
}

function Find-VulkanRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    Add-PathCandidate $candidates $env:WHP_VULKAN_SDK_ROOT
    Add-PathCandidate $candidates $env:VULKAN_SDK

    $defaultRoot = 'C:\VulkanSDK'
    if (Test-Path $defaultRoot) {
        Get-ChildItem $defaultRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Add-PathCandidate $candidates $_.FullName }
    }

    foreach ($c in $candidates) {
        if ([string]::IsNullOrWhiteSpace($c)) { continue }
        $include = Join-Path $c 'Include/shaderc/shaderc.hpp'
        $spirv = Join-Path $c 'Include/spirv_cross/spirv_cross.hpp'
        $libDir = Join-Path $c 'Lib'
        if ((Test-Path $include) -and (Test-Path $spirv) -and (Test-Path $libDir)) {
            return Get-WhipFullPath $c
        }
    }
    return ''
}

function Ensure-VulkanSdk {
    $vk = Find-VulkanRoot
    if (-not [string]::IsNullOrWhiteSpace($vk)) {
        Write-Ok "Vulkan SDK found: $vk"
        return $vk
    }

    Invoke-WingetInstall -Id 'KhronosGroup.VulkanSDK' -Name 'Vulkan SDK'

    $vk = Find-VulkanRoot
    if ([string]::IsNullOrWhiteSpace($vk)) {
        throw "Vulkan SDK installation finished, but it was not detected. Restart terminal or set WHP_VULKAN_SDK_ROOT/VULKAN_SDK."
    }
    Write-Ok "Vulkan SDK found: $vk"
    return $vk
}

function Test-MonoRoot([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path $Root)) { return $false }
    return ((Test-Path (Join-Path $Root 'include/mono/jit/jit.h')) -or (Test-Path (Join-Path $Root 'include/mono-2.0/mono/jit/jit.h')))
}

function Find-MonoRoot {
    $candidates = [System.Collections.Generic.List[string]]::new()
    Add-PathCandidate $candidates $env:WHP_MONO_ROOT
    Add-PathCandidate $candidates (Join-Path $RepoRoot 'Whip/vendor/mono')
    Add-PathCandidate $candidates 'C:\Program Files\Mono'
    Add-PathCandidate $candidates 'C:\Program Files (x86)\Mono'

    foreach ($c in $candidates) {
        if (Test-MonoRoot $c) { return Get-WhipFullPath $c }
    }
    return ''
}

function Resolve-MonoLayout([string]$Root) {
    $include = ''
    if (Test-Path (Join-Path $Root 'include/mono/jit/jit.h')) {
        $include = Join-Path $Root 'include'
    } elseif (Test-Path (Join-Path $Root 'include/mono-2.0/mono/jit/jit.h')) {
        $include = Join-Path $Root 'include/mono-2.0'
    }

    $libCandidates = @(
        (Join-Path $Root 'lib/Debug/libmono-static-sgen.lib'),
        (Join-Path $Root 'lib/Release/libmono-static-sgen.lib'),
        (Join-Path $Root 'lib/libmono-static-sgen.lib'),
        (Join-Path $Root 'lib/mono-2.0-sgen.lib'),
        (Join-Path $Root 'lib/libmono-2.0-sgen.lib'),
        (Join-Path $Root 'lib/mono-2.0.lib'),
        (Join-Path $Root 'lib/libmono-2.0.lib'),
        (Join-Path $Root 'lib/monosgen-2.0.lib')
    )

    $releaseLib = ''
    foreach ($l in $libCandidates) {
        if (Test-Path $l) { $releaseLib = $l; break }
    }

    $debugCandidates = @(
        (Join-Path $Root 'lib/Debug/libmono-static-sgen.lib'),
        (Join-Path $Root 'lib/mono-2.0-sgend.lib'),
        (Join-Path $Root 'lib/libmono-2.0-sgend.lib')
    ) + $libCandidates

    $debugLib = ''
    foreach ($l in $debugCandidates) {
        if (Test-Path $l) { $debugLib = $l; break }
    }

    $dllCandidates = @(
        (Join-Path $Root 'bin/mono-2.0-sgen.dll'),
        (Join-Path $Root 'bin/mono-2.0.dll'),
        (Join-Path $Root 'bin/libmono-2.0-sgen.dll'),
        (Join-Path $Root 'bin/monosgen-2.0.dll')
    )
    $dll = ''
    foreach ($d in $dllCandidates) {
        if (Test-Path $d) { $dll = $d; break }
    }

    $runtimeLib = ''
    if (Test-Path (Join-Path $Root 'lib/mono')) {
        $runtimeLib = Join-Path $Root 'lib/mono'
    }

    return [PSCustomObject]@{
        Root = $Root
        Include = $include
        DebugLib = $debugLib
        ReleaseLib = $releaseLib
        Dll = $dll
        RuntimeLib = $runtimeLib
    }
}

function Ensure-Mono {
    $mono = Find-MonoRoot
    if ([string]::IsNullOrWhiteSpace($mono)) {
        Invoke-WingetInstall -Id 'Mono.Mono' -Name 'Mono for Windows'
        $mono = Find-MonoRoot
    }

    if ([string]::IsNullOrWhiteSpace($mono)) {
        throw "Mono was not detected. Install Mono.Mono or set WHP_MONO_ROOT."
    }

    $layout = Resolve-MonoLayout $mono
    if ([string]::IsNullOrWhiteSpace($layout.Include)) {
        throw "Mono headers were not found under $mono. Expected mono/jit/jit.h."
    }
    if ([string]::IsNullOrWhiteSpace($layout.ReleaseLib)) {
        throw "Mono import/static .lib was not found under $mono. If you use a custom Mono build, set WHP_MONO_LIBRARY_RELEASE."
    }

    Write-Ok "Mono found: $mono"
    Write-Ok "Mono include: $($layout.Include)"
    Write-Ok "Mono library: $($layout.ReleaseLib)"
    if (-not [string]::IsNullOrWhiteSpace($layout.RuntimeLib)) {
        Write-Ok "Mono runtime assemblies: $($layout.RuntimeLib)"
    } else {
        Write-Warn "Mono runtime assemblies were not detected; runtime scripting may fail unless mono/lib is copied manually."
    }
    return $layout
}

function Write-CMakeEnvironment {
    param(
        [string]$VulkanRoot,
        [object]$MonoLayout,
        [string]$VcpkgRoot
    )

    New-Item -ItemType Directory -Force -Path $DepsRoot | Out-Null

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('# Generated by scripts/bootstrap.ps1. Do not edit manually.')
    $lines.Add('# This file is included by cmake/WhipBootstrap.cmake.')

    if (-not [string]::IsNullOrWhiteSpace($VulkanRoot)) {
        $lines.Add(('set(WHP_VULKAN_SDK_ROOT "{0}" CACHE PATH "Detected Vulkan SDK root" FORCE)' -f (Convert-ToCMakePath $VulkanRoot)))
    }

    if (-not [string]::IsNullOrWhiteSpace($VcpkgRoot)) {
        $lines.Add(('set(WHP_VCPKG_ROOT "{0}" CACHE PATH "Detected vcpkg root" FORCE)' -f (Convert-ToCMakePath $VcpkgRoot)))
    }

    if ($null -ne $MonoLayout) {
        $lines.Add(('set(WHP_MONO_ROOT "{0}" CACHE PATH "Detected Mono root" FORCE)' -f (Convert-ToCMakePath $MonoLayout.Root)))
        $lines.Add(('set(WHP_MONO_INCLUDE_DIR "{0}" CACHE PATH "Detected Mono include directory" FORCE)' -f (Convert-ToCMakePath $MonoLayout.Include)))
        $lines.Add(('set(WHP_MONO_LIBRARY_DEBUG "{0}" CACHE FILEPATH "Detected Mono Debug library" FORCE)' -f (Convert-ToCMakePath $MonoLayout.DebugLib)))
        $lines.Add(('set(WHP_MONO_LIBRARY_RELEASE "{0}" CACHE FILEPATH "Detected Mono Release library" FORCE)' -f (Convert-ToCMakePath $MonoLayout.ReleaseLib)))
        if (-not [string]::IsNullOrWhiteSpace($MonoLayout.Dll)) {
            $lines.Add(('set(WHP_MONO_DLL "{0}" CACHE FILEPATH "Detected Mono runtime DLL" FORCE)' -f (Convert-ToCMakePath $MonoLayout.Dll)))
        }
        if (-not [string]::IsNullOrWhiteSpace($MonoLayout.RuntimeLib)) {
            $lines.Add(('set(WHP_MONO_RUNTIME_LIB_DIR "{0}" CACHE PATH "Detected Mono runtime assembly directory" FORCE)' -f (Convert-ToCMakePath $MonoLayout.RuntimeLib)))
        }
    }

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $envContent = ($lines -join [Environment]::NewLine) + [Environment]::NewLine
    $envChanged = Write-FileIfChanged -Path $EnvFile -Content $envContent -Encoding $utf8NoBom

    $summary = [PSCustomObject]@{
        generatedAt = (Get-Date).ToString('o')
        mode = $Mode
        vulkanSdkRoot = $VulkanRoot
        mono = $MonoLayout
        vcpkgRoot = $VcpkgRoot
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -Path $SummaryFile -Encoding UTF8

    if ($envChanged) {
        Write-Ok "Generated $EnvFile"
    } else {
        Write-Ok "Using unchanged $EnvFile"
    }
}

try {
    if ($Mode -eq 'Off') {
        Write-Warn 'Bootstrap mode is Off; only writing any already detected paths.'
    }

    Write-Section 'Host tools'
    Ensure-Command -Command 'git' -WingetId 'Git.Git' -Name 'Git'
    if (-not $FromCMake) {
        Ensure-Command -Command 'cmake' -WingetId 'Kitware.CMake' -Name 'CMake'
    }
    Ensure-VisualStudioBuildTools
    $vcpkgRoot = Ensure-Vcpkg
    Ensure-VcpkgPackages -VcpkgRoot $vcpkgRoot

    Write-Section 'SDKs'
    $vulkanRoot = Ensure-VulkanSdk
    $monoLayout = Ensure-Mono

    Write-Section 'CMake environment'
    Write-CMakeEnvironment -VulkanRoot $vulkanRoot -MonoLayout $monoLayout -VcpkgRoot $vcpkgRoot

    Write-Host ''
    Write-Ok 'Whip bootstrap completed.'
    exit 0
}
catch {
    Write-Fail $_.Exception.Message
    if ($Mode -eq 'CheckOnly') {
        Write-Host ''
        Write-Host 'CheckOnly mode does not install missing dependencies. Run:' -ForegroundColor Yellow
        Write-Host '  powershell -ExecutionPolicy Bypass -File scripts/bootstrap.ps1 -Mode Prompt' -ForegroundColor Yellow
    }
    exit 1
}
