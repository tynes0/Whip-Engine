param(
	[string]$Task = "buildPlugin",
	[string]$GradleVersion = "9.0.0",
	[switch]$ForceDownload,
	[Parameter(ValueFromRemainingArguments = $true)]
	[string[]]$GradleArguments
)

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$LocalGradleRoot = Join-Path $Root ".gradle-local"
$GradleUserHome = Join-Path ([System.IO.Path]::GetTempPath()) "WhipToolsRider\gradle-user-home-v2"
$GradleBuildRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("WhipToolsRider\gradle-build\" + [Guid]::NewGuid().ToString("N"))
$GradleHome = Join-Path $LocalGradleRoot "gradle-$GradleVersion"
$GradleExe = Join-Path $GradleHome "bin\gradle.bat"

Set-Location -LiteralPath $Root

function Get-JavaMajorVersion {
	param([string]$JavaExe)

	if (-not (Test-Path -LiteralPath $JavaExe)) {
		return 0
	}

	$startInfo = New-Object System.Diagnostics.ProcessStartInfo
	$startInfo.FileName = $JavaExe
	$startInfo.Arguments = "-version"
	$startInfo.UseShellExecute = $false
	$startInfo.RedirectStandardOutput = $true
	$startInfo.RedirectStandardError = $true
	$process = [System.Diagnostics.Process]::Start($startInfo)
	$stdout = $process.StandardOutput.ReadToEnd()
	$stderr = $process.StandardError.ReadToEnd()
	$process.WaitForExit()
	$versionText = "$stdout`n$stderr"
	if ($versionText -match 'version\s+"(?<version>[^"]+)"') {
		$version = $Matches.version
		if ($version.StartsWith("1.")) {
			$parts = $version.Split(".")
			if ($parts.Length -ge 2) {
				return [int]$parts[1]
			}
		}

		$major = $version.Split(".")[0]
		return [int]$major
	}

	return 0
}

function Get-JavaHomeFromExe {
	param([string]$JavaExe)

	$javaPath = Resolve-Path -LiteralPath $JavaExe -ErrorAction SilentlyContinue
	if (-not $javaPath) {
		return $null
	}

	$binDirectory = Split-Path -Parent $javaPath.Path
	return Split-Path -Parent $binDirectory
}

function Test-JavaHome {
	param([string]$JavaHome)

	if ([string]::IsNullOrWhiteSpace($JavaHome)) {
		return $false
	}

	$javaExe = Join-Path $JavaHome "bin\java.exe"
	if (-not (Test-Path -LiteralPath $javaExe)) {
		return $false
	}

	return (Get-JavaMajorVersion $javaExe) -ge 17
}

function Get-CandidateJavaHomes {
	$candidates = New-Object System.Collections.Generic.List[string]

	foreach ($envName in @("JAVA_HOME", "JDK_HOME")) {
		$value = [Environment]::GetEnvironmentVariable($envName)
		if (-not [string]::IsNullOrWhiteSpace($value)) {
			$candidates.Add($value)
		}
	}

	$pathJava = Get-Command java -ErrorAction SilentlyContinue
	if ($pathJava) {
		$javaHome = Get-JavaHomeFromExe $pathJava.Source
		if ($javaHome) {
			$candidates.Add($javaHome)
		}
	}

	$roots = @(
		"$env:ProgramFiles\Eclipse Adoptium",
		"$env:ProgramFiles\Java",
		"$env:ProgramFiles\Microsoft",
		"$env:ProgramFiles\JetBrains",
		"$env:LOCALAPPDATA\Programs\Eclipse Adoptium",
		"$env:LOCALAPPDATA\Programs\Java",
		"$env:LOCALAPPDATA\Programs\JetBrains",
		"$env:LOCALAPPDATA\JetBrains\Toolbox\apps\Rider"
	)

	foreach ($root in $roots) {
		if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root)) {
			continue
		}

		Get-ChildItem -LiteralPath $root -Directory -Recurse -ErrorAction SilentlyContinue |
			Where-Object {
				$_.Name -match "^(jdk|jbr).*(17|18|19|20|21|22|23|24|25)" -or
				(Test-Path -LiteralPath (Join-Path $_.FullName "bin\java.exe"))
			} |
			ForEach-Object { $candidates.Add($_.FullName) }
	}

	return $candidates | Select-Object -Unique
}

function Use-Java17 {
	foreach ($candidate in Get-CandidateJavaHomes) {
		if (Test-JavaHome $candidate) {
			$env:JAVA_HOME = (Resolve-Path -LiteralPath $candidate).Path
			$env:PATH = (Join-Path $env:JAVA_HOME "bin") + [IO.Path]::PathSeparator + $env:PATH
			$major = Get-JavaMajorVersion (Join-Path $env:JAVA_HOME "bin\java.exe")
			Write-Host "Using Java ${major}: $env:JAVA_HOME"
			return
		}
	}

	throw "Java 17+ bulunamadi veya JAVA_HOME Java 8'e bakiyor. Kurmak icin: winget install EclipseAdoptium.Temurin.17.JDK"
}

function Get-GlobalGradle {
	if ($ForceDownload) {
		return $null
	}

	return Get-Command gradle -ErrorAction SilentlyContinue
}

function Test-RiderHome {
	param([string]$RiderHome)

	if ([string]::IsNullOrWhiteSpace($RiderHome) -or -not (Test-Path -LiteralPath $RiderHome)) {
		return $false
	}

	return (
		(Test-Path -LiteralPath (Join-Path $RiderHome "product-info.json")) -or
		(Test-Path -LiteralPath (Join-Path $RiderHome "bin\rider64.exe")) -or
		(Test-Path -LiteralPath (Join-Path $RiderHome "bin\rider.bat"))
	)
}

