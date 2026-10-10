## Build
Builds are driven by the **ESP8266-IDF VS Code extension** (https://github.com/Dzantemir/ESP8266-IDF); its `launcher.py` invokes the toolchain. The extension's `python/launcher.py` lives under the editor's extensions folder. Locations:

- Linux (local VS Code): `~/.vscode/extensions/dzantemir.esp8266-idf-*/python/launcher.py`
- Linux (remote / VS Code Server): `~/.vscode-server/extensions/dzantemir.esp8266-idf-*/python/launcher.py`
- macOS: `~/.vscode/extensions/dzantemir.esp8266-idf-*/python/launcher.py`
- Windows: `%USERPROFILE%\.vscode\extensions\dzantemir.esp8266-idf-*\python\launcher.py`

The target is fixed to ESP8266 (IDF_TARGET=esp8266). Run the launcher with Python 3 from the project root.

```bash
# Linux / macOS
LAUNCHER=$(ls ~/.vscode*/extensions/dzantemir.esp8266-idf-*/python/launcher.py 2>/dev/null | head -n1)
python3 "$LAUNCHER" .vscode/spec.json
```

```powershell
# Windows (PowerShell)
$launcher = (Get-ChildItem "$env:USERPROFILE\.vscode\extensions\dzantemir.esp8266-idf-*\python\launcher.py" | Select-Object -First 1).FullName
python "$launcher" .vscode/spec.json
```

`spec.json` is consumed by the launcher. Required fields (omit `root`/`cwd` and the build runs in `$HOME`/profile dir and fails with "CMakeLists.txt not found"):

```json
{
  "action": "idf",
  "args": ["build"],
  "root": "<absolute project-root path>",
  "cwd": "<absolute project-root path>",
  "idfPath": "<value of $IDF_PATH (or %IDF_PATH%)>",
  "idfToolsPath": "<value of $IDF_TOOLS_PATH (or %IDF_TOOLS_PATH%)>",
  "sdkVersion": "<ESP8266_RTOS_SDK version, e.g. $(git -C "$IDF_PATH" describe)>",
  "marker": "<project>/.vscode/marker.json"
}
```

Notes:
- Place both `spec.json` and `marker.json` in the project's `.vscode/` folder.
- `idfPath`/`idfToolsPath` come from the environment (`IDF_PATH` / `IDF_TOOLS_PATH`); expand `~` (or `%USERPROFILE%`) to absolute paths.
- Other actions: `build` / `clean` / `fullclean` / `size` / `monitor` / `flash` / `reconfigure`.
- `marker.json` `exitCode` holds the exit code.
- On Linux/macOS use `python3`; on Windows `python` (or `py -3`). A Python 3.9+ interpreter is required.

### Success criteria
- Exit code 0 and final output `Project build complete.`
- Artifacts: `build/<project>.bin`, `build/<project>.elf`, `build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`
- Flash addresses: `0x0` bootloader / `0x8000` partition-table / `0x10000` app
