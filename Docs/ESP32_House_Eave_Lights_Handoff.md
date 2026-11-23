# ESP32-S3 House Eave Lighting — Project Handoff

**Date:** 2026-11-23  
**Status:** One 65-puck strand verified under direct microcontroller control. Software project not yet created.  
**Purpose:** Give a new VS Code/Codex coding session the hardware facts, target architecture, safety constraints, and first implementation plan without requiring the conversation history.

## 1. Project goal

Build a **custom, Wi-Fi-controlled outdoor eave-light system** using an **ESP32-S3**, retaining the existing 24 V individually addressable RGB puck strands and replacing the Lotus Lantern Pro Bluetooth controller.

**Key differentiator:** The main PC/browser software should be a **visual house-layout and animation designer** — *not merely a traditional LED strip control panel*.

The user wants to:

- Import a **photograph (or multiple views) of their actual house** as an editor background.
- Draw physical wire/strand routes over the photograph, define start points and directions, and place each physical LED at its approximate location.
- Assign each placed light to its real physical address: `(output_id, pixel_index)`.
- Create named zones (porch, front, garage, side, back, etc.), including **noncontiguous groups** crossing multiple strands.
- Build **virtual animation paths** that can reverse, skip/jump between physically separated regions, and traverse the house in logical order.
- Design and **preview animations on the house image without hardware connected**.
- Keep selected regions a steady color while other regions chase, fade, or animate.
- Upload maps, presets, and compatible animation definitions to the ESP32 over Wi-Fi **without reflashing firmware**.
- Execute stored animations locally on the ESP32 when PC/phone is offline; add phone-friendly control, schedules and, optionally, original IR remote support.

The system should be extensible: initially 2 physical outputs × 65 LEDs, but avoid hard-coding a two-output topology into the design.

## 1a. Decisions (2026-11-23)

