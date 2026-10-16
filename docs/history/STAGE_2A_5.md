# Stage 2a-5 — Colour-coded operation guides

Created: 2026-10-16 · Implementation: Claude Opus 5.5 · Request: the author (Stage 2a-5 instructions)

**Status: built and host-tested. NOT tested on hardware** (the author will check the display on the device). The 2a-4 hardware results (`STAGE_2A_4.md` §11) are **not** results for 2a-5.

- Base: `MediaJog_2a_4` (hardware-tested; **unchanged**, checksums verified OK).
- New folder `MediaJog_2a_5/`, build id `2a-5`, screen tag `USB 2a5` / `BLE 2a5`.

## 1. Purpose

Display only. On the small screen, the key operation and its meaning were drawn in the same colour (for example `Fn+S: SETUP` all white or all yellow). Stage 2a-5 colours the operation and the description differently, the same way on every screen. No function is added and no logic is changed.

## 2. Colour rule

| Element | Colour | Example |
| --- | --- | --- |
| Operation: a key, a key combination, or a knob action | **cyan** (`TFT_CYAN`, already used for the screen titles) | `Fn+S`, `Fn+;`, `1-4/turn`, `knob press`, `ok (Enter)`, `esc (`)`, `del`, `t` |
| Description | **white** (`TFT_WHITE`) | `: SETUP`, ` Ctl`, `: SAVE`, `: undo` |
| Selection, values, status, profile names, notes | unchanged | row highlight, `64 (12/14)`, `DIR: NORMAL`, `NV OK`, `NOT CONFIGURED`, the last event |
| Warnings and errors | unchanged | `GATT REG rc…` (red), `! OLD … RELEASE UNCONFIRMED` |

- Only operation guides are coloured. Values and status lines are not split.
- The SETUP row labels (`1 CW`, `1 SENS`, …) are row names with the existing selection highlight; they are unchanged.
- The modifier indicators of the EDIT screens (`Ctl Sft Alt Cmd`, green = on) are state, not a guide; unchanged.

## 3. Implementation

- One helper, `printGuide()`: a guide string marks the operation with braces, e.g. `"{Fn+S}: SETUP"`. The helper prints the pieces one after the other at the cursor (operation in `GUIDE_KEY_COLOR`, the rest in `GUIDE_TEXT_COLOR`, both on black); the braces are not drawn.
- With the fixed-width default font (6 × 8 px per character at size 1), this puts every character exactly where the single `print()` of 2a-4 put it: same text, size, position and spacing. No extra redraw: the screens are drawn exactly when they were before.

| Screen | Guide lines now colour-coded | Before |
| --- | --- | --- |
| Fn+1 YOUTUBE, Fn+3 EDITING (status screen) | — (these screens show no operation guide) | — |
| Fn+2 SPINNER | `Fn+. -   Fn+; +` | yellow |
| Fn+4 CUSTOM | `Fn+S: SETUP` (the last event and `NOT CONFIGURED` stay yellow) | yellow |
| Fn+5 C-SPINNER | `Fn+S: SETUP` after `DIR: …` (the direction stays white); `Fn+. -   Fn+; +` | white / yellow |
| CUSTOM SETUP | `1-3/turn: select  knob press: edit`; `ok (Enter): SAVE   esc (`): CANCEL` | white |
| CUSTOM EDIT (CW, CCW, BTN) | `Type a key, or turn knob for list`; `Fn+C Ctl  Fn+S Sft  Fn+A Alt  Fn+G Cmd`; `del: clear  ok/knob press: done`; `Fn+H: TAP/HOLD   esc (`): undo` / `esc (`): undo this action` | white |
| C-SPINNER SETUP (rows SENS / DIR / BTN / MODE; DIR and MODE toggle on this screen) | `1-4/turn: select  knob press: change`; `ok (Enter): SAVE   esc (`): CANCEL` | white |
| C-SPINNER EDIT SENS | `turn knob or Fn+; / Fn+. : change`; `ok/knob press: done  esc (`): undo` | white |
| C-SPINNER EDIT BTN | `Type a key, or turn knob for list`; the modifier line; `Fn+L mouse L  Fn+R mouse R  del: none`; `Fn+H TAP/HOLD  ok/press: done  esc: undo` | white |
| CONNECTION | `1 USB 2 BLE ok/press:apply esc(`):close`; `t:trace knob:select` (the handles after it stay dark grey; a GATT error stays red) | dark grey |
| BLE trace pages, U135 missing | — (no operation guide) | — |

## 4. Unchanged logic (checked)

- Function comparison 2a-4 → 2a-5: 162 of 167 functions byte-identical, none removed; 1 new (`printGuide`); 5 changed, all drawing functions: `drawCustomStatusBody`, `drawSetupScreen`, `drawCspinSetupScreen`, `drawConnScreen`, `drawSpinnerFull`.
- Outside functions only the build id / screen tag and the two colour constants differ.
- Byte-identical: all USB / BLE HID, U135 input, sensitivity, direction, profile switching, TAP / HOLD, assignment, NVS, switch and Launcher-related code. GATT identity check against BLE-B1: PASS.

## 5. Build

- arduino-cli, FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc`, core 3.3.9, `--warnings all`: **no errors; no warning from the sketch.** The warnings in the log come only from the board package's headers (`Arduino.h`, TinyUSB `cdc_host.h`, `USBHIDKeyboard.h`); a clean rebuild of 2a-4 shows the same kind (504 lines, none from the sketch).
- Flash 896,711 bytes (68 %), RAM 69,596 bytes (21 %).
- Checksums (`MediaJog_2a_5/SHA256SUMS.txt`):
  - source `20a3659b05c748025b6fd28e3a921d2cda50a71cde7977adbc4837196c3ea26d`
  - app-only binary `MediaJog_2a_5.ino.bin` `4093c0471900fd4a9154575803991071dd2fa23f8aa2136e4816d76a88ca7677`
