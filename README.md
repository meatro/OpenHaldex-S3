# OpenHaldex-S3

### A free, open-source Haldex controller built on ~$50 of off-the-shelf hardware

**Lift seat ? plug in ? done.**  
No jacks. No harness. No permanent changes.

OpenHaldex-S3 is a **map-driven AWD controller** for Gen 1, 2, and 4 Haldex-equipped VW and transverse Audi vehicles.

It is intentionally inexpensive, fast to install, and brutally simple.

All that is required is a LilyGo T-2CAN development board, two connectors, and the free software in this repository.

---

## Why this exists

I've owned a Mk5 VW R32 since 2021 and have spent years poking the Haldex system with sticks trying to make it behave differently.

In late 2025 I found **OpenHaldex-C6** by Forbes Automotive. The software was a good starting point, but I found the engagement too harsh for my liking, as their controller is effectively on/off and customization was very basic. In addition, their controller hardware presented some hurdles:

- The controller was frequently sold out
- Every part of the hardware was custom made
- Including the required custom wiring harness

As a factory technician, replacing OEM wiring without a clear benefit doesn't excite me. Extending CAN several feet just to add a button didn't make sense.

So I built the same controller logic on inexpensive, readily available hardware, kept the factory wiring intact, and made the install trivial.

---

## The install

1. Lift the rear seat
2. Unplug the factory 6-pin Haldex connector
3. Plug OpenHaldex-S3 inline
4. Put the seat back down

That's the entire install.

I installed mine in about five minutes, in the snow, with no lift. Uninstalling is the same process in reverse.

One connector. Stock wiring immediately restored.  
You can move the same unit between cars in minutes and leave **zero trace**.

---

## Hardware required

1. **LilyGo T-2CAN ESP32-S3**  
   https://lilygo.cc/en-us/products/t-2can
2. **VW connectors**
   - 1J0-973-713 (to Haldex)
   - 1J0-973-813 (from vehicle)

I used junkyard Tiguan 4Motion connectors. Re-pinning instructions are on Google or YouTube, if needed. (If you are not familiar with VAG connectors, learn before disassembling them!)

Total cost is under **$50**.

- No soldering
- No proprietary PCB
- No splicing
- No custom harness
- No locked ecosystem

---

## Firmware installation

Firmware installation is only required once. All updates after that are performed **over-the-air** through the control panel. OTA checks `https://www.springfieldvw.com/openhaldex-s3/version.json` and installs both firmware + LittleFS when available.

---

## Web UI

After flashing, the device hosts its own Wi-Fi access point **and/or** can connect to your phone hotspot (to retain internet access).

- If hotspot credentials are saved, it tries **STA** for ~15s.
  - On success, browse to **http://openhaldex.local**
- If STA fails or no creds are set, it falls back to **AP**.
  - Connect to the OpenHaldex-S3 AP
  - Open **http://192.168.4.1/** or **http://openhaldex.local**

Available pages:

- Home dashboard (controller toggle, live gauges, engagement bar)
- Live map editor (9x7 speed/throttle with active trace)
- CAN viewer (decoded + raw, bus filter, token filter, diagnostic presets)
- Diagnostics (controller/CAN/frame/network status)
- OTA updater (check/install progress)

CAN viewer extras:

- 30s downloadable text dump (`/api/canview/dump`)
- Safe diagnostic capture mode (`/api/canview/capture`) that forces Controller OFF + Broadcast ON while active

No apps, no serial adapters, no special tools.

---

## Maps

- Current map is saved at `/maps/current.json` in LittleFS.
- Saved maps are `/maps/<name>.json`.
- Bundled TXT maps (read-only) ship in /maps:
  - `fwd.txt`
  - `conservative.txt`
  - `aggressive.txt`
  - `danger.txt`

---

## Mk5 VW R32 wiring

Wiring takes about 5-10 minutes. The hardest part is finding a screwdriver.

### Haldex Connector (VW 1J0-973-713)

