# Third-Party Notices

OpenHaldex-S3 is licensed under the MIT License. This file records upstream
project provenance and attribution for work that OpenHaldex-S3 builds on or
interoperates with.

## OpenHaldex / A Banging Donk

- Upstream project: OpenHaldex / OpenHaldexT4 by A Banging Donk
- Repository: <https://github.com/ABangingDonk/OpenHaldexT4>
- License basis: permissive / MIT-compatible reuse as publicly stated by the
  original author and carried forward by the OpenHaldex project lineage.

OpenHaldex-S3 acknowledges A Banging Donk as the originator of the OpenHaldex
platform. The wider OpenHaldex ecosystem, including later C6 and S3 work,
traces back to the original OpenHaldexT4 project and its Gen 1 Haldex control
work.

## OpenHaldex-C6 / Forbes Automotive

- Upstream project: OpenHaldex-C6 by Forbes Automotive
- Repository: <https://github.com/Forbes-Automotive/OpenHaldex-C6>
- License basis: MIT License.

OpenHaldex-S3 acknowledges Forbes Automotive for reverse-engineering and
implementing substantial Gen 2, Gen 4, and Gen 5 Haldex CAN frame behavior, and
for introducing the learned Haldex output calibration table concept.
OpenHaldex-S3 includes and interoperates with this Haldex generation support
and related implementation knowledge where applicable.

## OpenHaldex-S3 / SpringfieldVW

- Project: OpenHaldex-S3 by SpringfieldVW / Chris "meatro"
- Repository: <https://github.com/meatro/OpenHaldex-S3>
- License basis: MIT License.

OpenHaldex-S3 includes firmware, web UI, data assets, PlatformIO project
structure, release metadata, and documentation under the MIT License.

OpenHaldex-S3's project-specific work includes the ESP32-S3 / LilyGO T-2CAN
port, full HTML/CSS/JavaScript web UI, LittleFS web app structure, local
name-service support, HTTP API control, live map editing and interpolation,
PlatformIO project structure, Wi-Fi setup flow, CAN View UI, GVRET /
SavvyCAN-oriented tooling, onboard diagnostics, OTA/update structure, parked
sleep power saving, and related user-facing development workflow.

## Lineage Summary

The OpenHaldex project history is bidirectional after the initial C6 and S3
work:

`OpenHaldexT4 -> OpenHaldex-C6 -> OpenHaldex-S3 -> OpenHaldex-C6`

OpenHaldex-C6 is downstream from A Banging Donk's OpenHaldexT4 work for the
original platform lineage. OpenHaldex-S3 is downstream from early C6 work for
Gen 2/4/5 Haldex support. OpenHaldex-C6 later imported OpenHaldex-S3-derived
work back into C6 on 2026-02-08 in the `S3 port onto C6` commit. As a result,
modern C6 feature families such as the web UI, maps, CAN View, API control,
PlatformIO structure, Wi-Fi/web workflow, and release metadata include
OpenHaldex-S3-derived lineage, while C6 remains credited for Gen 2/4/5 CAN
reverse-engineering and learned calibration-table work.