function Get-RiderHomeFromRegistry {
	$registryRoots = @(
		"HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
		"HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
		"HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
	)

	foreach ($registryRoot in $registryRoots) {
		$entries = Get-ItemProperty $registryRoot -ErrorAction SilentlyContinue |
			Where-Object { $_.DisplayName -like "JetBrains Rider*" -and -not [string]::IsNullOrWhiteSpace($_.InstallLocation) } |
			Sort-Object DisplayName -Descending

		foreach ($entry in $entries) {
			if (Test-RiderHome $entry.InstallLocation) {
				return (Resolve-Path -LiteralPath $entry.InstallLocation).Path
			}
		}
	}

	return $null
}

function Get-RiderHomeFromCommonRoots {
	$roots = @(
		"$env:LOCALAPPDATA\Programs",
		"$env:LOCALAPPDATA\JetBrains\Toolbox\apps",
		"$env:ProgramFiles\JetBrains",
		"$env:ProgramFiles",
		"F:\Uygulamalar"
	)

	foreach ($root in $roots) {
		if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root)) {
			continue
		}

		$riderExe = Get-ChildItem -LiteralPath $root -Recurse -Filter rider64.exe -ErrorAction SilentlyContinue | Select-Object -First 1
		if ($riderExe) {
			$riderHome = Split-Path -Parent (Split-Path -Parent $riderExe.FullName)
			if (Test-RiderHome $riderHome) {
				return (Resolve-Path -LiteralPath $riderHome).Path
			}
		}
	}

	return $null
}

function Get-LocalRiderHome {
	$envRiderHome = [Environment]::GetEnvironmentVariable("WHP_RIDER_HOME")
	if (Test-RiderHome $envRiderHome) {
		return (Resolve-Path -LiteralPath $envRiderHome).Path
	}

	$registryRiderHome = Get-RiderHomeFromRegistry
	if ($registryRiderHome) {
		return $registryRiderHome
	}

	return Get-RiderHomeFromCommonRoots
}

function Get-GradleTaskArguments {
	$resolvedArguments = @($GradleArguments)
	$hasLocalRiderPath = $false
	$hasBuildRoot = $false
	foreach ($argument in $resolvedArguments) {
		if ($argument -like "-PlocalRiderPath=*") {
			$hasLocalRiderPath = $true
		}

		if ($argument -like "-PwhipBuildRoot=*") {
			$hasBuildRoot = $true
		}
	}

	if (-not $hasLocalRiderPath) {
		$localRiderHome = Get-LocalRiderHome
		if ($localRiderHome) {
			Write-Host "Using local Rider platform: $localRiderHome"
			$resolvedArguments += "-PlocalRiderPath=$localRiderHome"
		} else {
			Write-Host "Local Rider platform not found; Gradle will download Rider platform artifacts."
		}
	}

	if (-not $hasBuildRoot) {
		Write-Host "Using Gradle build root: $GradleBuildRoot"
		$resolvedArguments += "-PwhipBuildRoot=$GradleBuildRoot"
	}

	return @($Task) + $resolvedArguments + @("--no-daemon")
}

function Install-LocalGradle {
	if ((Test-Path -LiteralPath $GradleExe) -and -not $ForceDownload) {
		return
	}

	$downloadDirectory = Join-Path $LocalGradleRoot "downloads"
	$zipPath = Join-Path $downloadDirectory "gradle-$GradleVersion-bin.zip"
	$downloadUrl = "https://services.gradle.org/distributions/gradle-$GradleVersion-bin.zip"

	New-Item -ItemType Directory -Force -Path $downloadDirectory | Out-Null

	if ((Test-Path -LiteralPath $GradleHome) -and $ForceDownload) {
		Remove-Item -LiteralPath $GradleHome -Recurse -Force
	}

	if (-not (Test-Path -LiteralPath $zipPath) -or $ForceDownload) {
		Write-Host "Downloading Gradle $GradleVersion..."
		Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath
	}

	Write-Host "Extracting Gradle $GradleVersion..."
	Expand-Archive -LiteralPath $zipPath -DestinationPath $LocalGradleRoot -Force
}

function Clear-GradleTemporaryFiles {
	$temporaryDirectory = Join-Path $GradleUserHome ".tmp"
	if (Test-Path -LiteralPath $temporaryDirectory) {
		Write-Host "Cleaning Gradle temp directory: $temporaryDirectory"
		Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
	}

	New-Item -ItemType Directory -Force -Path $temporaryDirectory | Out-Null
}

Use-Java17
$GradleTaskArguments = Get-GradleTaskArguments
$env:GRADLE_USER_HOME = $GradleUserHome
New-Item -ItemType Directory -Force -Path $env:GRADLE_USER_HOME | Out-Null
Clear-GradleTemporaryFiles
Write-Host "Using Gradle user home: $env:GRADLE_USER_HOME"

$globalGradle = Get-GlobalGradle
if ($globalGradle) {
	Write-Host "Using global Gradle: $($globalGradle.Source)"
	& $globalGradle.Source @GradleTaskArguments
	exit $LASTEXITCODE
}

Install-LocalGradle

if (-not (Test-Path -LiteralPath $GradleExe)) {
	throw "Gradle indirildi ama calistirilabilir dosya bulunamadi: $GradleExe"
}

Write-Host "Using local Gradle: $GradleExe"
& $GradleExe @GradleTaskArguments
exit $LASTEXITCODE
