# Development history

These are the **development and test records**, copied unchanged from the development repository. They are kept for transparency and for anyone interested in how the firmware was built and tested. For using the project, start with the [README](../../README.md) and [`docs/guide/`](../guide/).

**Please note:**
- **Folder names refer to the development repository.** The records mention firmware folders such as `MediaJog_1a/` … `MediaJog_2a/`, `MediaJog_BLE_1_r4/` or `MediaJog_BLE_M1/`, and paths such as `<stage>/build/<stage>.ino.bin`. This public repository contains only `firmware/MediaJog_2a_2`, `firmware/MediaJog_1e_4` and `firmware/MediaJog_BLE_B1` (source only). Checksums quoted in the records are the same as in `firmware/*/SHA256SUMS.txt`.
- **Some records are partly in Japanese**, in particular `REPORT.md`.
- **Records are append-only.** A statement made early may be corrected in a later section of the same file. The latest section wins.
- **"Edgar"** is the project's AI reviewer (OpenAI GPT); "Claude" implemented and documented with the author.

| File | Content |
| --- | --- |
| `DEVELOPMENT_PLAN.md` | Plan, design invariants, open issues, overall status |
| `REPORT.md` | Chronological investigation and test log |
| `STAGE_1A.md` … `STAGE_1E_4.md` | USB firmware stages: YouTube keys, profiles, mouse and spinner, CUSTOM setup, HOLD, saved settings |
| `BLE_1.md` | First Bluetooth LE firmware (BLE-1 … r4), including the reconnection fix "B7" |
| `STAGE_2A_DESIGN.md`, `STAGE_2A.md` | USB / Bluetooth LE integration (Stage 2a) |
| `BLE_M1.md` | Bluetooth mouse: the library limitation and the second-service attempt |
| `BLE_B1_DESIGN.md`, `BLE_B1.md` | The fixed GATT table (BLE-B1) |
| `STAGE_2A_2_DESIGN.md`, `STAGE_2A_2.md` | Stage 2a-2: integration, host tests, hardware results, known open issues |
