# Stage 2a-4 — C-SPINNER (PROFILE 5)

Created: 2026-10-16 · Implementation: Claude Opus 5.5 · Request: the author (Stage 2A-4 instructions) · Design basis: `STAGE_2A_3_DESIGN.md` §4–§5, §12

**Status: built and host-tested. NOT tested on hardware.** Nothing in this document is a hardware result.

- Base: `MediaJog_2a_3_r2` (hardware-verified; **unchanged**, checksums verified OK on 2026-10-16).
- New folder `MediaJog_2a_4/`, build id `2a-4`, screen tag `USB 2a4` / `BLE 2a4`.

## 1. Purpose

A mouse profile whose settings belong to the user, next to the existing SPINNER (Fn+2), which stays unchanged. It is general (games, video editing, ordinary pointer use), not an Arkanoid feature (`STAGE_2A_3_DESIGN.md` §12.4).

## 2. Review before implementation (2026-10-16)

Checked: `STAGE_2A_3_DESIGN.md`, `DEVELOPMENT_PLAN.md`, `REPORT.md`, the r2 source, and the NVS records (`cfg`, `link`, `ext`).

| Point | Finding | What was done |
| --- | --- | --- |
| "14 steps" and the value 64 | Consistent. The sensitivity is one of the 14 values `SENS_STEPS` = 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128 HID counts per detent. **64 is step 12 of 14** (index 11). The screen shows the count (64) and the step (12/14) | Same table, same steps and same arithmetic for PROFILE 5; no change to the sensitivity specification |
| Initial sensitivity | "As the existing SPINNER's initial setting" = the firmware default, **8** (step 6/14), not the 64 the author uses on PROFILE 2. The `ext` record of 2a-3 already stores this default | Default 8. To get the Arkanoid baseline: Fn+5, then `Fn+;` six times (or SETUP → SENS) |
| Design §4: "`ok` or the knob press toggles the direction" | Conflicts with CUSTOM SETUP, where `ok` on the list means SAVE | `ok` = SAVE as in CUSTOM; DIR is toggled with `2` or the knob press. **To be confirmed by the author / Edgar** |
| SENS in SETUP | New in this request (the design changed SENS only with `Fn+;` / `Fn+.`) | Both: `Fn+;` / `Fn+.` on the C-SPINNER screen (stored 2 s after the last step), and a SENS row in SETUP (stored on Save) |
| TAP / HOLD | The design had `Fn+H` in the BTN edit | Both: a MODE row in SETUP, and `Fn+H` in the BTN edit |
| NVS format | The `ext` record (MJEX v1, 21 bytes) already defines every C-SPINNER field and validates it | No format change; `cfg` unchanged (profiles 1–4 only) |

No conflict that blocks the implementation was found. The items marked "to be confirmed" are UI choices.

## 3. Behaviour

| Item | Implemented |
| --- | --- |
| Selection | `Fn+5`. Display name `C-SPINNER` ("PROFILE 5:C-SPINNER" on the spinner screen) |
| Rotation | Relative mouse X through PROFILE 2's path (half-count carry, `MOUSE_PENDING_MAX`, at most 4 reports of ±127 per loop), with PROFILE 5's own sensitivity; reversed direction negates the encoder delta |
| Sensitivity | `Fn+;` / `Fn+.` in PROFILE 5 change only the C-SPINNER value; PROFILE 2's value (in `cfg`) is never touched, and the reverse |
| Button | key with modifiers (or modifiers only), mouse left, mouse right, or none; TAP or HOLD |
| Key TAP / HOLD | As CUSTOM's BTN: one tap per press / held while the knob is held (same queue and HOLD base state) |
| Mouse HOLD | Button held while the knob is held (as PROFILE 2) |
| Mouse TAP | One click: the button goes down, and up again once the host has confirmed the down report (USB: transfer completed; BLE: accepted by the stack). The knob release does nothing |
| Defaults | sensitivity 8 (6/14), direction normal, mouse left, HOLD: PROFILE 5 starts like PROFILE 2 at its default |
| Releases | Profile switch, SETUP / CONNECTION entry, U135 loss, link loss and transport switch release a held key or mouse button (existing release points) |
| Transports | USB and BLE through the existing report paths; GATT table unchanged (identical to B1) |

