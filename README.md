# OpenHaldex-S3: DIY AWD Controller for VAG Vehicles

**QUICK ONE-CLICK WEB INSTALLER AT WWW.OPENHALDEX.DEV**

![OpenHaldex-S3 screenshot](https://openhaldex.dev/wp-content/uploads/2026/04/New-Project-2.webp)

OpenHaldex-S3 is an ESP32-S3 based AWD controller for Haldex-equipped Volkswagen and transverse Audi vehicles.

It is designed around inexpensive off-the-shelf hardware, inline installation at the factory Haldex connector, and a built-in web UI for setup, tuning, diagnostics, logging, and OTA updates.

The project supports Haldex generations **1, 2, 4, and 5**. VAG did not use Gen 3 Haldex in this application.

## Status

This repository is prepared for the v1.0 release. The current release metadata is kept in `version.json` and injected into the firmware build by `scripts/version.py`.

The controller logic has been road-tested heavily on Gen 2 and now includes Gen 5 / MQB support. As with any drivetrain controller, verify all settings on your own vehicle before aggressive driving.

## What It Does

OpenHaldex-S3 sits inline between the vehicle chassis CAN bus and the Haldex control module. It can bridge stock traffic unchanged or modify selected CAN signals to request a different Haldex lock behavior.

Main features:

- Haldex Gen 1, 2, 4, and 5 support
- PQ chassis DBC support for Gen 1/2/4
- MQB chassis DBC support for Gen 5
- Home dashboard with live mode, ratio, telemetry, and 8 user-selectable signal tiles
- Setup page for generation selection, input mapping, dashboard signals, learned calibration, and CAN mode trigger
- Map mode with editable 2D lock table, custom bins, map import/export, and saved map selection
- Speed, Throttle, and RPM curve modes
- Fixed lock presets and stock/off pass-through behavior
- Shared mode behavior gates:
  - disengage under speed
  - minimum throttle before lock request
  - disable lock above speed
  - lock release ramp
  - controller output enable
  - Haldex status broadcast
- Learned Haldex output calibration table
- Configurable CAN mode trigger, defaulting to the factory ESP/traction button signal after generation selection
- CAN View with decoded/raw frames, filtering, bus selection, row origin markers, 30 second dump, and diagnostic capture mode
- Diagnostics, logs, Wi-Fi settings, and OTA updates from the built-in web UI

## Safety

This project changes drivetrain behavior. Use it accordingly.

- Select the correct Haldex generation before saving setup.
- Calibrate the learned Haldex output table before relying on requested lock values.
- Verify mapped speed, throttle, and RPM inputs at idle and low speed.
- Do not tune while driving unless another person is operating the UI.
- Keep a known conservative map available.
- CAN View is for inspection and troubleshooting. It is not intended to replace a dedicated high-rate USB CAN interface.

## Hardware

Required:

1. LilyGo T-2CAN ESP32-S3
   <https://lilygo.cc/en-us/products/t-2can>
2. VW 6-pin Haldex connectors:
   - `1J0-973-713` to Haldex
   - `1J0-973-813` from vehicle

Typical hardware cost is under $50 if using the LilyGo board and used OEM connectors.

The intended installation does not require soldering, splicing factory vehicle wiring, or a proprietary controller harness.

## Installation

Basic install:

1. Lift the rear seat.
2. Unplug the factory 6-pin Haldex connector.
3. Plug OpenHaldex-S3 inline.
4. Put the seat back down.

Removing the controller restores the factory connection path.

## Mk5 VW R32 Wiring Example

This example uses the common Mk5 R32 / PQ style Haldex connector wiring. Confirm wiring for your own vehicle before building a harness.

### Haldex Connector (`1J0-973-713`)

1. 12V power -> LilyGo 12-24V (BK/VI)
2. Ground -> LilyGo GND (BR)
3. Brake -> Vehicle pin 3 (BK/RD)
4. CAN L -> LilyGo CANLB (OR/BR)
5. CAN H -> LilyGo CANHB (OR/BK)

### Vehicle Connector (`1J0-973-813`)

1. 12V power -> LilyGo 12-24V (BK/VI)
2. Ground -> LilyGo GND (BR)
3. Brake -> Haldex pin 3 (BK/RD)
4. CAN L -> LilyGo CANLA (OR/BR)
5. CAN H -> LilyGo CANHA (OR/BK)

Summary:

- Both 12V+ wires go to LilyGo 12-24V.
- Both ground wires go to LilyGo GND.
- Chassis CAN goes to CANHA/CANLA.
- Haldex CAN goes to CANHB/CANLB.
- Brake and handbrake wires pass through. OpenHaldex-S3 does not interrupt them.
- Keep CAN pairs twisted as long as practical.
- Do not undersize 12V power or ground wiring.

## First Setup

After flashing and connecting to the web UI:

1. Open **Setup**.
2. Select the correct Haldex generation: Gen 1, Gen 2, Gen 4, or Gen 5.
3. Save settings. This applies recommended default CAN inputs and mode trigger for that generation.
4. Calibrate the learned Haldex output table while Haldex CAN data is live.
5. Return to **Home** and select a mode.

The generation selector intentionally starts blank on first setup. Saved local/device settings take priority after a generation has been selected.

## Web UI Access

The controller can run as its own access point and can also connect to a saved phone hotspot or local Wi-Fi network.

- If STA credentials are saved and enabled, it attempts STA for about 15 seconds.
  - On success, browse to `http://openhaldex.local`.
- If STA fails or is not configured, it falls back to AP mode.
  - Connect to the OpenHaldex-S3 access point.
  - Browse to `http://192.168.4.1/` or `http://openhaldex.local`.

Pages:

- **Home**: mode selection, live ratio, status badges, and dashboard signal tiles
- **Map**: 2D requested-lock table, bins, shaping, saved maps, TXT import/export, mode behavior
- **Speed Settings**: requested lock by vehicle speed
- **Throttle Settings**: requested lock by throttle/pedal value
- **RPM Settings**: requested lock by engine speed
- **CAN View**: decoded/raw CAN inspection and short captures
- **Diagnostics**: controller, CAN, Haldex, frame, and network status
- **Logs**: runtime logs and logging controls
- **Setup**: generation, input mapping, dashboard mapping, CAN mode trigger, calibration
- **OTA**: firmware/filesystem updates and Wi-Fi settings
- **Help / About**: onboard documentation and project information

## Control Modes

### Lock

Applies a fixed requested lock level from the Home ratio control.

### Speed

Applies requested lock based on vehicle speed alone. Curve points are interpolated between saved speed bins.

### Throttle

Applies requested lock based on throttle or pedal input. This mode ties AWD response directly to driver demand.

### RPM

Applies requested lock based on engine speed.

### Map

Applies requested lock from a two-axis table. By default the axes use the mapped Speed and Throttle input slots.

Advanced users can assign those input slots to any decoded CAN signal in Setup, then edit map bins and table values around those signals.

Map tools include:

- saved map dropdown
- load/save/save-as/delete
- TXT download and upload for sharing maps
- table cell editing
- custom speed/throttle bins
- column shaping for quick map drafts
- shared mode behavior controls

### Off / Stock

Returns to stock/pass-through behavior.

## Setup Details

Setup has four main jobs:

- Select and save Haldex generation.
- Map decoded CAN signals to required control inputs.
- Assign decoded CAN signals to the 8 Home dashboard tiles.
- Configure the optional CAN mode trigger.

Recommended defaults are generation-specific:

- Gen 1/2/4 use PQ chassis signals.
- Gen 5 uses MQB chassis signals.

The CAN mode trigger can enable one selected mode when one decoded signal meets a condition. The default trigger is the ESP/traction button signal for the selected generation, but the user can replace it with another decoded CAN signal.

## CAN View

CAN View is useful for validating signals and short troubleshooting captures.

It includes:

- decoded signal table
- raw frame table
- chassis / Haldex / all bus filter
- token filtering by ID or signal name
- generated-frame row marking
- 30 second text dump
- diagnostic capture mode

It is not intended to be a full SavvyCAN replacement or a high-rate real-time Wi-Fi CAN interface. For that use case, use a dedicated USB CAN interface.

## Maps and Filesystem

Map storage lives in LittleFS.

- Active runtime map: `/maps/current.json`
- Saved custom maps: `/maps/<name>.json`
- Bundled TXT presets:
  - `fwd.txt`
  - `conservative.txt`
  - `aggressive.txt`
  - `danger.txt`

Loading a TXT map imports it into runtime and persists it through the current map path.

## Firmware Installation

For the simplest install, use the quick web installer at <https://openhaldex.dev>. The details below are for manual builds, release assets, and OTA behavior.

Firmware installation is required once. After that, updates can be performed through the OTA page.

The OTA updater checks:

`https://openhaldex.dev/release/s3/version.json`

The latest metadata points to versioned release assets such as:

- `https://openhaldex.dev/release/s3/v1.0/firmware.bin`
- `https://openhaldex.dev/release/s3/v1.0/littlefs.bin`

OTA can update both firmware and LittleFS when release assets are available.

## Build With PlatformIO

This project is pinned to known-good PlatformIO and ESP32 Arduino package versions.

1. Install VS Code and PlatformIO IDE.
2. Open this folder as a PlatformIO project.
3. Build:

   ```powershell
   platformio run -e lilygo-t2can-s3
   ```

4. Upload firmware, replacing `COM#` with the LilyGo serial port:

   ```powershell
   platformio run -e lilygo-t2can-s3 -t upload --upload-port COM#
   ```

5. Upload LittleFS:

   ```powershell
   platformio run -e lilygo-t2can-s3 -t uploadfs --upload-port COM#
   ```

Build configuration:

- Platform: `pioarduino/platform-espressif32`
- Board: `esp32-s3-devkitc1-n16r8`
- Flash: 16 MB
- Filesystem: LittleFS
- Main environment: `lilygo-t2can-s3`

## Project Layout

- `src/functions/api`: HTTP API handlers
- `src/functions/can`: CAN receive/transmit and frame mutation paths
- `src/functions/canview`: DBC decode tables and CAN View cache
- `src/functions/core`: runtime state, modes, maps, curves, and calculations
- `src/functions/net`: Wi-Fi and OTA update logic
- `src/functions/storage`: Preferences, LittleFS maps, logs, and calibration persistence
- `src/functions/tasks`: firmware tasks
- `src/functions/web`: web server and static file serving
- `include/functions`: public headers for the firmware modules
- `data`: LittleFS web UI and bundled maps
- `scripts`: PlatformIO helper scripts
- `.github/workflows`: release deployment automation

## Attribution

OpenHaldex-S3 builds on the original OpenHaldex project by BangingDonk and subsequent OpenHaldex-C6 work by Forbes Automotive.

This project adapts and extends that work for ESP32-S3 hardware, inline installation, live tuning, Gen 5 support, and additional telemetry.

## Licensing

OpenHaldex-S3 is a split-license repository.

- Original OpenHaldex-S3 firmware code and MIT-upstream code remain under MIT.
- The web UI in `data/` is separately licensed under `data/LICENSE.md`.
- Gen 5 support and identified OpenHaldex-C6-derived portions are distributed under the Forbes Automotive Source-Available License (FASL) v1.0 and are non-commercial only.

If you redistribute source or binaries that include Gen 5 support, keep the third-party notices and Forbes FASL text with the distribution.

See:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `THIRD_PARTY_LICENSES/Forbes-Automotive-FASL-v1.0.txt`
- `data/LICENSE.md`
