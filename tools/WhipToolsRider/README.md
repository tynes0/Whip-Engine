# Whip Tools Rider

JetBrains Rider plugin scaffold for Whip Engine script debugging.

The plugin reads Whip's generated `Assets/Scripts/.whip-debugger.json` file and exposes:

- `Tools > Whip > Attach to Whip Debugger`
- debugger contract discovery from either the generated script workspace or the Whip project root
- Mono debugger endpoint validation
- clipboard copy for the `host:port` endpoint
- best-effort opening of Rider's existing attach UI when a matching Rider action is available

## Build

Requirements:

- Java 17
- Rider installed locally, or network access for JetBrains Platform artifacts

From this directory:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Or use the Windows wrapper:

```cmd
build.cmd
```

The script uses a global Gradle installation if available. If Gradle is not installed,
it downloads a local portable Gradle distribution into `.gradle-local/` and builds with it.
It also searches for Java 17+ and temporarily sets `JAVA_HOME` for the build, so a system
PATH that still points to Java 8 should not break the plugin build.
If Rider is installed, the script uses that local Rider installation instead of downloading
the full Rider platform ZIP. Set `WHP_RIDER_HOME` if Rider lives in a custom directory:

```powershell
$env:WHP_RIDER_HOME = "F:\Uygulamalar\JetBrains Rider 2025.2.2.1"
```

The wrapper builds in a fresh temp directory to avoid Windows file locks from stale Gradle
outputs. The plugin ZIP is produced under the temp build root printed by the command:

```text
%TEMP%\WhipToolsRider\gradle-build\<run-id>\distributions\
```

If Java 17 is missing on Windows:

```powershell
winget install EclipseAdoptium.Temurin.17.JDK
```

If you still prefer a global Gradle install:

```powershell
winget install Gradle.Gradle
```

If your Rider version differs, update `platformVersion` in `gradle.properties`.

## Workflow

1. Open a Whip project in Whip Editor.
2. Enable `Project Settings > Script Debugger`.
3. Save the project and restart Whip Editor.
4. Build scripts once so `Assets/Scripts/.whip-debugger.json` is refreshed.
5. Open the generated C# solution in Rider.
6. Run `Tools > Whip > Attach to Whip Debugger`.

This first Rider pass prepares and validates the attach endpoint. The next step is a deeper Rider integration that creates/runs a dedicated Mono attach configuration directly instead of handing off to Rider's attach UI.