## 4. C-SPINNER SETUP (`Fn+S` in PROFILE 5)

Same key scheme as CUSTOM SETUP. Nothing is sent while it is open.

| Row | Change it with | Effect |
| --- | --- | --- |
| `1 SENS` | `1` or knob press → EDIT SENS: turn the knob or `Fn+;` / `Fn+.`; `ok` / knob press = done; `` ` `` = undo | 1–128 (14 steps); "PROFILE 2 SENS is not changed" is shown |
| `2 DIR` | `2` or knob press | NORMAL ⇄ REVERSE |
| `3 BTN` | `3` or knob press → EDIT BTN | type a key, or turn the knob for the CUSTOM key list; `Fn+C/S/A/G` Ctl/Sft/Alt/Cmd; `Fn+L` mouse left; `Fn+R` mouse right; `del` none; `Fn+H` TAP/HOLD; `ok` / knob press = done; `` ` `` = undo |
| `4 MODE` | `4` or knob press | TAP ⇄ HOLD |

- `ok` (Enter) on the rows = **SAVE**; `` ` `` (esc) = **CANCEL** (nothing changed, nothing stored).
- Turning the knob on the rows moves the selection.
- A modifier pressed while the button is a mouse button makes it a key action (modifiers only). A key action with neither key nor modifier is stored as "none".
- `del` keeps MODE (it is its own row); in CUSTOM, `del` also resets TAP.
- Uppercase key names as in CUSTOM (unchanged convention).

## 5. NVS

| Trigger | Written | When |
| --- | --- | --- |
| `Fn+;` / `Fn+.` in PROFILE 5 | `ext` sensitivity | `EXT_SAVE_DELAY_MS` (2 s) after the last step; only if different (status `NV WAIT` meanwhile) |
| Save in C-SPINNER SETUP | `ext` sensitivity, direction, button | at once (in `loop()` after the HID sends) |
| Cancel | nothing | — |
| `Fn+5` | `ext` active profile 5 + the current `cfg` profile | at once |
| Leaving PROFILE 5 (`Fn+1..4`) | `cfg` profile as before; `ext` active profile | at once |
| Switches among 1..4 only | `cfg` as before; `ext` **not** written (as r2) | — |

- `cfg` never holds 5. At boot PROFILE 5 is restored only if `ext` is valid, says 5, and its stored `cfg` profile equals the `cfg` profile now (older firmware, which ignores `ext`, may have changed it).
- Each write is checked (read back); failure shows `NV WFAIL` / `NV FULL` and, for Save, "SAVE NOT STORED". Not retried automatically (as `cfg`).
- A newer-version or unreadable `ext` makes C-SPINNER work in RAM only ("SAVED (RAM ONLY)"); the record is never overwritten.
- No erase of any kind; the space check (`NV_MIN_AVAILABLE_ENTRIES`) is unchanged. Launcher data and other namespaces are not touched.
- Rollback to r2 / 2a-3: they read `ext` v1 unchanged (KEY STEPs), ignore the C-SPINNER fields and start in the `cfg` profile.

## 6. Unchanged (checked in the source)

- Function comparison r2 → 2a-4: 132 of 148 functions byte-identical, none removed, 19 new. The 16 changed functions only add the PROFILE 5 branches: `profileName`, `actionFor`, `sendQueued` (HOLD key source), `nvStateText`, `loadExt`, `serviceExtSave`, `handleUiKey`, `drawSetupScreen`, `drawStatus`, `drawSpinnerSensitivity`, `drawSpinnerFull`, `setProfile`, `pollKeyboard`, `onRotation`, `onButton`, `loop`.
- Byte-identical, among others: `sendMouse`, `changeSensitivity`, `pushDetent`, `purgeRotation`, `newestQueuedRotation`, `pushEvent`, `tapKey`, `sendRelease`, all USB / BLE send functions, `notifyReport`, `bleBegin`, the switch logic, CUSTOM SETUP, `loadSettings` / `writeSettings` / `serviceSettings`, the `ext` encode / decode / validation.
- PROFILE 2 code path: same variables (`sensIndex`, `delta`) and arithmetic; its screen text is unchanged.
- GATT identity check against BLE-B1: PASS.

## 7. Build

- arduino-cli, FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc`, core 3.3.9, `--warnings all`: **no errors, no warnings**. Flash 896,375 bytes (68 %), RAM 69,596 bytes (21 %).
- Checksums (`MediaJog_2a_4/SHA256SUMS.txt`):
  - source `fc17bf73d413a813421a7ba16b4bbea0bc8024d035ffa7af6f552729f0757085`
  - app-only binary `MediaJog_2a_4.ino.bin` `4e1dc5edc8815a209f1b6e67e0f931f141a4ced50bb26d74ff53154455e4b431`
