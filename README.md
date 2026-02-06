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

Firmware installation is only required once. All updates after that are performed **over-the-air** through the control panel.

---

## Web UI

After flashing, the device hosts its own Wi-Fi access point **or** can connect to your phone hotspot (to retain internet access).

- If hotspot credentials are saved, it tries **STA** for ~15s.
  - On success, browse to **http://openhaldex.local**
- If STA fails or no creds are set, it falls back to **AP**.
  - Connect to the OpenHaldex-S3 AP
  - Open **http://192.168.4.1/**

Available pages:
- Live map editor (9x7 speed/throttle)
- Map selector (home + editor)
- CAN viewer with decoded PQ signals
- OTA firmware updates

No apps, no serial adapters, no special tools.

---

## Maps

- Current map is saved at `/maps/current.json` in LittleFS.
- Saved maps are `/maps/<name>.json`.
- Bundled TXT maps (read-only) ship in the filesystem root:
  - `conservative.txt`
  - `aggressive.txt`
  - `danger.txt`

---

## Mk5 VW R32 wiring

Wiring takes about 5-10 minutes. The hardest part is finding a screwdriver.

### Haldex Connector (VW 1J0-973-713)
1. 12V Power	→ LilyGo 12–24V (BK/VI)
2. Ground	→ LilyGo GND (BR)
3. Brake	→ Vehicle pin 3 (BK/RD)
5. CAN L	→ LilyGo CANLB (OR/BR)
6. CAN H	→ LilyGo CANHB (OR/BK)

### Vehicle Connector (VW 1J0-973-813)
1. 12V Power	→ LilyGo 12–24V (BK/VI)
2. Ground	→ LilyGo GND (BR)
3. Brake	→ Haldex pin 3 (BK/RD)
5. CAN L	→ LilyGo CANLA (OR/BR)
6. CAN H	→ LilyGo CANHA (OR/BK)

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