- Note: builds are not byte-reproducible (a clean rebuild of the unchanged 2a-4 source gave a different binary hash). The recorded files are the reference.
- Install only the app-only `.ino.bin` through the Launcher.

## 6. Host tests (Mac, stubs; not hardware)

| Suite | Result |
| --- | --- |
| Stage 2a / 2a-2 suite (tag `2a5`) | 85 / 85 PASS |
| Stage 2a-2 BLE scenarios | 32 / 32 PASS |
| KEY STEP / ext suite | 34 PASS, 1 FAIL — the same FIFO-across-reversal check that fails on r2 and 2a-4 (replaced on purpose by r2's purge) |
| r2 reversal suite | 21 / 21 PASS |
| C-SPINNER suite (2a-4) | 80 / 80 PASS |
| **Display comparison 2a-4 ↔ 2a-5 (new)** | **270 / 270 PASS** |

Display comparison: the display stub records every character with its x / y position, text size and colours. 25 screen states (all profiles, CUSTOM SETUP / EDIT, C-SPINNER SETUP / SENS / BTN edit with picker, modifiers and mouse buttons, CONNECTION incl. a GATT error, trace page), each with USB and with BLE as the active transport, plus the U135-missing screen (51 screens in all), are drawn by 2a-4 and by 2a-5 and compared:
- every screen has the same characters at the same positions and sizes (only the build id `4` → `5` differs);
- every colour change is on one of the guide lines of §3, and every changed character is cyan or white on black;
- no other line (status, values, selections, warnings, diagnostics) changed colour.

## 7. Not verified (hardware)

- How cyan / white look on the device (contrast, readability), and that no character is cut or left over on the real display. The host test checks positions in a model of the fixed-width font, not the panel.
- Everything else of 2a-5 on the device. The logic is byte-identical to 2a-4, but 2a-5 itself has not been run on hardware.

## 8. Display check on the device (for the author)

1. Install `MediaJog_2a_5.ino.bin` through the Launcher; tag `USB 2a5` (or `BLE 2a5`).
2. Fn+2: bottom line `Fn+.` / `Fn+;` cyan, `-` / `+` white; sensitivity number as before.
3. Fn+4: `Fn+S` cyan, `: SETUP` white. `Fn+S`: CUSTOM SETUP guide lines; `1` → EDIT guide lines.
4. Fn+5: `DIR: …` white, `Fn+S` cyan; `Fn+S` → C-SPINNER SETUP, `1` (SENS), `3` (BTN) screens.
5. Fn+B: CONNECTION bottom two lines.
6. On each: nothing cut at the right edge, no left-over characters, text where it was in 2a-4.
7. A short function check (pointer, one click, one key) to confirm the install.