- Install only the app-only `.ino.bin` through the Launcher (not the merged image).

## 8. Host tests (Mac, stubs; not hardware)

The Arduino-preprocessed source runs `setup()` / `loop()` against stubs (USB HID, NimBLE, NVS in memory, U135, keyboard); each scenario runs in its own process.

| Suite | Result |
| --- | --- |
| Stage 2a / 2a-2 suite (tag strings updated to `2a4`) | 85 / 85 PASS (as r2) |
| Stage 2a-2 BLE scenarios | 32 / 32 PASS |
| KEY STEP / ext suite | 34 PASS, 1 FAIL — the same check fails on r2: it expects FIFO across a reversal, which r2's purge replaced on purpose |
| r2 reversal suite | 21 / 21 PASS |
| **New C-SPINNER suite (23 scenarios)** | **80 / 80 PASS** |

New scenarios cover: Fn+5 and defaults; default PROFILE 5 = default PROFILE 2 (same reports); independent sensitivities in RAM and NVS; **sensitivity 64 gives exactly the PROFILE 2 reports at 64** (1, 4 and 10 detents in one read, and a right-then-left turn); direction reverse; Cancel; SENS in SETUP stored on Save only; `Fn+;` debounce; mouse right TAP (one click, also on a quick press) and HOLD; key TAP; Shift HOLD with rotation; releases on profile switch and SETUP entry; BTN none; picker and modifier from a mouse action; PROFILE 5 restore at boot and its `cfg` check; leaving PROFILE 5; no `ext` write for switches among 1..4; write failure; newer `ext`; BLE movement and click; PROFILE 2 screen and `Fn+S` unchanged.

Mutation check: 10 deliberate defects (shared sensitivity, PROFILE 2 sensitivity used in 5, direction ignored, `ext` not written on leaving 5, `cfg` asked to hold 5, click never released, no `cfg` check at boot, HOLD using the CUSTOM key, SETUP SENS not stored, Cancel applying the draft): **all 10 detected** (one only after a check was tightened to the write-failure counters).

## 9. Not verified (hardware)

Everything in this stage. In particular:
- Fn+5, the C-SPINNER screen and SETUP screens on the device (layout, readability);
- rotation and sensitivity on the host, including **64 reproducing the Arkanoid baseline** (speed, responsiveness) — the host tests show identical reports, not the feel;
- direction reverse; mouse right (HOLD / TAP click) on macOS, Windows, ChromeOS; key TAP / HOLD and keyboard + mouse together, over USB and BLE;
- NVS: restore of PROFILE 5 and the settings after a power cycle; coexistence with the Launcher settings; rollback to r2;
- that PROFILE 1–4 behave as on r2 on hardware (source unchanged in those paths; not re-tested);
- the "`BLE READY` without input" issue (open since r2) is not addressed by this stage.

