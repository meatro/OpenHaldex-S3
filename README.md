# OpenHaldex-S3: DIY AWD Controller for VAG Vehicles

**QUICK ONE-CLICK WEB INSTALLER AT WWW.OPENHALDEX.DEV**

![OpenHaldex-S3 screenshot](https://openhaldex.dev/wp-content/uploads/2026/04/New-Project-2.webp)

OpenHaldex-S3 is an ESP32-S3 based AWD controller for Haldex-equipped Volkswagen and transverse Audi vehicles.

It is designed around inexpensive off-the-shelf hardware, inline installation at the factory Haldex connector, and a built-in web UI for setup, tuning, diagnostics, logging, and OTA updates.

The project supports Haldex generations **1, 2, 4, and 5**. VAG did not use Gen 3 Haldex in this application.

Designed to be easy to build, configure, and use, OpenHaldex-S3 requires no soldering or advanced electrical experience.

Current release: **v1.1**. This release includes Gen 5 / MQB support, parked sleep power saving for vehicles that keep the controller powered from battery after ignition-off, and expanded Haldex diagnostics.

## Open Source And Provenance

*Update: <https://github.com/Forbes-Automotive/OpenHaldex-C6/issues/38> - OpenHaldex-C6 removed the restrictive banner UI requirement and returned to the permissive MIT open-source model intended for the OpenHaldex lineage. Proper provenance and open-source terms have now been restored across the current public OpenHaldex lineage.*

OpenHaldex-S3 exists to keep OpenHaldex innovation public, inspectable, and reusable under permissive open-source terms.
- Inspectable Code: Every piece of logic we build is public and inspectable. "Open Source" means users can inspect, build, modify, and verify the software they run.
- Community-First: OpenHaldex-S3 keeps performance profiles, tooling, UI, and firmware behavior available as source for users to inspect, build, modify, and improve.
- Feature Origins: Advanced OpenHaldex-C6 feature families such as Expert Mode live map tracing, SavvyCAN/GVRET Wi-Fi bridging, CAN View UI, web/API control, and PlatformIO-based development were built and verified on the S3 platform before being adapted back into C6.

references: 
- <https://github.com/Forbes-Automotive/OpenHaldex-C6/issues/34>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6#acknowledgements>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6#licensing>
- <https://github.com/Forbes-Automotive/OpenHaldex-C6/blob/main/THIRD_PARTY_NOTICES.md>

## Public history

*Update: <https://github.com/Forbes-Automotive/OpenHaldex-C6/issues/34> - OpenHaldex-C6 source and MIT licensing have been restored. OpenHaldex-S3 is now also distributed under one permissive MIT license across firmware, web UI, bundled data assets, release metadata, and documentation. The history below remains as provenance for feature origins and source continuity.*

OpenHaldex-S3 is the public, verifiable source for many advanced OpenHaldex feature families later seen in commercial OpenHaldex products, including map control ("Expert Mode" in OpenHaldex-C6 V2.10), CAN View, GVRET/SavvyCAN-oriented tooling, the HTML/JS web interface, PlatformIO project structure, API control, Wi-Fi AP mode, and `openhaldex.local` navigation.

These links show OpenHaldex-S3 source being imported into the Forbes Automotive OpenHaldex-C6 repository two days after the S3 snapshot:

- 2026-02-06: OpenHaldex-S3 public source snapshot:
  https://github.com/meatro/OpenHaldex-S3/commit/a8cadb7266fecd6b7860d3fb774f48dd41994def
- 2026-02-08: Forbes Automotive commit titled `S3 port onto C6`:
  https://github.com/Forbes-Automotive/OpenHaldex-C6/compare/44b90f10902a2aceccbe0facd335db46700942ee...a4dc321cd999474471289aa5769c75a0986b9455

*That import included OpenHaldex-S3 firmware source, web UI files, maps, CAN View code, PlatformIO configuration, and release metadata. That work later became part of the OpenHaldex-C6 V2.10 feature set, and subsequent GitHub issue discussions resulted in restored MIT licensing and clearer upstream provenance.*

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
2. OEM Haldex connector pair for your vehicle:
   - Gen 1: `1J0-973-814` to Haldex and `1J0-973-714` from vehicle
   - Gen 2 / Gen 4 / Gen 5: `1J0-973-713` to Haldex and `1J0-973-813` from vehicle

Typical hardware cost is under $50 if using the LilyGo board and used OEM connectors.

The intended installation does not require soldering, splicing factory vehicle wiring, or a proprietary controller harness.

## Installation

Basic install:

1. Lift the rear seat.
2. Unplug the factory Haldex connector.
3. Plug OpenHaldex-S3 inline.
4. Put the seat back down.

Removing the controller restores the factory connection path.

## Wiring And Harness

OpenHaldex-S3 is wired as an inline harness. Power and non-CAN signals pass through to the Haldex module, while the chassis CAN pair enters the LilyGo T-2CAN and the Haldex CAN pair leaves the LilyGo T-2CAN.

Confirm every pin against the factory wiring diagram for your exact chassis and Haldex generation before powering the board. Gen 1 uses an 8-pin connector. Gen 2, Gen 4, and Gen 5 use the same 6-pin connector shell, but the pinout is not the same across every generation.

### LilyGo T-2CAN Terminals

1. 12-24V -> fused Haldex controller power branch.
2. GND -> Haldex controller ground branch.
3. CANLA -> vehicle / chassis CAN low.
4. CANHA -> vehicle / chassis CAN high.
5. CANLB -> Haldex module CAN low.
6. CANHB -> Haldex module CAN high.

Summary:

- Vehicle / chassis CAN goes to CANHA/CANLA.
- Haldex module CAN goes to CANHB/CANLB.
- Power and ground stay connected to the Haldex module and are branched to the LilyGo.
- Non-CAN vehicle signals pass through unless you are intentionally adding external Gen 1 hardware.
- Keep CAN pairs twisted as long as practical.
- Do not undersize 12V power or ground wiring.

### Gen 1 8-Pin Connector Reference

Gen 1 is the only Haldex generation in this README that uses the 8-pin connector pair. Do not use this pinout for Gen 2, Gen 4, or Gen 5.

### Vehicle Connector (`1J0-973-714`)

1. Term15 -> Haldex pin 1.
2. Ground -> Haldex pin 2 and LilyGo GND.
3. Brake light -> Haldex pin 3.
4. Handbrake -> Haldex pin 4.
5. K-Line -> Haldex pin 5.
6. N/A -> not used.
7. Chassis CAN L -> LilyGo CANLA.
8. Chassis CAN H -> LilyGo CANHA.

### Haldex Connector (`1J0-973-814`)

1. Term15 -> Vehicle pin 1.
2. Ground -> Vehicle pin 2.
3. Brake light -> Vehicle pin 3.
4. Handbrake -> Vehicle pin 4.
5. K-Line -> Vehicle pin 5.
6. N/A -> not used.
7. Haldex CAN L -> LilyGo CANLB.
8. Haldex CAN H -> LilyGo CANHB.

Summary:

- Term15, brake light, handbrake, and K-Line pass through.
- Ground passes through and branches to LilyGo GND.
- Use the correct fused controller power source for LilyGo 12-24V and verify it before connecting the board.
- OpenHaldex-S3 does not include built-in high-side brake or handbrake output drivers.

### Gen 2 / PQ 6-Pin Example

This example uses the common Mk5 R32 / PQ style Haldex connector wiring. Confirm wiring for your own vehicle before building a harness.

### Haldex Connector (`1J0-973-713`)

1. 12V power -> Vehicle pin 1 and LilyGo 12-24V (BK/VI)
2. Ground -> Vehicle pin 2 and LilyGo GND (BR)
3. Brake -> Vehicle pin 3 (BK/RD)
4. CAN L -> LilyGo CANLB (OR/BR)
5. CAN H -> LilyGo CANHB (OR/BK)

### Vehicle Connector (`1J0-973-813`)

1. 12V power -> Haldex pin 1 and LilyGo 12-24V (BK/VI)
2. Ground -> Haldex pin 2 and LilyGo GND (BR)
3. Brake -> Haldex pin 3 (BK/RD)
4. CAN L -> LilyGo CANLA (OR/BR)
5. CAN H -> LilyGo CANHA (OR/BK)

Summary:

- The 12V feed passes through and branches to LilyGo 12-24V.
- Ground passes through and branches to LilyGo GND.
- Brake and handbrake style signals pass through. OpenHaldex-S3 does not interrupt them.
- Chassis CAN enters CANHA/CANLA.
- Haldex CAN returns from CANHB/CANLB.

### Gen 4 / Gen 5 6-Pin Example

This example uses the common Term15 / ground / Term30 / CAN pair layout used on later Haldex controllers. Confirm the pinout before using this on any specific Gen 4 or Gen 5 vehicle.

### Vehicle Connector (`1J0-973-813`)

1. Term15 -> Haldex pin 1.
2. Ground -> Haldex pin 2 and LilyGo GND.
3. Term30 -> Haldex pin 3 and LilyGo 12-24V.
4. N/A -> not used.
5. Chassis CAN L -> LilyGo CANLA.
6. Chassis CAN H -> LilyGo CANHA.

### Haldex Connector (`1J0-973-713`)

1. Term15 -> Vehicle pin 1.
2. Ground / MALT -> Vehicle pin 2.
3. Term30 -> Vehicle pin 3.
4. N/A -> not used.
5. Haldex CAN L -> LilyGo CANLB.
6. Haldex CAN H -> LilyGo CANHB.

Summary:

- Term15 passes through to the Haldex module.
- Term30 passes through and branches to LilyGo 12-24V.
- Ground / MALT passes through and branches to LilyGo GND.
- Chassis CAN enters CANHA/CANLA.
- Haldex CAN returns from CANHB/CANLB.
- On Gen 5 vehicles, Term30 can remain powered after ignition-off. Enable Power Save after setup.

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

OpenHaldex-S3 is part of the wider OpenHaldex project lineage.

- A Banging Donk originated the OpenHaldex platform with OpenHaldexT4 and the original Gen 1 Haldex control work.
- Forbes Automotive / OpenHaldex-C6 built on that lineage and reverse-engineered substantial Gen 2, Gen 4, and Gen 5 Haldex CAN frame behavior. Forbes Automotive also introduced the learned Haldex output calibration table concept.
- OpenHaldex-S3 adapts and extends the OpenHaldex platform for ESP32-S3 / LilyGO T-2CAN hardware, adding the full HTML/CSS/JavaScript web UI, local name-service access, HTTP API control, PlatformIO project structure, live map editing and interpolation, GVRET/SavvyCAN-oriented tooling, CAN View, DBC integration, UDS/KWP/TP2.0 diagnostic functions, parked sleep power saving, OTA/release workflow, and more.

The project history is not a simple one-way fork chain. A practical summary is:

`OpenHaldexT4 -> OpenHaldex-C6 -> OpenHaldex-S3 -> OpenHaldex-C6`

OpenHaldex-C6 is downstream from A Banging Donk's OpenHaldexT4 work for the original platform lineage. OpenHaldex-S3 is downstream from that early C6 work for Gen 2/4/5 Haldex support. OpenHaldex-C6 then imported OpenHaldex-S3 work back into C6 on 2026-02-08 in the `S3 port onto C6` commit, including S3-derived firmware, web UI, maps, CAN View, PlatformIO structure, API control, Wi-Fi/web workflow, and release metadata. That later import makes OpenHaldex-C6 downstream from OpenHaldex-S3 for those modern feature families, even though C6 also remains upstream for Gen 2/4/5 CAN reverse-engineering and learned calibration-table work.

## Licensing

OpenHaldex-S3 is licensed under the permissive MIT License.

The firmware, web UI, bundled data assets, PlatformIO project structure, release metadata, and documentation are available under MIT terms. You may use, copy, modify, publish, distribute, sublicense, and/or sell copies of the software, provided the copyright and license notices are preserved.

Third-party provenance and upstream acknowledgements are documented in `THIRD_PARTY_NOTICES.md`.

See:

- `LICENSE`
- `THIRD_PARTY_NOTICES.md`
- `data/LICENSE.md`