1. 12V Power -> LilyGo 12-24V (BK/VI)
2. Ground -> LilyGo GND (BR)
3. Brake -> Vehicle pin 3 (BK/RD)
4. CAN L -> LilyGo CANLB (OR/BR)
5. CAN H -> LilyGo CANHB (OR/BK)

### Vehicle Connector (VW 1J0-973-813)

1. 12V Power -> LilyGo 12-24V (BK/VI)
2. Ground -> LilyGo GND (BR)
3. Brake -> Haldex pin 3 (BK/RD)
4. CAN L -> LilyGo CANLA (OR/BR)
5. CAN H -> LilyGo CANHA (OR/BK)

Summary: **Both** 12V+ wires from connectors go to LilyGo 12V+. **Both** 12V- wires from connectors go to LilyGo GND. Chassis connector CAN H/L goes to CANHA/CANLA. Haldex connector CAN H/L goes to CANHB/CANLB. Splice the brake wires from both connectors together. If you have handbrake, splice those together as well. (OpenHaldex-S3 does not interrupt the brake or handbrake wires.)

Note: 12V power and ground wires are 0.5MM to carry the required power to the differential. Do not use smaller wires. CAN wires should remain twisted pairs as long as possible.

---

## Build (PlatformIO)

This project is pinned to known-good PlatformIO + ESP32 Arduino versions.