The stepped movement seen in Arkanoid (`STAGE_2A_3_DESIGN.md` §12.3) is out of scope; nothing was changed for it.

## 10. Suggested hardware checks (for the author; none done)

1. Install `MediaJog_2a_4.ino.bin` through the Launcher; check the tag `USB 2a4` and `NV OK`.
2. Fn+2: sensitivity still as stored (64 if it was 64); pointer and drag as before.
3. Fn+5: screen "PROFILE 5:C-SPINNER", 8 (6/14), BTN HOLD Mouse L, DIR NORMAL; pointer and drag like Fn+2 at 8.
4. Fn+5, `Fn+;` ×6 → 64 (12/14); Fn+2 still 64 and unchanged; web MAME Arkanoid with Fn+5 at 64 (with `-mouse`, Esc afterwards).
5. SETUP: DIR REVERSE → Save → direction reversed; BTN Mouse R TAP → one right click per press; BTN a key TAP / HOLD (e.g. Shift HOLD + turn).
6. Cancel leaves everything as it was.
7. Power off / on in PROFILE 5: PROFILE 5 and its settings come back; Fn+1..4 unchanged.
8. Repeat 3–5 over BLE.

## 11. Hardware test results (2026-10-16, author: Cardputer ADV + Mac)

Real-device results reported by the author for `MediaJog_2a_4` (`4e1dc5ed…`). They apply to 2a-4 only.

### 11.1 BLE

| Test | Result |
| --- | --- |
| Fn+2 SPINNER mouse movement | PASS |
| Fn+5 C-SPINNER mouse movement | PASS |
| Fn+5 at sensitivity 8 | PASS |
| Fn+5 at sensitivity 64 | PASS |
| Fn+2 and Fn+5 sensitivities independent | PASS: changing Fn+5 from 8 to 64 left Fn+2 at 64. At 64 both profiles moved the pointer by about the same amount (by feel) |

### 11.2 Direction

| Test | Result |
| --- | --- |
| NORMAL → REVERSE | PASS |
| Saved | PASS |
| Pointer direction actually reversed | PASS |
| Kept after a profile switch | PASS |
| Kept after power off / on | PASS |

### 11.3 Mouse button

| Test | Result |
| --- | --- |
| Mouse L → Mouse R | PASS |
| Mouse R gives a right click | PASS |
| TAP / HOLD switch | PASS |
| TAP Mouse R: three right clicks in a row | PASS |
| TAP Mouse L: no drag | PASS |
| HOLD Mouse L: drag works | PASS |

### 11.4 Keyboard assignment

| Test | Result |
| --- | --- |
| Key A assigned to the knob | PASS |
| A typed | PASS |
| HOLD A, knob held | PASS: macOS (English input) showed its accent menu; Japanese input repeated the character ("あああああ"). Both are the computer's normal reaction to a held key |
| TAP A, knob held | PASS: one character only |
| Shift + A | PASS: capital A |
| NONE | PASS: knob press types nothing |

### 11.5 NVS (after power off / on)

- C-SPINNER direction REVERSE kept: PASS.
- C-SPINNER sensitivity 64 kept: PASS.

### 11.6 USB

- USB selected, Fn+5 C-SPINNER: the Mac pointer moves when the knob is turned: PASS.

### 11.7 Scope

- No defect was found within these tests.
- The UI choices of §2 (DIR toggled with `2` / knob press, `ok` = SAVE; SENS and MODE rows) were used as implemented; no change was requested.
- **Not covered by these tests:** web MAME Arkanoid with Fn+5 (Arkanoid was played with Fn+2, `STAGE_2A_3_DESIGN.md` §12); Windows / ChromeOS; USB button and keyboard tests on Fn+5 (only pointer movement over USB); key HOLD + rotation together; long-term use; the "`BLE READY` without input" issue (still open).
