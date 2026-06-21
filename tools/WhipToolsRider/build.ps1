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

function Require-Java {
	$java = Get-Command java -ErrorAction SilentlyContinue
	if (-not $java) {
		throw "Java 17+ bulunamadi. Rider plugin build icin once Java 17 kur: winget install EclipseAdoptium.Temurin.17.JDK"
	}
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

Require-Java

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
