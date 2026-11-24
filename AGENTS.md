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

**Board:** AYWHP ESP32-S3 dev board, ESP32-S3-WROOM-1-**N16R8** module (16 MB flash, 8 MB octal PSRAM), with **two USB-C ports**:
- **COM / UART port** (CH343 chip): the one in use, shows up as COM13. Requires USB CDC On Boot = **Disabled**.
- **USB port** (the ESP32-S3's built-in USB): shows up as a different COM port and would need USB CDC On Boot = **Enabled** for Serial output. Don't use it unless the user switches.

Octal PSRAM uses GPIO 35–37: never assign those pins.

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

# Compile only. Use a fixed build folder so the ESP32 core and libraries are reused:
# about 40 s instead of about 165 s for a fresh folder (measured 2026-11-23).
$bp = "$env:TEMP\eave-lights-build"
& $ide --verify --board $fqbn --pref "build.path=$bp" $ino

# Compile and upload
& $ide --upload --board $fqbn --port COM13 $ino
```

- Passing `--board` on the command line does not change the IDE's saved settings.
- A warning about an invalid `MT6701` library is expected and harmless.
- **Boot loop with `partition 3 invalid … exceeds flash chip`:** the IDE built with Flash Size 4MB. Set Tools → Flash Size → 16MB and upload again. To check what an IDE build actually used, look at `fqbn` in `%TEMP%\arduino_build_*\build.options.json`. (Verified 2026-11-23: the chip reads 16 MB flash and 8 MB embedded PSRAM with `esptool flash-id`.)
- Use explicit `CRGB(r,g,b)` values for test colors. FastLED's `CRGB::Green` is (0,128,0).
- After compiling, report the "Sketch uses … / Global variables use …" lines.
- Only upload when the user asks: the board drives the house lights.

## Wireless updates (v0.12.0+)

The board is installed outside, so normally upload over Wi-Fi:
- **Arduino IDE:** Tools → Port → `eavelights at 192.168.1.25` (network port), then Upload. The IDE asks for `OTA_PASSWORD` from `secrets.h`, or `AP_PASSWORD` if that isn't defined. From the command line: `--upload --port 192.168.1.25`. Only upload when the user asks.
- **Web page:** Setup → Firmware update with the `.bin` from Sketch → Export compiled Binary (`POST /api/firmware?password=…`).
- A failed update leaves the old firmware running (two 3 MB app slots). Any firmware that drops `ota.ino` loses wireless updates, and the next update then needs USB.

## Sketch layout: one `.ino` per task

The user prefers **separate `.ino` files (IDE tabs) for different tasks** instead of `.h`/`.cpp` pairs.

```text
ESP32_Eave_Lights/              folder name must equal the main sketch name
  ESP32_Eave_Lights.ino         MAIN: must stay in the repo root, never move or rename.
                                defines, includes, all shared types (LightState, PathPoint,
                                ZoneDef, Preset, FileHeader), states[] per channel, globals,
                                setup(), loop()
  effects_more.ino              the 16 extra effects (rainbow, gradient, stripes, twinkle,
                                fire, lightning…); what speed/size mean per effect is
                                listed at the top. Must match the MODES table in web_page.h
  effects.ino                   draws each channel (solid/fade/test/chase/comet) at 50 fps,
                                layers strands → Whole house → zones, then Find-a-light overlay
  led_output.ino                strand setup, power caps, showFrame()
  locate.ino                    Find a light: show only chosen ranges, 5-min auto-off
  ota.ino                       wireless updates: ArduinoOTA (IDE network port) + web .bin upload
  paths.ino                     Whole house path (B65→B1, A1→A65) and paths made from
                                zones (steps: zone, reverse, gap) in /paths.bin
  presets.ino                   saved looks (name + LightState) in /presets.bin
  scenes.ino                    scenes: snapshot of every channel's LightState, /scenes.bin
  backup.ino                    backup (/api/files, /fs/<path>) and restore (/api/restore-*)
  settings_store.ino            strand/house settings in Preferences, delayed save;
                                mounts FFat storage
  test_pattern.ino              red/green/blue/white/off test cycle
  web_api.ino                   JSON API: /api/state, /api/frame, /api/locate, /api/zones,
                                /api/presets (endpoint list at the top of the file)
  web_page.h                    the control page HTML/CSS/JS (see exception below), 4 tabs:
                                Home (live photo/strip, scenes, what's on, All off),
                                Lights (grouped picker, look + mode fold-outs, saved looks),
                                House (editor in an iframe, /editor?embed=1),
                                Setup (fold-outs: strands, find, zones, paths, backup, firmware)
  editor_page.h                 House view editor at /editor: photos, sections drawn along the
                                eave with evenly spaced lights, Live night-glow view
  files.ino                     photo upload, layout.json save, /files static serving
  wifi.ino                      home Wi-Fi, hotspot fallback, mDNS eavelights.local
  zones.ino                     zones (name + light ranges + settings) in /zones.bin,
                                range text parser ("A1-12, B30-65")
  secrets.h                     Wi-Fi passwords, gitignored (copy from secrets.example.h)
  <task>.ino                    add new tasks the same way
  Docs/                         handoff and notes (ignored by the compiler)
```

How the IDE handles tabs:
- The main `.ino` comes first, then the other tabs **in alphabetical order**, all joined into one file before compiling.
- Put **all `#define`s, `#include`s and shared global variables in the main `.ino`** so every tab can see them.
- Functions can be called from any tab, because the IDE generates prototypes automatically. A `struct` or `enum` can't be used before the tab that defines it. Keep a type inside the tab that uses it, or define it in the main `.ino` if several tabs use it.
- Keep `setup()` and `loop()` in the main `.ino`, and have them call one function per task (`initOutputs()`, `runTestPattern()` …).
- Name tabs in lowercase snake_case after the task.
- The `WebServer server` object is declared in the main `.ino` because `files.ino` comes before `web_api.ino` alphabetically.
- `/files/layout.json` is written by the editor page and only stored by the board. The firmware never parses it, so its schema (documented at the top of `editor_page.h`) can change without a firmware change.
- **Exception: web page text goes in `.h` files** (`web_page.h`, `editor_page.h`). The IDE scans `.ino` tabs to generate prototypes and mistakes JavaScript `function name(){` lines inside a raw string for C++, which breaks the build with "'function' does not name a type".
- Save `.ino` files as UTF-8 **without** a byte-order mark. PowerShell 5.1 `Set-Content -Encoding utf8` adds one and the compiler rejects it.
- **Adding an effect:** add it to the end of `enum Mode` (never renumber, the numbers are saved), to `MODE_NAMES` in `web_api.ino`, the `drawChannel()` switch, and the `MODES` table in `web_page.h` (which colors and settings it uses, and their labels). Use `hash32()` instead of `random()` so effects stay repeatable.
- `.bin` storage files have a `FileHeader` with the record size. Changing `LightState`, `ZoneDef`, `PathDef`/`PathStep`, `Preset`, `Scene`, or `MAX_ZONES`/`MAX_PATHS` (which change `Scene`'s size) makes old files unreadable, and zones/saved looks start empty. Warn the user before changing those structs, and suggest Setup → Download backup first. Better still, write a converter for the old version, as `zones.ino` does for `zones.bin` v1. While a restore runs, `savesBlocked` stops all saving so uploaded files aren't overwritten from memory.

Standalone throwaway test sketches go in their own folder under `tests/<name>/<name>.ino`, so they never get compiled into the main sketch.

## Strand lengths

Strand lengths are **not fixed in code**: `ledCount[]` is set on the page (Setup → Strand lengths, `POST /api/strands`) and saved (`a_cnt`, `b_cnt`). Arrays are sized for `MAX_LEDS` (300) per strand. Never use a fixed light count. Loop to `ledCount[o]`; the Whole house path is `houseLen`. `/api/frame` sends each strand's count. At boot all `MAX_LEDS` are cleared, then the controllers are trimmed with `setLeds()`.

## Firmware rules (from the handoff)

- LED color order is **`RBG`**, chipset `WS2811`. Never change it to `GRB`.
- Output A = GPIO 8 (tested). Output B = GPIO 9 (`ENABLE_OUTPUT_B 1`; pin verified with strand A in v0.3.0, the installed strand B not yet connected). Each **channel** has its own `LightState` in `states[]`: 0 = strand A, 1 = strand B, `CH_HOUSE` = Whole house path (v0.7+). Effects draw on a channel's "line" through `lineLength()`/`lineLight()`, so they work the same on strands and paths. Drawing order while on: strands → Whole house → zone paths (`CH_PATH0 + slot`, up to 8) → zones (`CH_ZONE0 + slot`, up to 12). Zone-path gap positions use `VIRTUAL_OUTPUT` and draw to a hidden `gapSink`. Paths are rebuilt whenever a zone changes. Zones and saved looks are stored on the FFat partition, so the **16M Flash (3MB APP/9.9MB FATFS)** partition scheme is required. The API's `output=all` changes both strands (not paths) and lines up their phases.
- Every frame goes through `showFrame()` so the power caps are always applied. Never call `FastLED.show()` on raw colors.
- `initOutputs()` stays the first line of `setup()` (sends OFF).
- No blocking `delay()` in `loop()`. Use `millis()` timing.
- No Wi-Fi passwords in tracked files: use `secrets.h`, which is gitignored.

## Versioning and git

- Bump `FW_VERSION` in the main `.ino` on every build that changes firmware.
- Commit each finished change. **Never push** unless asked.
- Leave the user's own unrelated edits alone.
