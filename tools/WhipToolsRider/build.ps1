param(
	[string]$Task = "buildPlugin",
	[string]$GradleVersion = "9.0.0",
	[switch]$ForceDownload
)

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$LocalGradleRoot = Join-Path $Root ".gradle-local"
$GradleHome = Join-Path $LocalGradleRoot "gradle-$GradleVersion"
$GradleExe = Join-Path $GradleHome "bin\gradle.bat"

function Get-JavaMajorVersion {
	param([string]$JavaExe)

	if (-not (Test-Path -LiteralPath $JavaExe)) {
		return 0
	}

	$versionOutput = & $JavaExe -version 2>&1
	$versionText = ($versionOutput | Out-String)
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

Use-Java17

$globalGradle = Get-GlobalGradle
if ($globalGradle) {
	Write-Host "Using global Gradle: $($globalGradle.Source)"
	& $globalGradle.Source $Task
	exit $LASTEXITCODE
}

Install-LocalGradle

if (-not (Test-Path -LiteralPath $GradleExe)) {
	throw "Gradle indirildi ama calistirilabilir dosya bulunamadi: $GradleExe"
}

Write-Host "Using local Gradle: $GradleExe"
& $GradleExe $Task
exit $LASTEXITCODE
