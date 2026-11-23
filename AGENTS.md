# AGENTS.md — ESP32 Eave Lights

Instructions for coding agents (Claude Code, Codex, etc.) working in this repo.
Project background, hardware facts and decisions: [Docs/ESP32_House_Eave_Lights_Handoff.md](Docs/ESP32_House_Eave_Lights_Handoff.md). Read it before changing firmware.

## Toolchain

The user programs with a **portable Arduino IDE 1.8.19**, not Arduino IDE 2, arduino-cli or PlatformIO.

| Item | Value |
|---|---|
| IDE folder | `D:\00 Programming\01 Arduino\arduino-1.8.19 ESP 3.0` |
| Command-line build tool | `arduino_debug.exe` in that folder |
| ESP32 core | esp32:esp32 **3.3.12** (in `portable\packages`) |
| Libraries | `portable\sketchbook\libraries` (FastLED **3.10.10**) |
| Serial port | **COM13**: the board's CH343 USB-serial chip (VID 1A86 / PID 55D3) |

The user keeps several portable IDEs, one per toolchain version. Use this folder only, and don't install or update cores or libraries without asking.

## Board settings (Tools menu)

| Setting | Value | FQBN option |
|---|---|---|
| Board | ESP32S3 Dev Module | `esp32:esp32:esp32s3` |
| USB CDC On Boot | **Disabled** (Serial goes out UART0 through the CH343 on COM13) | default |
| Flash Size | 16MB (128Mb) | `FlashSize=16M` |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) | `PartitionScheme=app3M_fat9M_16MB` |
| PSRAM | OPI PSRAM | `PSRAM=opi` |
| Upload Mode | UART0 / Hardware CDC | default |
| Upload Speed | 921600 | default |

## Build and upload from the command line

PowerShell:

```powershell
$ide  = "D:\00 Programming\01 Arduino\arduino-1.8.19 ESP 3.0\arduino_debug.exe"
$fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
$ino  = "C:\Users\Admin\Desktop\ESP32_Eave_Lights\ESP32_Eave_Lights.ino"

# Compile only
& $ide --verify --board $fqbn $ino

# Compile and upload
& $ide --upload --board $fqbn --port COM13 $ino
```

- Passing `--board` on the command line does not change the IDE's saved settings.
- A warning about an invalid `MT6701` library is expected and harmless.
- After compiling, report the "Sketch uses … / Global variables use …" lines.
- Only upload when the user asks: the board drives the house lights.

## Sketch layout: one `.ino` per task

The user prefers **separate `.ino` files (IDE tabs) for different tasks** instead of `.h`/`.cpp` pairs.

```text
ESP32_Eave_Lights/              folder name must equal the main sketch name
  ESP32_Eave_Lights.ino         MAIN: must stay in the repo root, never move or rename
  led_output.ino                strand setup, power caps, showFrame()
  test_pattern.ino              red/green/blue/white/off test cycle
  <task>.ino                    add new tasks the same way (wifi.ino, web_api.ino, effects.ino ...)
  Docs/                         handoff and notes (ignored by the compiler)
```

How the IDE handles tabs:
- The main `.ino` comes first, then the other tabs **in alphabetical order**, all joined into one file before compiling.
- Put **all `#define`s, `#include`s and shared global variables in the main `.ino`** so every tab can see them.
- Functions can be called from any tab, because the IDE generates prototypes automatically. A `struct` or `enum` can't be used before the tab that defines it. Keep a type inside the tab that uses it, or define it in the main `.ino` if several tabs use it.
- Keep `setup()` and `loop()` in the main `.ino`, and have them call one function per task (`initOutputs()`, `runTestPattern()` …).
- Name tabs in lowercase snake_case after the task.

Standalone throwaway test sketches go in their own folder under `tests/<name>/<name>.ino`, so they never get compiled into the main sketch.

## Firmware rules (from the handoff)

- LED color order is **`RBG`**, chipset `WS2811`. Never change it to `GRB`.
- Output A = GPIO 8 (tested). Output B = GPIO 9 (planned, off via `ENABLE_OUTPUT_B`).
- Every frame goes through `showFrame()` so the power caps are always applied. Never call `FastLED.show()` on raw colors.
- `initOutputs()` stays the first line of `setup()` (sends OFF).
- No blocking `delay()` in `loop()`. Use `millis()` timing.
- No Wi-Fi passwords in tracked files: use `secrets.h`, which is gitignored.

## Versioning and git

- Bump `FW_VERSION` in the main `.ino` on every build that changes firmware.
- Commit each finished change. **Never push** unless asked.
- Leave the user's own unrelated edits alone.
