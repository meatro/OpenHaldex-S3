# Third-Party Notices

This repository contains material from multiple upstream sources.

## OpenHaldex

- Upstream project: OpenHaldex by BangingDonk
- License basis in this repository: MIT, where applicable

## OpenHaldex-C6 / Forbes Automotive

- Upstream project: OpenHaldex-C6 by Forbes Automotive
- License basis in this repository for identified C6-derived portions:
  Forbes Automotive Source-Available License (FASL) v1.0
- License text location:
  `THIRD_PARTY_LICENSES/Forbes-Automotive-FASL-v1.0.txt`

This repository treats only the Gen 5 / C6-derived additions as subject to
the FASL. Earlier OpenHaldex-S3 work authored independently before the
March 4, 2026 introduction of the public C6 FASL is not reclassified by this
notice.

Current mixed-source files containing Gen 5 / C6-derived material include:

- `include/functions/can/can_id.h`
  Gen 5 CAN ID additions only.
- `include/functions/can/standalone_can.h`
  Gen 5 counters, checksum declarations, and related standalone support only.
- `include/functions/io/frames.h`
  Gen 5 frame declarations only.
- `src/functions/can/can_rx.cpp`
  Gen 5-specific receive/decode portions only.
- `src/functions/can/lock_data.cpp`
  Gen 5 bridge/mutation logic.
- `src/functions/can/standalone_can.cpp`
  Gen 5 checksum helpers, lookup tables, and related standalone support.
- `src/functions/canview/vw_mqb.dbc`
  MQB DBC asset used for Gen 5 decode support.
- `src/functions/canview/vw_mqb_chassis_dbc.cpp`
  Generated MQB signal table derived from the Gen 5 MQB DBC asset.
- `src/functions/io/frames.cpp`
  Gen 5 standalone frame generation only.
- `src/functions/tasks/tasks.cpp`
  Gen 5 task dispatch only.

For mixed-source files, only the identified Gen 5 / C6-derived portions are
treated as FASL-covered. Unrelated OpenHaldex-S3-original portions remain
under the repository's stated license structure.

## OpenHaldex-S3 UI

- Path: `data/`
- License text location: `data/LICENSE.md`

The UI is separately licensed from the rest of the repository.
