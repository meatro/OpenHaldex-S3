# OpenHaldex-S3: DIY AWD Controller for VAG Vehicles

**QUICK ONE-CLICK WEB INSTALLER AT WWW.OPENHALDEX.DEV**

![OpenHaldex-S3 screenshot](https://openhaldex.dev/wp-content/uploads/2026/04/New-Project-2.webp)

OpenHaldex-S3 is an ESP32-S3 based AWD controller for Haldex-equipped Volkswagen and transverse Audi vehicles.

It is designed around inexpensive off-the-shelf hardware, inline installation at the factory Haldex connector, and a built-in web UI for setup, tuning, diagnostics, logging, and OTA updates.

The project supports Haldex generations **1, 2, 4, and 5**. VAG did not use Gen 3 Haldex in this application.

Designed to be easy to build, configure, and use, OpenHaldex-S3 requires no soldering or advanced electrical experience.

Current release: **v1.1**. This release includes Gen 5 / MQB support, parked sleep power saving for vehicles that keep the controller powered from battery after ignition-off, and expanded Haldex diagnostics.

## The Truth About "Open Source" Haldex Controllers

*Update: <https://github.com/Forbes-Automotive/OpenHaldex-C6/issues/34> - MIT open source license and matching source code are available again at OpenHaldex-C6 repository. MIT open-source Licensing and provenance concerns are mostly resolved aside from a restrictive "OpenHaldex-C6" banner that intended to show origin of the UI and underlying features to be Forbes Automotive. Once this restriction is removed from OpenHaldex-C6 repository licensing guidelines, all open-source concerns raised below can be considered resolved.*

While other commercial OpenHaldex variants marketed on social media and web storefronts claim to be "fully open-source," their underlying code for advanced performance features is completely closed, distributing binary-only blobs under a restrictive Source-Available License (FASL v1.0). **OpenHaldex-S3 is the true home of transparent OpenHaldex innovation.**
- Inspectable Code: Every piece of logic we build is public and inspectable. "Open Source" is not simply a marketing term to sell proprietary custom hardware.
- Community-First: *We do not lock performance profiles behind compiled binaries to force hardware sales.* 
- Feature Origins: Advanced OpenHaldex-C6 features released by Forbes Automotive like Expert Mode live map-tracing, SavvyCAN/GVRET Wi-Fi bridging, and DBC decoding were originally built and verified right here on the S3 platform before being adapted commercially.

references: 
- <https://github.com/Forbes-Automotive/OpenHaldex-C6/issues/34>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6#acknowledgements>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6#licensing>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6/blob/main/THIRD_PARTY_NOTICES.md>

## Public history

OpenHaldex-S3 is the public, verifiable source for many advanced OpenHaldex feature families later seen in commercial OpenHaldex products, including map control ("Expert Mode" in OpenHaldex-C6 V2.10), CAN View, GVRET/SavvyCAN-oriented tooling, the HTML/JS web interface, PlatformIO project structure, API control, Wi-Fi AP mode, and `openhaldex.local` navigation.

These links show OpenHaldex-S3 source being imported into the Forbes Automotive OpenHaldex-C6 repository two days after the S3 snapshot:

- 2026-02-06: OpenHaldex-S3 public source snapshot:
  https://github.com/meatro/OpenHaldex-S3/commit/a8cadb7266fecd6b7860d3fb774f48dd41994def
- 2026-02-08: Forbes Automotive commit titled `S3 port onto C6`:
  https://github.com/Forbes-Automotive/OpenHaldex-C6/compare/44b90f10902a2aceccbe0facd335db46700942ee...a4dc321cd999474471289aa5769c75a0986b9455

*That import included OpenHaldex-S3 firmware source, web UI files, maps, CAN View code, PlatformIO configuration, and release metadata and later reworked into "OpenHaldex-C6 V2.10" for commercial release without retaining any licensing or proper attribution to any upstream contributors.*

**OpenHaldex-S3 is developed and vehicle-tested by a VW/Audi-certified electrical and diagnostic specialist using OEM diagnostic procedures, ODIS, measured values, CAN logging, and real vehicle validation.**

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
- Gen 5 parked sleep mode to reduce battery draw on vehicles with constant controller power
- Haldex Diagnostics page with UDS/KWP transport status, route probing, module identity, DTC read/clear, and Gen 5 decoded measured values
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

## Response to Common Claims About the LilyGO T-2CAN

OpenHaldex-C6 documentation makes specific claims about the LilyGO T-2CAN hardware used by OpenHaldex-S3. This section responds to those claims directly.

OpenHaldex-S3 is not trying to be a sealed commercial controller module. It is a public-source DIY OpenHaldex controller using inexpensive, documented, replaceable hardware.

| Claim | OpenHaldex-S3 response |
|---|---|
| **“The ‘proprietary PCB’ is equally as proprietary as the LILYGO T-2Can.”** | False. The LilyGO T-2CAN ESP32-S3 is an off-the-shelf dual-CAN development board sold by LILYGO as part of its Industrial Series. It is available from normal suppliers and is not tied to one individual vendor’s small-batch hardware stock, private build process, support inbox, or replacement supply. |
| **“The LILYGO T-2Can does not feature any suitable means of over-voltage or fused protections.”** | Misleading. This treats rare electrical abuse cases like normal vehicle operation. The LilyGO T-2CAN schematic shows a 12–24V input stage with an SS24 diode, filtering, and onboard regulation. Transient suppression can be added at the harness/controller input for a few dollars. A TVS diode does not make the small-batch PCB from Forbes automotive-grade, ISO certified, or superior. OpenHaldex-S3 has been real-road tested by a VW/Audi technician for nearly 20,000 miles without incident. |
| **“The LILYGO T-2Can does not feature an onboard mode LED - so no immediate user feedback of current mode.”** | True. OpenHaldex-S3 provides mode/state through CAN-based mode logic or web UI, not a LED in the backseat. |
| **“The LILYGO T-2Can does not offer external mode changing for quick ‘on-the-fly’ changes.”** | False. OpenHaldex-S3 can change modes from CAN signal logic. Mode switching can be tied to decoded vehicle signals such as selected gear, ESP button state, or any usable CAN data. A physical button can also be added if desired. |
| **“The LILYGO T-2Can does not offer additional high-sided drivers for brake/handbrake control on Gen1 or Gen2 systems.”** | True, but only applies to a very limited use case and it is not a requested option. It is also not configured in Forbes Automotive's OpenHaldex-C6 wiring harness. |
| **“The LILYGO T-2Can uses MS4553S as CAN interface chips - these have lower ESD protection and in high-voltage or short circuit situations will destroy their irreplaceable protection fuses.”** | False. The T-2CAN schematic shows TD501MCAN devices as the CAN transceivers connected to CANH/CANL. The MS4553S parts are in the logic-level interface path, not the CAN bus transceivers. The schematic also shows CAN-side protection/filtering components on both CAN channels. If the board is ever damaged, the entire T-2CAN can be replaced for $35. |
| **“No optional JTAG break-out (which could be repurposed for other features - like GPS, for example).”** | True and irrelevant to normal users. The T-2CAN supports USB-C programming and exposes GPIO access for users who actually want custom expansion. Normal users do not need a JTAG header. |
| **“No pre-designed enclosure.”** | True. OpenHaldex-S3 is a DIY controller platform. 3D print a case, or buy a small electronics enclosure at any normal vendor for a few dollars. It's a box. |
| **“Terminations are screw-type - which can lead to poor user installation / connections - leading to sporadic communication faults or failures within the vehicles CAN network - this could include no-starts or steering failures.”** | Misleading. Bad wiring can cause faults on any controller. Loose wires, wrong pinout, or a poorly built aftermarket harness can all cause problems. Screw terminals do not inherently cause faults. Tighten the terminals, build the harness correctly, secure the wiring, and enclose the controller. |
| **“It’s still a custom made piece of hardware including the custom wiring harness YOU’D need to make: collect connectors, crimp/solder or twist & tape, it won’t be pretty.”** | Misleading. OpenHaldex-S3 uses the factory Haldex connector pair and the T-2CAN. It does not require cutting the factory vehicle harness or soldering. The clean version uses OEM-style connectors, correct wire size, proper terminals, and an enclosure. |
| **“The cost is comparative.”** | False. A normal DIY OpenHaldex-S3 build is a $30 LilyGO T-2CAN and $20 Haldex connector pair. Typical DIY cost is nowhere near a $270 OpenHaldex-C6 controller plus separate $100 wiring harness, shipping, and fees. |

A commercial controller may be attractive for someone who wants a packaged product. That does not make the LilyGO T-2CAN unsuitable, unsafe, or technically inferior for OpenHaldex.

References:

- LilyGO T-2CAN project documentation: <https://github.com/Xinyuan-LilyGO/T-2Can>
- LilyGO Industrial Series: <https://lilygo.cc/en-us/collections/industrial-series>
- Microchip MCP2515 product page: <https://www.microchip.com/en-us/product/mcp2515>
- MCP2515 datasheet: <https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf>

## Haldex Diagnostics

The Diagnostics page includes controller status, CAN status, frame status, power state, network state, and direct Haldex module diagnostics.

Supported diagnostic functions:

- UDS route probing for Gen 5 Haldex modules
- Gen 5 module identity reads, including ASAM/ODX, part number, software version, hardware number, dataset, and VIN when supported by the module
- Gen 5 DTC read and DTC clear
- Gen 5 decoded measured values using a compact OpenHaldex MWB manifest generated from VDCore/ODIS data
- Gen 2/4 KWP2000 over VW TP2.0 DTC clear support
- Gen 2/4 raw KWP local-identifier measured-value reads for development and validation

Gen 5 measured values currently include high-value Haldex signals such as terminal 30 voltage, pump current, pump PWM, pump voltage, module temperature, finned temperature, clutch temperature, locking rate/state, temperature-duration counters, and pump calibration records where supported by the module profile.

## Gen 5 Power Saving

Gen 5 vehicles can keep the controller powered from battery at all times. OpenHaldex-S3 v1.1 adds parked sleep mode to reduce parked draw when Gen 5 is selected.

When parked sleep is enabled, the controller enters deep sleep after ignition-off CAN state remains inactive for the configured delay. Chassis CAN activity wakes the ESP32-S3, the firmware checks the Gen 5 KL15 ignition signal, and it either continues full startup or returns to sleep. Hold the LilyGo BOOT button during reset to bypass parked sleep and keep the access point available for service.

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
- **Diagnostics**: controller, CAN, Haldex, frame, power, network, DTC, identity, and measured-value diagnostics
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

- `https://openhaldex.dev/release/s3/v1.1/firmware.bin`
- `https://openhaldex.dev/release/s3/v1.1/littlefs.bin`

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

This project adapts and extends that work for ESP32-S3 hardware, adding inline installation, "Expert Mode" speed-throttle Haldex tuning, live map editing/trace, interpolation, API control, GVRET/SavvyCAN and CAN View, DBC integration, UDS/KWP/TP2.0 diagnostic functions, and more.

## Licensing

OpenHaldex-S3 is a split-license repository.

- Original OpenHaldex-S3 firmware code and MIT-upstream code remain under MIT.
- The web UI in `data/` is separately licensed under `data/LICENSE.md`.
- Gen 5 support is the only OpenHaldex-C6-derived portion distributed under the Forbes Automotive Source-Available License (FASL) v1.0 and is non-commercial only. All other functions and features are original to the OpenHaldex-S3 project and later added to OpenHaldex-C6. See "Public History" section for more info: <https://github.com/meatro/OpenHaldex-S3#public-history>

If you redistribute source or binaries that include Gen 5 support, keep the third-party notices and Forbes FASL text with the distribution.

See:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `THIRD_PARTY_LICENSES/Forbes-Automotive-FASL-v1.0.txt`
- `data/LICENSE.md`
