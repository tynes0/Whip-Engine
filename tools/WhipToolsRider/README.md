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
- Gradle 9+
- network access for JetBrains Platform artifacts

From this directory:

```powershell
gradle buildPlugin
```

The plugin ZIP will be produced under:

```text
build/distributions/
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