| Topic | Decision |
|---|---|
| Toolchain | **Portable Arduino IDE 1.8.19** at `D:\00 Programming\01 Arduino\arduino-1.8.19 ESP 3.0` (ESP32 core 3.3.12, FastLED 3.10.10). `ESP32_Eave_Lights.ino` must stay in the repo root. Code is split into one `.ino` tab per task. See [AGENTS.md](../AGENTS.md). |
| Board settings | ESP32S3 Dev Module · USB CDC On Boot: **Disabled** (COM13 is the board's CH343 USB-serial chip) · Flash Size: 16MB · Partition: 16M Flash (3MB APP/9.9MB FATFS) · PSRAM: OPI PSRAM. |
| Output A | GPIO 8, 65 LEDs, WS2811, `RBG` (tested). |
| Output B | GPIO 9. **Pin verified 2026-11-23** (v0.3.0) by moving strand A's data from GPIO 8 to 9: test cycle, fade and web control all correct. The installed strand B itself is still untested. Enabled in code; currently mirrors output A. GPIO 8/9 are the Arduino default I2C pins: any future I2C device must use other pins explicitly. |
| Power switching | The 24 V supply is on a **dusk-to-dawn photocell** (user, 2026-11-23): lights are only powered at night, and saved settings bring the last look back each dusk. Standby draw with all lights black measured **0.188 A at 24 V** (one strand, ~2.9 mA per puck); accepted because it only occurs at night. No MOSFET/relay needed. If the ESP32 is on the same switched supply, the web page is only reachable after dusk. |
| Startup | Lights showing white at power-up until the ESP32 boots is **accepted**. Firmware sends OFF as the first thing in `setup()`. No 24 V power switch planned. |
| Power limit | Per-color caps: red 240, green 240, blue 240; mixed colors limited to a channel sum of 400 (white ≈ 133 each). White at 100 each measured 1.1 A per strand; white at 133 each (v0.1.2 test pattern) measured **1.26 A** per strand (2026-11-23, one strand). |
| Data wiring | Direct 3.3 V for now. **74AHCT125 not yet purchased**; required before permanent outdoor install. |
| **Physical layout** | Confirmed by user 2026-11-23. Facing the house, the controller is in the **middle**. **Strand A runs left**, **strand B runs right**; light 1 of each is next to the controller, A65 is the far left end, B65 the far right end. The Whole house path (B65 → B1 → A1 → A65) therefore sweeps **right to left**. |
| Whole house path | v0.7.0: built-in 130-light path B65 → B1, A1 → A65 with its own saved settings; covers both strands while on. Precursor to zones and editor-made paths. |
| Zones, saved looks, Find a light | v0.9.0: up to 12 zones (name + ranges like `A1-12, B30-65`, reversible), each with its own settings, drawn on top of strands and Whole house. Up to 32 saved looks (named settings, e.g. "Xmas" = red chase on green background) applied to any strand/zone/path. Find a light lights chosen lights only. Stored as binary files on the FFat partition; replaced by JSON when the photo editor arrives. |
| Paths from zones | v0.10.0: up to 8 paths, each an ordered list of up to 12 zones with per-zone reverse and a 0–30 light invisible gap after it (the "jump"). Leaving a zone out skips it. Drawn above Whole house, below zones. First real zone map made by the user 2026-11-23 (7 zones, B65→B1 then A1→A65, right to left). **Verified on hardware 2026-11-23**: a zone path with a gap jumping the porch works. |
| House view editor | v0.11.0 at `/editor`. User took 3 photos (front left, center front, front right). Photos are shrunk to ≤1920 px JPEG in the browser and stored in `/files` on FFat. A section = a zone (or part of one, From/To) or a whole strand, drawn as a polyline on one photo; its lights are spaced evenly along it, first and last light on the end points. Live mode darkens the photo and draws glow + wall wash in the real colors from `/api/frame`. Tapping a dot can flash that light on the house. Layout in `/files/layout.json` (schema at the top of `editor_page.h`). |
| Live view | v0.5.0: page polls `GET /api/frame` (exact post-cap RGB per LED). Measured **9.4 updates/s**, dots match the real strand (2026-11-23). Fast effects look choppier on the page than on the lights (50 fps); accepted. WebSockets later if needed. |
| Effects | Must be deterministic (settings + elapsed time only, no randomness) so a browser simulation can reproduce them exactly. |
| Wi-Fi | Join home 2.4 GHz network; fall back to its own access point if that fails. Credentials in untracked `secrets.h`. |

## 2. Verified hardware and test results

### ESP32 board

- Purchased board listing: ESP32-S3 development board with **ESP32-S3-N16R8** module (AYWHP, USB-C).
- Listing claims **16 MB flash + 8 MB PSRAM**. Confirm actual detected memory/partitioning in firmware rather than assuming the entire flash is free for files.
- ESP32-S3 has 2.4 GHz Wi-Fi and BLE (not classic Bluetooth).
- **GPIO 8** was tested as LED DATA output.
- **Direct 3.3 V output from ESP32-S3 on GPIO 8 successfully drove the entire 65-light strand** with red, green, blue, white and off sequences. This proves bench operation, not guaranteed immunity to outdoor wiring length, temperature, or EMI.

### Existing lights

- Outdoor individually addressable puck-style eave lights originally used with **Lotus Lantern Pro** controller.
- **24 V DC** power distribution.
- Two planned branches/strands in a **Y topology**, each **65 physical LED pucks** (130 total). One branch has been directly tested; the other is installed outside and is presumed similar but untested with ESP32.
- White approximately 7 × 7 mm LED package, several nearby SMD resistors and small six-pin IC. No conclusive IC identity; avoid claiming a specific silicon driver.
- Original controller waveform measured with oscilloscope: nominal data HIGH around 5–5.5 V; **800 kHz**, **1.25 µs per bit**, short HIGH pulse about **0.35 µs**.
- Tested with **FastLED using `WS2811` chipset setting**, including chasing individual pucks successfully. Therefore the strand is **WS2811-protocol-compatible for practical control**.
- **Color order for FastLED: `RBG`** (verified by physical LED color mapping).
  - With incorrect `GRB` setting: requested red appeared blue, green appeared red, blue appeared green.
  - Corrected `RBG` setting produced the expected red, green, blue, white, off outputs on ESP32.
- Exactly **65 addressable pucks were inferred** by a moving-pixel test: index corresponding to the 65th puck illuminated the final light, while earlier count of 64 did not.
- Lights power up **white when 24 V is applied even if DATA is disconnected or held LOW**; valid serial pixel data overrides this. Do **not** assume an un-driven data line means the lights will remain off after a power cycle. Implement explicit OFF frames during boot, and consider **power switching** if reliable physical off at startup is required.
- With no data signal and lights white, bench PSU current previously observed around **1.2 A at 24 V** for one branch (earlier recollection was higher; treat measured values below as best available and recheck).

### Current readings from one 65-light strand

Measured at 24 V while running FastLED color fade/hold tests:

| Display | Programmed maximum brightness | Measured current | Approximate power |
|---|---:|---:|---:|
| Red | 240 / 255 | 0.9 A | 21.6 W |
| Green | 240 / 255 | 0.7 A | 16.8 W |
| Blue | 240 / 255 | 0.9 A | 21.6 W |
| White (RGB combined) | 100 / 255 | 1.1 A | 26.4 W |

These are empirical, approximate **one-branch** readings, not a full-load maximum specification. Current for *all* custom per-pixel combinations and higher white levels is **unknown**. Do not extrapolate linearly or assume identical branches without measuring.

### Original controller findings

- Lotus Lantern Pro Bluetooth controller circuit was initially thought burned out after a short. Later debugging identified a **short in a detachable output connector/harness**, not necessarily in the PCB.
- Original controller PCB was subsequently powered, connected over Bluetooth and observed generating LED data normally.
- Positive 24 V input was found continuous to 24 V output on the original PCB.
- There is a Y-split waterproof connector harness with two branch connectors; suspected harness short was isolated during investigation. Do not reuse that damaged/shorted assembly until repaired and continuity-tested.
- Original controller is useful as a known-working protocol reference; **not needed** for direct ESP32 control.

### Confirmed test sketch

The ESP32-S3 tested successfully on **GPIO 8**, one strand of 65, FastLED WS2811, `RBG` color order, brightness limited to 80:

```cpp
#include <FastLED.h>

#define DATA_PIN 8
#define NUM_LEDS 65
#define BRIGHTNESS 80

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(1000);

  FastLED.addLeds<WS2811, DATA_PIN, RBG>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);
  Serial.println("ESP32-S3 3.3V DATA TEST");
}

void showColor(CRGB color, const char* name) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  Serial.println(name);
  delay(2000);
}

void loop() {
  showColor(CRGB::Red,   "RED");
  showColor(CRGB::Green, "GREEN");
  showColor(CRGB::Blue,  "BLUE");
  showColor(CRGB::White, "WHITE");
  showColor(CRGB::Black, "OFF");
}
```

**Arduino Nano reference sketch:** `Docs/Arduino_Nano_RGB_Fade_Test.ino` (removed from the working tree; retrieve with `git show 8189414:Docs/Arduino_Nano_RGB_Fade_Test.ino`) is a known-working **Nano** sketch (D6, 5 V logic, 65 pixels, WS2811, `RBG`). It eases each color in and out with per-color caps (red 220, green 240, blue 240, white 100). It is a reference for effects and color order only; its pin 6 does not apply to the ESP32.

**Caution:** The original Arduino Nano was reported to feel warm during early wiring tests. Later it operated a 65-light chase successfully. The ESP32 direct test succeeded, but permanent wiring must still use validated pinout, shared ground and proper protection.

## 3. Electrical architecture and procurement

**Planned** single ESP32-S3 controller with two independent DATA channels (possibly more later):

```text
                           regulated 24 V DC supply
                                     |
                               fused DC split
                        +------------+-------------+
                        |            |             |
                  24V -> 5V buck    branch A      branch B
                        |           +24V          +24V
                    ESP32-S3        65 pucks      65 pucks
                        |
          +-------------+-------------+
          |                           |
     GPIO 8 (tested)          GPIO 9 (pin tested)
          |                           |
      74AHCT125 buffer channels, 5 V powered (preferred for permanent use)
          |                           |
    ~220–330Ω series            ~220–330Ω series
          |                           |
      DATA branch A               DATA branch B

          ALL DC GROUNDS COMMON (ESP32, buck, level shifter, LEDs)
```

Parts selected/discussed:

- ESP32-S3-N16R8 development board: **already owned**.
- **SN74AHCT125N** quad non-inverting buffer / level shifter, powered at 5 V, accepts 3.3 V ESP32 logic: selected for permanent installation, even though direct 3.3 V worked on a short bench connection. Ground each used active-LOW `OE` pin; tie unused input/enable pins to defined levels. Add **0.1 µF decoupling capacitor** near VCC/GND.
- **24 V to regulated 5 V buck converter**, e.g. AKEYSRC 5 V 3 A module selected by user. Verify actual input range, 5 V output, loaded thermal performance, and correct ESP32 board 5 V entry pin.
- **~220–330Ω series resistor** for each LED DATA output near buffer/driver. For initial short bench testing the user omitted it successfully, but use it for a robust final installation.
- **24 V supply:** original PSU is **24 V/2 A**, insufficient to guarantee both branches at user's demonstrated levels. Discussed **24 V/5 A** as a candidate (120 W), subject to measuring worst-case draw for BOTH branches, wiring/connector ratings, and thermal margin. The supply *does not force* 5 A into the lights.
- **Separately fused DC branches** sized to protect wire gauge/connectors, plus a protected controller/buck branch; DC distribution, correct polarity, strain relief, weather-rated enclosure and outdoor wiring.
- Optional **3.3 V-compatible 38 kHz IR receiver** for original handheld IR remote, provided its actual carrier and codes can be decoded. Optional BLE control (ESP32-S3 BLE) and primary Wi-Fi browser control.

**Safety:** Never feed 24 V to ESP32 pins or the 74AHCT125. Do not presume blue/red/black harness colors always correspond to DATA/+24 V/GND on a connector until continuity-verified. Power down while changing wiring. The data input's 3.3 V success was a short bench test; use proper 5 V driver and signal-integrity testing before attaching lengthy installed house wiring. Ensure startup output is defined, including controller reboot while LED power is on.

## 4. Software vision: photo-based house LED designer

This is the **main product requirement**. Avoid reducing scope to sliders and preset buttons.

### House layout mode

- Import 1+ house photographs (front, porch, side, back); use the image(s) as a calibrated visual background.
- Zoom, pan, edit individual LED markers and drag or draw polylines corresponding to real cable routes.
- Option to auto-place N LED markers at evenly spaced positions along a drawn path, then edit each position manually.
- Assign metadata: physical output number, zero-based LED index, name, physical orientation, and optional notes.
- Show cable direction and numbered LEDs; highlight any duplicate or missing physical assignments.
- Save/load a named house layout. Support view-to-view mapping of the same physical LED where photographs overlap.

### Named zones and virtual paths

- Named zones: e.g. **Porch**, **Front**, **Garage**, **Side**, **Back**.
- Zone membership may contain **arbitrary, noncontiguous pixel indices across multiple physical outputs**.
- Virtual animation paths are **ordered lists of physical LED addresses** and may reverse, skip gaps, jump between sides, or repeat as appropriate.
- Layer/priority rules: e.g. Porch steady warm RGB-simulated white overrides a traveling effect on Front and Garage; a comet can leap over a reserved steady region.
- Support mirror, reverse, synchronized-outward-from-center, and continuous-through-the-house modes.

### Animation authoring

- Interactive preview drawn as glowing LED dots over the actual house image, with simulated blur/glow but hardware-accurate pixel colors.
- Play/pause/scrub, adjustable FPS, speed, brightness, gradients/colors, direction, tail length and loop boundaries.
- Editable static zones, chase/comet, breathing/fade, rainbow/wave, twinkle, seasonal patterns and later timelines/keyframes.
- Presets/playlists, transitions, schedules, and possibly per-zone brightness/power-budget limits.
- Separate **data-driven effect definitions** from **runtime algorithms**. User can make new presets/configurations without reflashing. Completely new native effect types still require code changes unless a bounded/sandboxed scripting or timeline engine is built.
- Preview should work in a desktop/browser editor even with the hardware offline.

### Connectivity and autonomy

- ESP32 hosts (or communicates with) a web API over local Wi-Fi for status, zones, effects, uploads, and live preview.
- The simplest architecture may be a **browser-based editor** served by ESP32 or a PC-hosted frontend with local API access. Evaluate PC browser file limits and performance; avoid attempting to render a full-resolution house photo in the ESP32 LED engine.
- Store compact **LED mapping, zone definitions, animation presets, configurations** in ESP32 flash via LittleFS. Full-resolution original photos may stay in PC/browser project files, with optional smaller preview assets on ESP32.
- Persist chosen effect on device and run autonomously without persistent browser/PC connection.
- Optional original IR remote controls power, brightness, scene selection, zone selection.
- OTA firmware updates later, with safe update/fallback design and flash-partition planning.

## 5. Internal model / schema suggestion

Separate physical addressing from the rendered image position:

```json
{
  "schemaVersion": 1,
  "outputs": [
    {"id": "left", "gpio": 8, "count": 65, "chipset": "WS2811", "colorOrder": "RBG"},
    {"id": "right", "gpio": null, "count": 65, "chipset": "WS2811", "colorOrder": "RBG"}
  ],
  "views": [
    {"id": "front", "image": "front-house.jpg", "width": 1920, "height": 1080}
  ],
  "pixels": [
    {"output": "left", "index": 0, "positions": [{"view": "front", "x": 0.15, "y": 0.45}]}
  ],
  "zones": [
    {"id": "porch", "members": [{"output": "left", "index": 0}]}
  ],
  "paths": [
    {"id": "around-house", "sequence": [{"output": "left", "index": 0}]}
  ]
}
```

This is a **schema example**, not a measured house layout. No actual house photos, zone boundaries, or final GPIO allocation have been supplied yet.

## 6. Technical recommendations for VS Code project

Suggested stack (not committed yet):

- **ESP32 firmware:** PlatformIO in VS Code, Arduino framework, C++ and FastLED. Confirm latest compatible versions and ESP32-S3 RMT driver behavior.
- **Web visual editor:** TypeScript + SVG/Canvas (or suitable lightweight framework), with house image as background and numbered LED overlays. Keep renderer and hardware API independent.
- **Configuration storage:** LittleFS JSON documents on ESP32; take flash partition/OTA needs into account. ESP32-S3-N16R8 listing: 16 MB flash, 8 MB PSRAM; verify at boot.
- **Communication:** HTTP JSON REST for config/status, optional WebSocket for live state, upload and preview. Do not require cloud or account signup.
- **File format:** Versioned, human-readable project config with physical output/pixel IDs; validation and backup/restore.

Prefer modular components such as:

```text
firmware/
  src/
    main.cpp
    led_outputs.*
    animation_engine.*
    layout_mapping.*
    config_store.*
    wifi_api.*
    optional_ir.*
web/
  src/
    layout_editor/
    animation_editor/
    simulation/
    api/
shared/
  schema/
README.md
```

This is a proposed structure only. Keep implementation incremental; do not build an overly large framework before basic LED/Wi-Fi control is stable.

## 7. Recommended milestones / acceptance checks

### Stage 1 — Create working project and verify hardware

- Initialize VS Code / PlatformIO project for **ESP32-S3-N16R8**.
- Confirm detected flash, PSRAM and Arduino framework settings.
- Replicate the **known-good GPIO 8, 65 pixels, WS2811, RBG** red/green/blue/white/off test.
- Make sure OFF is transmitted at boot; watch power-up behavior because bare LEDs default white.
- Protect power and data paths; log faults and supply measurements during testing.

### Stage 2 — Wi-Fi basic remote control

- Connect to user-provided local 2.4 GHz Wi-Fi (never hard-code credentials into Git).
- Simple local web UI / API for ON/OFF, RGB color, brightness and one smooth fade.
- ESP32 stores current state and continues locally when browser closes.

### Stage 3 — Two independent physical outputs

- Implement configurable output descriptors with left=65, right=65.
- Choose second **valid/uncommitted** GPIO after confirming the actual board pinout; don't consider previously suggested GPIO 17 as verified hardware.
- Test the installed second branch when available, including 5 V buffer, lead length, full data signal, and worst-case current; provide mirror vs independent mode.

### Stage 4 — Visual house editor

- Upload house photo(s).
- Draw wire path and auto-place LEDs.
- Edit and number exact markers; bind to physical output/index.
- Create named zones and cross-output virtual ordered paths.
- Save, reload and validate a project.

### Stage 5 — Simulation + effects

- Offline preview colored/animated LED markers against house photo(s).
- Static color, zones, fades, comet/chase, wave, cross-zone jumps, reverse and steady reserved regions.
- Frame-accurate simulation mapping matches ESP32 output.

### Stage 6 — Storage, synchronization and polish

- Upload layout and presets over Wi-Fi to LittleFS, no reflash for new presets.
- Add playlists/scheduling, optional IR, OTA with rollback considerations.
- Add power-budget constraints and per-zone/per-channel limits after empirical current measurements.

## 8. Open questions / must not assume

1. **Second strand:** Not yet tested with ESP32; installed at house. Confirm actual count and color order on the installed cable.
2. **Final wire topology:** House photo and exact wire directions/LED locations not yet supplied; don't invent zone addresses or route geometry.
3. **Power:** Confirm whether original 24 V/2 A was per strand; measure current for both branches at full intended RGB/white brightness, and size PSU/wires/fuses based on real ratings.
4. **Startup white:** Need a designed way to prevent unintended white during power application or controller boot (early OFF data and possibly switched 24 V load power). Avoid shorting signal lines to ground as a control method.
5. **GPIO:** ~~Second output pin TBD.~~ Resolved: GPIO 8 and GPIO 9 both tested with strand A (2026-11-23). Remaining: connect the installed strand B to GPIO 9 and test it.
6. **Signal integrity:** Direct 3.3 V succeeded on short bench wiring; prefer 74AHCT125 5 V buffer for long installed wires and test with oscilloscope.
7. **Weatherproofing:** Physical connectors/enclosure and waterproof routing are pending; damaged Y-splitter must not be reused until fixed.
8. **Remote:** Original IR carrier frequency/protocol not yet decoded. WLED is an alternative firmware if preferred, but a highly custom house-layout designer may warrant custom firmware/software.
9. **Flash:** Listed 16 MB flash / 8 MB PSRAM, but actual board memory and filesystem/OTA partitioning must be confirmed.

## 9. Coding assistant guidance

- Treat **hardware tests above as established**, distinguish them from proposed software behavior or unverified electrical assumptions.
- **Do not accidentally replace the tested `RBG` order with `GRB`.**
- Start with one verified branch on GPIO 8 and preserve a buildable/testable baseline after each stage.
- Keep **firmware, house layout data, simulation, and visual editor** logically separate.
- Support dynamic numbers of outputs/zones rather than baking 2 × 65 into every layer, while retaining this as the default config.
- For future builds, user preference is to **increment the application version with each build, commit changes to Git, and never push automatically**. Preserve unrelated edits. Set up versioning and repository conventions explicitly in the new project before applying this workflow.
- Don't store Wi-Fi passwords, secrets, or full-size photographs in source code.
- Prioritize safe hardware behavior and deterministic startup over advanced UI features initially.

---

**Suggested immediate VS Code task:** Create the PlatformIO ESP32-S3 firmware skeleton, compile and run the known-good 65-pixel GPIO 8 FastLED test, add a minimal HTTP endpoint for setting RGB color/brightness, and scaffold (but do not fully implement) the separate house-photo LED layout editor.