1. Install VS Code + PlatformIO IDE.
2. Open this folder (`openhaldex-s3-pio`) as a PlatformIO project.
3. Build (replace COM# with the LilyGo's COM port. Example, COM13):
   - `platformio run -e lilygo-t2can-s3`
4. Upload firmware:
   - `platformio run -e lilygo-t2can-s3 -t upload --upload-port COM#`
5. Upload LittleFS:
   - `platformio run -e lilygo-t2can-s3 -t uploadfs --upload-port COM#`

---

## Attribution

OpenHaldex-S3 builds upon the original OpenHaldex project by BangingDonk
(MIT License) and subsequent OpenHaldex-C6 work by Forbes Automotive.

This project adapts and extends that work for ESP32-S3 hardware, inline
installation, live tuning, and additional telemetry.

---

## Technical Changelog (Rolling)

This section tracks engineering-level changes made during the S3 port and stabilization work.

### Platform + Build System

- Migrated and pinned the project to PlatformIO with explicit ESP32 package versions for reproducible builds.
- Targeted board profile moved to `esp32-s3-devkitc1-n16r8` with explicit 16 MB flash configuration.
- Locked custom partitions + LittleFS in `platformio.ini`:
  - `board_build.flash_size = 16MB`
  - `board_build.partitions = partitions.csv`
  - `board_build.filesystem = littlefs`
- Added `scripts/version.py` pre-build hook to inject firmware version metadata from `version.json` into firmware APIs/UI.

### Project Architecture Refactor

- Refactored from monolithic Arduino-style layout to modular C++ structure under `src/functions/*` and `include/functions/*`.
- Major subsystem split:
  - `can`, `canview`, `core`, `api`, `net`, `storage`, `tasks`, `web`, `io`.
- Centralized runtime state and telemetry in `core/state` to reduce duplicated globals and simplify API/status output.

### CAN Stack + Bus Roles

- Implemented dual-bus handling for LilyGo T-2CAN-S3:
  - Chassis bus via MCP2515
  - Haldex bus via ESP32 TWAI
- Added explicit bridge TX path tagging for generated vs bridged frames.
- Added frame-level diagnostics for last sent key frames (motor/brake IDs) including:
  - payload bytes
  - generated/bridged classification
  - frame age

### Controller/Map Logic

- Integrated 2D map-driven lock control path with bilinear interpolation:
  - speed bins x throttle bins
  - editable lock table
- Implemented mode behavior simplification around controller enabled/disabled workflow:
  - Controller disabled -> stock/bridge behavior
  - Controller enabled -> map-driven behavior
- Preserved lock transformation helpers and frame mutation model from known working OpenHaldex lineages while adapting for S3 bus architecture.

### Map Storage + Filesystem

- LittleFS mount and map persistence hardened.
- Active runtime map stored as `current.json` and persisted under `/maps`.
- Added map CRUD APIs and UI controls:
  - list, load, save, delete
- Bundled default maps delivered in FS image (`fwd/conservative/aggressive/danger`) for immediate first boot usability.

### Web API Surface

- Expanded HTTP API set for runtime control and tooling:
  - `/api/status`
  - `/api/settings`
  - `/api/map`
  - `/api/maps` + load/save/delete endpoints
  - `/api/wifi`
  - `/api/network`
  - `/api/update`, `/api/update/check`, `/api/update/install`
  - `/api/canview`
  - `/api/canview/dump`
  - `/api/canview/capture`
- Normalized status payloads for UI pages (home/map/diag/canview/ota) to consume one consistent telemetry contract.

### CAN Viewer Overhaul

- Rebuilt CAN viewer into decoded + raw tables with consistent polling and redraw model.
- Added bus filtering (`all`, `chassis`, `haldex`) and search filtering.
- Added TX/RX display separation in rows and generation tagging (`generated` flag).
- Added row highlighting for generated frames to distinguish OpenHaldex-injected traffic from pass-through traffic.
- Added 30-second downloadable capture workflow via `/api/canview/dump` with bus filter support.
- Added diagnostic preset dropdown that appends known tokens into the same space-delimited filter input.
- Added safe capture mode toggle that temporarily locks runtime behavior for repeatable diagnostics.

### Diagnostics Improvements

- Added structured diagnostics blocks for:
  - controller state and lock request/actual values
  - CAN health (ready/failure/last seen ages)
  - frame-level generation/bridge verification
  - network state
- Added/updated state label translation path for more readable diagnostics output.

### UI/Frontend Consolidation

- Consolidated duplicated inline CSS/JS into shared `data/styles.css` and `data/scripts.js`.
- Added consistent top navigation across pages.
- Added main page live telemetry visuals:
  - speed/throttle gauges (responsive sizing)
  - engagement bar
- Added map live-trace improvements and active-cell visibility tuning.
- Added UX consistency updates for controller toggle semantics and diagnostics naming.

### Networking (AP/STA + mDNS)

- Implemented AP/STA fallback model:
  - attempt STA (hotspot creds) with timeout window
  - fall back to AP if STA unavailable
- Added persistent Wi-Fi credentials + STA enable state via Preferences storage.
- Added network status reporting endpoint and UI block for mode/IP/hostname/internet state.
- Added mDNS support for `openhaldex.local` and AP IP fallback workflows.

### OTA Update Pipeline

- Implemented OTA check/install subsystem in `functions/net/update`:
  - version check from hosted `version.json` (`springfieldvw.com/openhaldex-s3`)
  - firmware + filesystem dual-update flow
  - HTTP redirect handling
  - timeout + stream/write error handling
  - staged progress reporting (bytes done/total, speed, stage)
- Added OTA UI integration with online/offline status and update state.

### Web Installer + Distribution

- Added ESP Web Tools manifest (`manifest.json`) for direct browser flashing with explicit offsets:
  - bootloader
  - partitions
  - firmware
  - littlefs
- Stabilized installer paths to hosted website assets to avoid cross-origin redirect issues during binary fetch.

### Release Automation

- Added GitHub Actions workflow: `.github/workflows/release-sync-website.yml`
- Workflow behavior:
  - trigger on release events (`published/prereleased/released/edited`) or manual dispatch with tag
  - download release assets
  - copy manifest
  - verify payload
  - FTPS deploy to website target directory
  - clean-slate upload option for deterministic website installer payloads

### Licensing + Provenance

- Added MIT license file with explicit project attribution and provenance notes.
- Documented upstream lineage and retained attribution references in repository materials.

### Known Validation Focus Areas

- Gen2 interpretation of certain Haldex feedback signals remains under active validation against real-world vehicle behavior.
- Generated-frame vs measured-response correlation tooling is now in place (CAN dump + generated frame markers) for iterative road-test debugging.
