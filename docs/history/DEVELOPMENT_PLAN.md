# Cardputer Media Jog — Development Plan

**Updated:** 2026-10-13  
**Status:** Stage 1d hardware-tested in part; USB reliability investigation and cross-platform testing pending. **Stage 1e (§6): 1e-1 (four profiles) is hardware-tested in part (2026-10-13); 1e-2 (Custom setup UI): test 1 failed, while revision r2 passed its core hardware checks (setup, Save, Cancel, Custom output, Command), with several checklist items untested. r3 (larger key names on the overview screen): the author confirmed the label sizing; the rest of its test is not recorded. 1e-3 (Custom HOLD): Edgar's prioritized hardware tests passed (HOLD Shift + Right selection, release on knob release and on profile switch); the rest of its checklist is not tested. 1e-4 (NVS persistence): hardware results reported by the author (NVS restore, Launcher Wi-Fi kept, SOUND TERRARIUM OK); **fixed as the USB release candidate**. 1e-5 (final regression) is planned only. **BLE-1** (separate BLE-only prototype, `docs/BLE_1.md`) is built, not hardware-tested.** Flat and 15° bases are both printed and assembled (§4).  
**Project name:** Cardputer Media Jog (working name)

> This document supersedes the working Japanese development-plan narrative for planning purposes. The original `REPORT.md` and stage reports remain historical evidence; planned actions must not be confused with completed tests. No firmware was modified as part of this documentation update.
>
> **2026-10-13 correction:** No in-repository backup of the previous Japanese plan was found. The technical decisions it carried are restored below in **§2.1 (design invariants)** and **§2.2 (historical decision index)**, with pointers to the original evidence in `REPORT.md` and `STAGE_1A.md`–`STAGE_1D.md`. If the author holds a copy of the Japanese plan, add it as `docs/archive/DEVELOPMENT_PLAN_ja_2026-10-12.md`.
>
> **2026-10-13 search result:** The project directory was searched by file name and by the Japanese plan's title and section headings. **No original copy was found.** `docs/archive/` is therefore intentionally absent; the historical plan has not been reconstructed.

## 1. Purpose and product direction

Create a compact, modular rotary HID controller using the **M5Stack Cardputer ADV** and **M5Stack Unit Encoder U135**. The Cardputer's keyboard and screen form a local control interface; the encoder supplies rotation and push input. The initial host connection is **USB HID keyboard + relative mouse**, with no mandatory host-side Python service, resident app, terminal process, or Chrome extension. Initial use cases: paused-video frame jog, browser games using mouse input, and eventually editing/shortcut control.

**Separate two concepts:** a **profile** describes the function of dial movement and push; a **host slot** identifies the destination machine and connection transport. Switching a USB cable between computers cannot be done in software; BLE host selection is a later project.

Do not claim a new input technology, unique market position, universal emulator compatibility, or price advantage without evidence.

## 2. Hardware and interaction architecture

- **Controller:** Cardputer ADV (ESP32-S3), screen and physical keyboard used for local settings.
- **Input:** U135 via external Grove I²C (`SDA GPIO2`, `SCL GPIO1`, address `0x40`). The Cardputer keyboard uses a separate internal I²C bus.
- **Encoder observations:** one physical detent produced `+2` / `-2` raw counter changes in the probe test. Push-button polarity was established by real-device testing; do not infer it solely from contradictory early manufacturer documentation.
- **Output:** composite USB HID keyboard and relative mouse in current firmware. No gamepad analog-axis output.
- **Mode 1 (YouTube):** clockwise `.`; counterclockwise `,`; push `k`. Frame stepping is for paused video, with host focus required.
- **Mode 2 (SPINNER):** clockwise relative mouse +X; counterclockwise −X; U135 push holds mouse left button; release releases it.
- **Local keys:** `Fn+1` selects YouTube; `Fn+2` selects Spinner. In Spinner, `Fn+;` increases sensitivity and `Fn+.` decreases it. These Cardputer keys are not passed through as ordinary host typing.

### Stage 1d sensitivity behavior

14 sensitivity steps, in **mouse counts per detent**: **1 / 2 / 3 / 4 / 6 / 8 / 12 / 16 / 24 / 32 / 48 / 64 / 96 / 128**.

- Default **8**, the same as Stage 1c. The value is volatile: it is not stored in NVS and resets on every power-on. A single step index (`sensIndex`) is the only value a future NVS setting would need to store.
- **Arithmetic:**
  - One detent is 2 raw encoder counts, so each raw count is worth sensitivity ÷ 2 mouse counts.
  - The raw counter delta is used in full.
  - The firmware works in half mouse counts and carries the **leftover half count produced by odd sensitivities** (−1, 0 or +1) to the next delta.
  - All arithmetic is `int32_t`.
- **Output:**
  - Split into reports of at most ±127 (`int8_t` X in TinyUSB's standard mouse descriptor).
  - At most 4 reports per loop (±508).
  - Pending motion is capped at ±1016 counts; any excess is dropped and counted as `clip`.
- **Sensitivity change:** pending movement and the half-count carry are discarded.
- **Direction reversal:** no special handling. The opposite delta is added with its sign, as in Stage 1c. A reversal-discard rule was considered and deliberately not adopted.
- **Keys:** `Fn+;` (up-arrow legend) and `Fn+.` (down-arrow legend), judged from the library's `fn` flag plus the base character. They act once per press, in Profile 2 only.
- **Display:** the Profile 2 screen is redrawn in full only on layout or USB-state changes. The value is updated in place, and the diagnostics rows at most 4 times per second. Rotation alone never triggers a full-screen redraw.

This design is not evidence of lossless high-speed rotation; verify input tracking separately.

### 2.1 Design invariants (do not change without review)

These decisions were made after reading the library sources and were then confirmed on hardware where noted. They look like candidates for "simplification" but each one prevents a specific failure. Source line references are for `m5stack:esp32` 3.3.9, M5Unified 0.2.25, M5GFX 0.2.32 and M5Cardputer 1.1.1 as installed on the build machine.

1. **Build settings: USB Mode = `USB-OTG (TinyUSB)`, USB CDC On Boot = `Enabled`.** Every MediaJog sketch refuses to compile otherwise (`#error`).
   - In `Hardware CDC and JTAG` mode, the TinyUSB HID classes compile but do nothing: the core's own example turns into empty `setup()`/`loop()`.
   - With CDC On Boot disabled, `Serial` becomes UART0 with TX on GPIO43, which the Cardputer ADV uses for audio I²S.
   - The JTAG debugger is not available in USB-OTG mode.
   - Evidence: `STAGE_1A.md` §3, §5; REPORT §11.
2. **Initialization order:**
   - First `M5Cardputer.Display.begin()` (the display-only start that U135_Probe v2 proved on this unit).
   - Then `M5Cardputer.begin(cfg, true)`, with IMU, RTC, speaker, microphone, external displays, `clear_display` and `serial_baudrate` disabled.
   - Then `Wire.begin` for the U135 on GPIO2/GPIO1.
   - Why:
     - `M5.begin()` is needed only because the keyboard driver selects the ADV's TCA8418 reader from `M5.getBoard()`.
     - With the display already detected, `M5GFX::init_impl()` returns immediately, so the screen is not initialized twice.
     - `M5.begin()` also skips driving GPIO46 high. On the ADV, GPIO46 is the microphone data line and is not a power-hold pin.
   - **Do not replace this with the official examples' single `M5Cardputer.begin()` call** without re-testing the display.
   - Evidence: `STAGE_1B.md` §5-1; confirmed on hardware in Stage 1b.
3. **I²C bus separation:**
   - U135: I2C_NUM_0, Arduino `Wire`, GPIO2 (SDA) / GPIO1 (SCL), 200 kHz.
   - Keyboard (TCA8418 at 0x34): I2C_NUM_1, owned by M5Unified through `Wire1`, GPIO8/GPIO9, interrupt on GPIO11, 400 kHz.
   - The sketches never use `Wire1`.
   - Do not use the board variant's `IN_SDA`/`IN_SCL` macros: they are swapped relative to the official pin table.
   - Evidence: `STAGE_1B.md` §5-4, REPORT §1-3.
4. **U135 reads are checked:**
   - Every register read checks `endTransmission()`, `requestFrom()` and `read()`, and rejects button values other than 0/1.
   - A failed read is discarded and never becomes an input.
   - The device's presence is re-probed every 500 ms.
   - Button polarity is taken from hardware testing: raw `0` = pressed.
   - Evidence: REPORT §9.
5. **USB HID layout:**
   - One HID interface using the report protocol only: no boot interface, subclass 0, protocol 0. Report ID 1 = keyboard (8 bytes), Report ID 2 = relative mouse (5 bytes).
   - The sending `USBHID` object is declared **first** with `HID_ITF_PROTOCOL_NONE`. This matters because the first-constructed `USBHID` fixes the interface protocol, so the result does not depend on the order in which the mouse and keyboard objects are declared.
   - Consequence: the device does not act as a boot-protocol keyboard (BIOS etc.).
   - All reports are sent with `USBHID::SendReport()` so that the result can be checked.
   - Evidence: `STAGE_1C.md` §5-1; confirmed on hardware in Stage 1c-1.
6. **Keys are sent as HID Usage IDs plus modifier bits, never as ASCII:**
   - `.` = 0x37, `,` = 0x36, `k` = 0x0E, with no modifier.
   - ASCII-based APIs add Shift for upper-case letters (for example, `K` would be sent as Shift+k), and they depend on the host keyboard layout.
   - Evidence: REPORT §11; hardware: Stage 1a.
7. **Send-result handling:**
   - `SendReport()` = true means TinyUSB reported the HID IN transfer as **complete**. `USBHID::SendReport()` waits for a semaphore given in `tud_hid_report_complete_cb()` (`libraries/USB/src/USBHID.cpp:325-369`). TinyUSB documents that callback as "Invoked when sent REPORT successfully to host" (`class/hid/hid_device.h:150`).
     - It shows that the report left the device in a completed USB transaction, i.e. the host controller polled the endpoint and received it.
     - It does **not** show that the host operating system, the HID driver or the focused application processed the report or acted on it.
     - The library notes that a stale or extra completion can leave the semaphore given, and clears it before each send (`USBHID.cpp:351-355`). With a single sending task, as in these sketches, the remaining ambiguity is small but not formally zero.
     - *(Wording corrected 2026-10-13: the earlier text "the host read the report" was stronger than the implementation supports. Firmware behavior is unchanged.)*
   - If `ready()` is false, nothing was sent; the input is kept and retried.
   - If the outcome is unknown (timeout), the key tap or movement is **not** resent, to avoid duplicates.
   - A failed all-keys-up report is retried every loop until it succeeds. Every USB-ready period starts with an all-keys-up report.
   - Evidence: `STAGE_1A.md` §5.
8. **Mouse button safety:**
   - The button state is resent until a report is confirmed. This is safe because a button report carries a state, not an event.
   - The button is released on a profile switch, on loss of the U135, and whenever USB becomes not-ready.
   - After a reconnect, the first mouse report is a release, and the button goes down only after a **new** press of the knob.
   - Evidence: `STAGE_1C.md` §5-2. Hardware: drag release on `Fn+1` and on cable removal (Stage 1c-1). The release-after-reconnect path has **not** been exercised, because automatic reconnect does not work yet (P1).
9. **Cardputer keys are local only:** no code path forwards Cardputer keystrokes to the host. Chords are recognized from `keysState().fn` plus exactly one recognized base character, and act once per press. Evidence: `STAGE_1B.md` §5-3; hardware: Stage 1b.
10. **Deployment:** the app-only `.ino.bin` is installed through the Launcher's Wi-Fi upload. An Arduino IDE upload, or a `merged.bin`, writes the bootloader (0x0), partition table (0x8000), boot_app0 (0xe000) and app (0x10000), and so removes the Launcher (`platform.txt` upload pattern). Evidence: `STAGE_1A.md` §5⑤.
11. **Known harmless warning:** `KEY_TAB` is defined by both M5Cardputer's `Keyboard_def.h` and the core's `USBHIDKeyboard.h`. The sketches do not use it. Evidence: `STAGE_1C.md` §4.

### 2.2 Historical decision index

| Decision | Status | Where recorded |
| --- | --- | --- |
| Profile 1: one detent = one key tap; the ±1 raw-count remainder is kept, not discarded | Implemented since 1a | `STAGE_1A.md` §2 |
| Keyboard event queue of 32 and up to 4 taps per loop | **Provisional** values; capacity estimated, not measured | `STAGE_1A.md` §5④ |
| YouTube Music profile (`Shift+L`/`Shift+H`/`Space`) | Shortcuts confirmed on a physical Mac keyboard; **not implemented** on the device; deprioritized (1-second steps) | REPORT §13 |
| Do not add confusing duplicate profiles for the same application | Policy | Previous plan, REPORT §14 |
| Wi-Fi is not a native HID transport and needs host-side software; an ESP-NOW receiver dongle that outputs USB HID is a possible alternative needing extra hardware | Unverified idea | REPORT §11 |
| Settings persistence: NVS (Preferences), versioned format, delayed write on change; microSD only for export | Planned, not implemented | REPORT §11 |
| U135 resolution: officially 30 pulses per revolution; comparisons with other spinners' step counts need care with units | Context for P3 | REPORT §11, §15, §19 |
| RetroArch candidates: FBNeo "Mouse (full)" (mouse buttons used) vs "Mouse (ball only)" (buttons stay on RetroPad); MAME 2003-Plus "Mouse Device" | Documentation only; not installed or tested | REPORT §19, §22 |
| Grove hot-unplug test | Deliberately not performed (the project connects units with power off) | `STAGE_1C.md` §7 |
| USB initial recognition intermittent; USB auto-reconnect fails | Open; root cause unknown | REPORT §18, §21 |

## 3. Completed milestones and evidence status

| Milestone | Actual outcome | Still unverified / caveats |
| --- | --- | --- |
| Probe / Stage 0 | U135 detection, ±2 raw count per detent, button transitions, LCD, I²C observed working | Longer-term electrical/mechanical endurance |
| Stage 1a | Launcher Wi-Fi application-only upload; macOS Chrome YouTube jog and push-to-play/pause worked | Exact frames per detent; rapid rotations; YouTube Music on device |
| Stage 1b | `Fn+1` / `Fn+2` profile selection; no ordinary host typing from Cardputer keyboard | Extended stress tests |
| Stage 1c-1 | Composite keyboard/mouse; pointer X motion, Finder drag, drag release on profile switch, Mac drag release on USB removal | Single-click, Grove hot-unplug, automatic USB recovery |
| Stage 1c-2 browser test | Browser Breakout paddle moved at sensitivity 8, but movement was too small for practical play | Root cause not isolated among scaling, OS pointer behavior, game input |
| Stage 1d | Built successfully. The author then used it in browser Breakout: **48–64 counts/detent** felt comfortable, and rapid left–right movement felt smooth, without noticeable catching or lag (subjective; `STAGE_1D.md` §9, REPORT §25) | Checklist not executed item by item (per-item status in `STAGE_1D.md` §9-3); SHA-256 of the uploaded binary not recorded; no quantified count-loss/latency study |
| Flat 3D-print mount | Large green knob and removable flat green base printed; Cardputer and U135 installed and photographed | No measured drop/torque endurance |
| Centered-knob revision | Encoder moved approximately **10 mm inward** while preserving base length; revised print completed and photographed; knob fits within base footprint in shown view | Fit/ergonomics are observed visually, not dimensional tolerance certification |
| 15° wedge base | Designed in Fusion 360. **Printed and assembled** with the Cardputer ADV and U135 (author report, 2026-10-13; earlier the same day reported as "being printed"). The author operated the knob on both bases and reported **no substantial difference in operating feel** compared with the flat base | Subjective comparison only. Stability, slipping, push force, long-term fatigue and ergonomics **not systematically evaluated**; no ergonomic advantage is claimed. The 18.936 mm CAD distance should not be assumed to be a verified tabletop height |

**Specific real-world game observation:** The author found 48–64 much easier than the earlier fixed 8 for rapid side-to-side paddle movement in a browser Breakout game on macOS/Chrome, and reported smooth movement without noticeable catching or lag. This is subjective, app-specific evidence; do not extrapolate to all platforms, emulators, or high-speed accuracy. It shows that more gain made this game playable. It does not separate the possible influence of macOS pointer acceleration or of how the game reads mouse input.

## 4. Mechanical design: modular enclosure

### 4.1 Current printed flat design

- Bright-green 3D-printed base with a large matching rotary knob and white Cardputer ADV/U135.
- Locating posts engage LEGO-compatible mounting holes in the two devices, allowing removable installation without glue.
- Revised encoder position is ~10 mm closer to the base center, without shortening the base.
- Maintain access to the Cardputer screen, keyboard, USB-C connector, U135 push action, and Grove cable.
- The knob is oversized relative to the encoder body: check side loads on the encoder shaft and clearance when pressing.

### 4.2 15° angled base (printed and assembled; an alternative, not a replacement)

Status (updated 2026-10-13; earlier the same day this section read "designed; print in progress"):
- **Flat base (centered dial):** printed, assembled and photographed (§4.1).
- **15° angled base:** printed and assembled with the Cardputer ADV and U135.
- **Author's comparison:** after operating the knob on both, the author reported **no substantial difference in operating feel**. This is a subjective impression.
- **Not systematically evaluated:** stability, slipping, push force, long-term fatigue and ergonomics. The 15° base is **not** claimed to improve ergonomics.
- **Relationship:** the two bases are **alternatives** for users to choose from. Neither is the "original" or the "improved" version. Both are intended for eventual publication.

Design notes kept from the CAD stage:
- The 15° slope was chosen to tilt the Cardputer screen toward the user while keeping a low profile. This was a design intent, and its effect has not been evaluated.
- The encoder axis and push direction tilt with the base. Remaining checks, if wanted: tabletop stability, push force, slipping, ease of turning.
- For a published model, document the actual ground-contact plane, front/rear heights, build orientation and supports. Do not equate a CAD measurement between selected surfaces with a measured assembled height.

### 4.3 Optional mounting and buttons

- Consider removable **rubber feet**, **magnet inserts**, or **suction-cup adapters** as separate mounting options. Magnets require a ferromagnetic surface; suction cups require a suitable smooth, nonporous surface. Avoid assuming either is universally effective.
- Two large **Sanwa arcade buttons** mapped to Space/Enter were discussed but **not implemented**; depth, switch terminals, and wiring conflict with the compact form factor. Keep low priority. Thin tactile switches with printed caps are an alternative concept only.
- Publish hardware files only after checking fit, print settings, and licensing. Proposed directory: `hardware/` for models; `images/` for photographs. Existing `paddle.3mf` may be moved to `hardware/` after verifying the file and updating links. Publish **both** bases (flat and 15°) as alternatives, with print notes for each.

## 5. Highest-priority engineering issues

### P0 — Preserve working versions and Launcher

- Never overwrite milestone sources/binaries:
  - `U135_Probe/` (v2)
  - `MediaJog_1a`, `MediaJog_1b`, `MediaJog_1c` (hardware-verified milestones)
  - `MediaJog_1d` as built on 2026-10-12. It has been used on hardware (subjective result); the uploaded binary's hash was not recorded.
- Preserve recorded SHA-256 manifests (`MediaJog_1b/`, `MediaJog_1c/` and `MediaJog_1d/` have `SHA256SUMS.txt`; 1a and the probe hashes are recorded in the stage docs). Next time a binary is uploaded, record its SHA-256 at upload time. Stage 1d sketch SHA-256: `3b8c5667a551663912639d2cfd7c7dc5e7e187f45cf4313806aae1dd75ac30c6`; app-only binary SHA-256: `2692467b9716ba6981947353256da79acf9b1e59a42f038e4af04640377b3d6f`.
- Use **Launcher Wi-Fi upload of the app-only `.ino.bin`** for tested deployment. Do not substitute merged images or direct IDE flashing without a separate recovery plan. SOUND TERRARIUM and Launcher must remain intact.

### P1 — USB reconnect and enumeration

- **Observed:** Unplug/replug USB while powered on can leave `USB SUSPENDED` displayed; power OFF→ON restored HID readiness. Initial recognition has also been intermittent.
- **Unknown:** root cause, relation between initial enumeration and reconnect, device visibility in macOS System Information after failure, CDC behavior, and event timing.
- **Next test:** log host USB device listing, serial-port presence, cable/port, unplug duration, power state, and device events. Reproduce on an isolated development copy, not a preserved milestone.
- **Acceptance:** reliable enumeration and reconnect without unsafe stuck mouse-button behavior, or a clearly documented limitation and workaround.

### P2 — OS compatibility

- Test **Windows**, then **Linux** using unchanged Stage 1d application firmware where possible. Verify USB enumeration as keyboard+mouse, focused YouTube controls, pointer/drag, sensitivity adjustment, reconnect behavior, and one mouse-controlled game.
- Do not claim tested compatibility before real tests. Generic USB HID support suggests feasibility but does not guarantee every shortcut or game mapping.

### P3 — Sensitivity and rotation performance

- Measure slow detents, rapid turns, reversals, prolonged use, pending-motion behavior, button holding, and UI responsiveness.
- Keep **8** as documented firmware default until intentionally changed. **48–64** is the user's practical Breakout range, not a universal recommended default.
- Consider NVS persistence with versioned settings and delayed writes only after the UX is settled; profile-specific sensitivity may be useful.

### P4 — Documentation and reproducible release

- Add checked hero/top/side photographs to `images/`, tested print files to `hardware/`, and assembly instructions.
- Document firmware build environment, checksums, app-only Launcher installation, safe recovery, and exact supported platforms.
- Translate technical docs into English carefully, preserving chronological distinctions. Do not rewrite original experimental observations as if they were newer tests.
- Check redistribution rights before publishing manufacturer schematics or third-party evidence files.

## 6. Stage 1e plan — "Set it once. Use it your way." (PLANNED, NOT IMPLEMENTED)

> **Status:** planning only (agreed direction, 2026-10-13). No Stage 1e code exists. Stage 1d remains the latest implemented firmware, and its behavior must be preserved until Stage 1e is implemented **and** hardware-tested. Nothing in this section is a test result.
>
> **Update 2026-10-13:** sub-stage **1e-1** (four profiles; Profile 4 a placeholder with no output) has been implemented in `MediaJog_1e_1/` and **built**. It has **not** been installed on hardware and is awaiting GPT review (`docs/STAGE_1E_1.md`). Stage 1d remains the reference firmware until 1e-1 passes hardware testing. Everything else in this section is still planned only.
>
> **Update 2026-10-13 (later):**
> - **1e-1 hardware test:** 10 checks passed and one was inconclusive (fast rotation then switching). Some checks were not recorded (`STAGE_1E_1.md` §10).
> - **1e-2** (on-device Custom setup UI, TAP only, RAM only) is implemented in `MediaJog_1e_2/` and **built**, not hardware-tested (`docs/STAGE_1E_2.md`).
> - HOLD (1e-3) and persistence (1e-4) are still planned only.
>
> **Update 2026-10-13 (1e-3):** HOLD for the Custom knob press is implemented in `MediaJog_1e_3/` and **built**, not hardware-tested (`docs/STAGE_1E_3.md`). Persistence (1e-4) is still planned only.
>
> **Update 2026-10-13 (1e-3 hardware, 1e-4):** Edgar's prioritized 1e-3 tests passed on hardware (`STAGE_1E_3.md` §10). **1e-4** (NVS persistence) is implemented in `MediaJog_1e_4/` and **built**, not hardware-tested (`docs/STAGE_1E_4.md`).

### 6.1 Product concept

The controller should behave like a **dedicated device** for the user's preferred application, while remaining configurable for other uses. Example: a user who always edits video selects Video Editing once; after every power-on the device comes back as an editing controller. The same applies to YouTube, Spinner and Custom users.

All configuration happens **locally** on the Cardputer ADV keyboard and display. No PC-side configuration application is required, and no host-side software is introduced (consistent with §1).

### 6.2 Functional requirements

| ID | Requirement | Notes / status |
| --- | --- | --- |
| FR-1 | Four profiles: 1 YouTube, 2 Spinner, 3 Video Editing, 4 Custom | 1 and 2 exist (Stage 1d); 3 and 4 are new |
| FR-2 | Profile selection: `Fn+1`…`Fn+4` | Proposed. See §6.4 for availability on the ADV |
| FR-3 | Video Editing defaults: clockwise = **Right Arrow** (HID usage 0x4F), counterclockwise = **Left Arrow** (0x50), press = **Space** (0x2C), one tap per detent / per press | Proposed for Adobe Premiere Pro and Apple Final Cut Pro, based on commonly documented default shortcuts (arrow = one frame, Space = play/stop). **Not tested in either application; no compatibility is claimed.** Behavior depends on which panel has focus |
| FR-4 | Custom profile: clockwise, counterclockwise and button each configurable as one HID keyboard key, optionally with modifiers (Ctrl, Shift, Alt/Option, GUI/Command) | Only keys representable in the existing keyboard report (Report ID 1). Consumer/media keys would need a new HID report and are **out of scope** unless separately approved |
| FR-5 | Custom button behavior: **TAP** (press+release when the knob is pressed) or **HOLD** (key down while the knob is held, up on release) | Rotation actions are TAP only (one tap per detent, as in Profile 1) |
| FR-6 | A configuration UI separate from the normal operating UI: distinct screen, explicit enter / save / cancel | Proposed entry key in §6.4 |
| FR-7 | **Configuration keystrokes never reach the USB host.** No HID report is generated while the configuration UI is open. All keys and buttons are released before it opens | Extends invariant §2.1-9 |
| FR-8 | Persistent settings: last selected profile, Custom mappings, Custom button behavior, Spinner sensitivity | Mechanism subject to review (§6.5). **Write timing is fixed by the agreed policy in §6.9.** Sensitivity stays a single global value unless a per-profile value is approved later |
| FR-9 | On boot: restore the last profile and its settings; validate stored data; fall back to safe defaults on any error | Defaults: Profile 1, sensitivity 8, Custom = unassigned (no key) until configured |
| FR-10 | **Never restore an active HID key-down or mouse-button-down state.** Every boot and every USB-ready period starts with all keys and buttons released | Extends invariants §2.1-7 and §2.1-8 |
| FR-11 | Avoid unnecessary flash writes | §6.5, §6.9 |
| FR-12 | Preserve Stage 1d behavior of Profiles 1 and 2, all §2.1 invariants, and the Launcher-compatible app-only workflow | Regression tests in §6.8 |

### 6.3 Design proposals (to be reviewed before implementation)

1. **Action model.**
   - Represent each encoder action as `{type, modifiers, usage}`, where type is one of KEY_TAP, KEY_HOLD, MOUSE_X or MOUSE_BUTTON.
   - Profiles 1, 3 and 4 use the keyboard path; Profile 2 keeps the Stage 1d mouse path unchanged.
   - Keys are always stored and sent as HID usage IDs plus modifier bits (invariant §2.1-6), never as characters.
2. **Keyboard state model (required for HOLD).**
   - Today every tap sends a report with a single key and then an all-keys-up report (`sendKeyReport()` in Stage 1a–1d). If a HOLD key were down, a rotation tap built that way would **silently release it**.
   - Stage 1e needs a small key-state model: the set of held keys and modifiers, plus the tap key. Every keyboard report must be built from that state.
   - This is a change to a hardware-verified code path and must be regression-tested on Profile 1.
3. **HOLD safety.** Apply the mouse-button rules (invariant §2.1-8) to held keys:
   - Release on profile switch, on opening the configuration UI, on loss of the U135, and whenever USB becomes not-ready.
   - Resend the all-up state until it is confirmed.
   - Require a **new** knob press after any of those events.
   - Note: the host's own key auto-repeat will act on a held key. That is expected host behavior, not a firmware repeat.
4. **Configuration UI.**
   - Separate screen; the encoder can be used for local navigation.
   - Keys can be captured from the Cardputer keyboard. The library already maps printable keys to HID usages, so the captured key is converted, not sent.
   - A list picker covers keys the Cardputer cannot type directly: arrows, F1–F12, Esc, Enter, Tab, Delete, Home/End, Page Up/Down.
   - Modifier toggles, including **GUI/Command**, which has no physical key on the Cardputer.
   - The screen shows the HID key name, not a host character, because the host keyboard layout (e.g. US vs JIS) decides the character for letters and symbols.
5. **Persistence.**
   - Arduino `Preferences` (NVS) with a project-unique namespace (e.g. `mediajog`, ≤15 characters).
   - One versioned settings record: magic, version, length, CRC, then the data.
   - Range checks on load: profile 1–4; sensitivity index 0–13; usage IDs in the keyboard page; behavior TAP or HOLD.
   - Any failure → defaults, plus an on-screen "settings reset" note.
   - Never call `nvs_flash_erase()`.

### 6.4 Key availability on the Cardputer ADV (source-checked, partly unverified)

| Combination | Library result (M5Cardputer 1.1.1) | Hardware status |
| --- | --- | --- |
| `Fn+1`, `Fn+2` | `fn=true` + `'1'`/`'2'` | **Verified** (Stage 1b) |
| `Fn+3`, `Fn+4` | `fn=true` + `'3'`/`'4'`; same number row as `1`/`2` (`Keyboard.h:21-22`). The detection method is the same as for `Fn+1`/`Fn+2` | **Verified on hardware** (Stage 1e-1 test, 2026-10-13, `STAGE_1E_1.md` §10) |
| `Fn+;`, `Fn+.` | Sensitivity up/down (Stage 1d) | Up: inferred from use; down: not separately recorded (`STAGE_1D.md` §9-3) |
| Configuration entry | `Fn+S` (`'s'` at `Keyboard.h:49`), Profile 4 only | **Implemented in 1e-2** (`STAGE_1E_2.md` §2); **verified on hardware with r2** (2026-10-13, `STAGE_1E_2.md` §11) |
| Configuration Save / Cancel | **ok** (Enter, via the library's `enter` flag) = Save, **esc** (`` ` ``, `Keyboard.h:18`) = Cancel, per the keycap legends in the author's image | **Implemented in 1e-2**; not yet tested on hardware |
| Modifier capture | `KEY_OPT` is defined as `0x00` in the library (`Keyboard_def.h:18`), the same value as "no key". In addition, `KEY_TAB`/`KEY_BACKSPACE` are defined differently by M5Cardputer and the core's `USBHIDKeyboard.h` | **Avoided by design in 1e-2:** modifiers are toggled with `Fn+C/S/A/G` instead of captured from the Cardputer's modifier keys, and special keys are read from the library's flags, never compared with these macros |

### 6.5 Persistence: technical risks and unverified assumptions

| ID | Risk / assumption | Evidence | Mitigation / check |
| --- | --- | --- | --- |
| R-1 | **(Reviewed again for 1e-4, 2026-10-13: still present in core 3.3.9 at `esp32-hal-misc.c:331-344`; cannot be disabled from the sketch; 1e-4 adds no erase call and a free-space guard. `STAGE_1E_4.md` §6.3)** **The Arduino core erases the whole NVS partition at boot** if `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` (`cores/esp32/esp32-hal-misc.c:331-337`). Under a Launcher, that partition is presumably shared with the Launcher and other apps | Source-checked. Present in **every** Arduino build, including Stage 1a–1d already | Keep MediaJog's NVS usage small and rare. Test that the Launcher's own settings survive (§6.8 AT-14). Document the risk to users |
| R-2 | The Launcher's partition table contains a usable `nvs` partition, and its size is adequate | **Unverified** (Launcher type/version not recorded) | If `Preferences.begin()` fails, run with defaults and show "settings not saved"; never block startup |
| R-3 | Other apps may use NVS. Namespaces isolate keys, but not partition space or the R-1 erase | Source-level reasoning | Unique namespace; never erase |
| R-4 | M5GFX already writes its board-detection result to NVS (namespace `M5GFX`, key `AUTODETECT`) when it changes (`M5GFX.cpp:1109-1111`), so current builds may already write NVS | Source-checked; not observed | Count this in the write budget; no action needed |
| R-5 | Flash wear and write frequency | NVS spreads writes over its pages | Follow the agreed per-setting timing in **§6.9** (this supersedes the earlier "write after a few seconds for all settings" idea). Always compare with the stored value first; never write per detent or per key event |
| R-6 | Power loss during a write | ESP-IDF NVS behavior assumed, not tested here | Validate the record (CRC/version) on load; fall back to defaults |
| R-7 | Launcher behavior when apps are switched (whether it touches NVS) | **Unverified** | Hardware test AT-14 |
| R-8 | The USB replug / `USB SUSPENDED` issue (§5 P1) | Open; root cause unknown | Stage 1e must not claim to fix it. Restoring a profile must not depend on USB state |

### 6.6 Other technical risks

- The keyboard-state model (§6.3-2) touches verified Profile 1 code. A full regression is needed.
- HOLD combined with a host's auto-repeat may be surprising for some keys. Document it, and keep TAP as the default.
- Editing applications bind shortcuts per panel and per version. Arrow keys may move a selection instead of the playhead when the wrong panel has focus. **Test, don't assume.**
- Letter and symbol keys depend on the host keyboard layout. Arrows, Space and function keys do not.
- Display and configuration UI drawing must not block encoder polling or USB sends. Use the Stage 1d partial-redraw approach.
- App size grows; confirm it still fits the Launcher's app slot (unverified limit).

### 6.7 Implementation sequence

Each step goes in a **new folder** (e.g. `MediaJog_1e/`), derived from Stage 1d. Stage 1d and all earlier milestones stay unchanged. Each step is gated by review and by a hardware check before the next one starts.

1. **1e-1 — Action model and key-state model; Profiles 3 and 4.**
   - **Status (2026-10-13): implemented and built, not hardware-tested** (`MediaJog_1e_1/`, `docs/STAGE_1E_1.md`).
   - **Hardware test, later on 2026-10-13:** 10 checks passed and one was inconclusive (fast rotation then switching); some checks were not recorded (`STAGE_1E_1.md` §10).
   - As implemented, the scope was narrowed to the minimum needed for four profiles: a per-profile key table selected in `actionFor()`, `Fn+3`/`Fn+4`, and the Profile 4 placeholder.
   - The **key-state model is deferred to 1e-3 (HOLD)**, where it is first needed. Tap-only output does not need it.
   - No behavior change for Profiles 1 and 2.
   - Profile 3 with the fixed Video Editing defaults; Profile 4 with Custom = unassigned.
   - `Fn+3`/`Fn+4`. No persistence.
2. **1e-2 — Configuration UI for Custom** (TAP only, volatile). Leak-proof by design (FR-7).
   - **Status (2026-10-13): implemented and built, not hardware-tested** (`MediaJog_1e_2/`, `docs/STAGE_1E_2.md`).
   - **Hardware test 1 (later on 2026-10-13): FAILED.** `Fn+S` did not open CUSTOM SETUP.
     - Source review found no defect in the `Fn+S` path. It did find a stale `MEDIA JOG 1e-1` label, which made the running build unidentifiable.
     - **Revision r2** (`MediaJog_1e_2_r2/`) adds a build tag on every screen and a live key display. HID behavior is unchanged. It is built, not hardware-tested.
     - The next test decides the cause (`STAGE_1E_2.md` §9–§10).
   - **r2 hardware test (2026-10-13): core functions passed.** `Fn+S` opens setup; assign, Save, Cancel, Custom output and `Fn+G` (Command) work; no host typing during setup; assignments survive profile switches and are cleared at power-off (expected).
     - The checklist is **not** complete: 6 PASS, 1 PASS for typing only, 7 partial, 5 not tested (`STAGE_1E_2.md` §11).
     - The test-1 failure is most consistent with a different build having run.
     - macOS Japanese input (IME) may compose letter keys; this is host behavior, and the firmware does not change it.
   - **r3** (`MediaJog_1e_2_r3/`): overview-screen readability only; built, not hardware-tested (`STAGE_1E_2.md` §12).
   - **UI:** in Profile 4, `Fn+S` opens CUSTOM SETUP. `1`/`2`/`3` or the knob select CW/CCW/BTN.
   - **Editing:** type a key, or turn the knob through a 28-key list (arrows, F1–F12, navigation keys …); `Fn+C/S/A/G` toggle Ctrl/Shift/Alt/GUI.
   - **Save / Cancel:** ok (Enter) = Save, esc (`` ` ``) = Cancel.
   - **Persistence hook for 1e-4:** "Save" commits to RAM in 1e-2. In 1e-4, the same Save is the only point where Custom mappings are written to NVS (§6.9).
3. **1e-3 — HOLD behavior** with the full release rules (§6.3-3), **including the key-state model (§6.3-2) deferred from 1e-1**, and a Profile 1/3 regression because it changes the verified tap path.
   - **Status (2026-10-13): implemented and built, not hardware-tested** (`MediaJog_1e_3/`, `docs/STAGE_1E_3.md`).
   - **Hardware test (later on 2026-10-13), prioritized tests only:** TAP/HOLD selector works; HOLD Shift + CW Right selects text; knob release restores normal movement; a profile switch releases Shift with no stuck modifier. The other H1–H19 items are not tested (`STAGE_1E_3.md` §10).
   - **Key-state model implemented:** every keyboard report = held base state + tap; after a tap the report returns to the base. With nothing held (Profiles 1–3, Profile 4 TAP) the reports are byte-identical to 1e-2 (source and host test).
   - Profile 4 BTN only: TAP or HOLD (`Fn+H` in EDIT BTN). CW/CCW stay TAP. Modifier-only actions (e.g. HOLD Shift) are valid.
   - Release on knob release, profile change, setup entry, U135 loss and USB not ready; no auto-resume. RAM only.
   - **Persistence hook for 1e-4:** TAP/HOLD is part of the Custom mapping, so it is saved only on explicit Save (§6.9).
4. **1e-4 — Persistence** (§6.3-5) with validation, defaults and the write policy.
   - **Status (2026-10-13): implemented and built, not hardware-tested** (`MediaJog_1e_4/`, `docs/STAGE_1E_4.md`).
   - One 21-byte record (magic, version 1, length, CRC-32) in namespace `mediajog`, key `cfg`, via `nvs.h` (not `Preferences`, for explicit error codes). Full validation; all-or-nothing restore.
   - Write policy as in §6.9, sensitivity debounce fixed at **2 s**. Writes run after the HID sends of the loop. Read-only session after a read error or a newer-version record.
   - Host-tested with a mock NVS backend (47/47). Device NVS was not touched.
   - **Hardware (author report, 2026-10-13):** NVS save/restore after restart, Launcher Wi-Fi settings kept, SOUND TERRARIUM normal; no known functional defects; 8 MB flash backed up beforehand. No per-item checklist mapping (`STAGE_1E_4.md` status update). **Fixed as the USB release candidate.**
5. **1e-5 — Hardware regression and application tests** (§6.8). Record the SHA-256 of the exact uploaded binary.

### 6.8 Acceptance tests (to run on hardware; none run yet)

| ID | Test | Pass condition |
| --- | --- | --- |
| AT-1 | Stage 1d regression, Profile 1 | YouTube `.` `,` `k` as in Stage 1d |
| AT-2 | Stage 1d regression, Profile 2 | Pointer, drag, `Fn+1` drag release, sensitivity keys and display as in Stage 1d |
| AT-3 | `Fn+3`, `Fn+4` | Correct profile is selected; nothing reaches the host (TextEdit focused) |
| AT-4 | Video Editing in Premiere Pro, if available | Clockwise/counterclockwise step the playhead by one frame with the timeline focused; press plays/stops. Record version and focus conditions |
| AT-5 | Video Editing in Final Cut Pro, if available | Same as AT-4 |
| AT-6 | Custom: assign keys with and without modifiers (including Command) | The host receives exactly the configured combination (checked in a text editor or with a keyboard viewer) |
| AT-7 | Configuration leak test | During a whole configuration session with a text editor focused, **no character, modifier or click** reaches the host |
| AT-8 | HOLD release rules | A held key is released on profile switch, on configuration entry, on USB unplug, and on encoder loss (if that test is approved; see §2.2) |
| AT-9 | Tap during HOLD | A rotation tap while a HOLD key is down does not release the held key unexpectedly (§6.3-2) |
| AT-10 | Persistence across power cycles | Each profile, the Custom mappings and behavior, and the sensitivity are restored after OFF→ON |
| AT-11 | No restored press | After reboot, no key or mouse button is down until a new physical press |
| AT-12 | Corrupt or absent settings | Defaults are used, the device starts normally, and a reset note is shown |
| AT-13 | Write frequency | Diagnostics counter: no write during rotation; one write per settled change |
| AT-14 | Launcher coexistence | The Launcher and its settings (e.g. Wi-Fi) and other apps still work after repeated MediaJog use and setting changes |
| AT-15 | USB issue unchanged | Document replug behavior; do not claim a fix unless separately tested |
| AT-16 | **Rapid power-off after a profile switch** | Switch profile, then switch the power OFF within about 1 s; repeat at least 10 times across all four profiles. On every boot the stored record is valid, and the restored profile is the one just selected (the save is immediate, §6.9). Any case where the previous profile comes back is recorded, not hidden |
| AT-17 | **Restoration of the last selected profile** | For each of Profiles 1–4: select it, power-cycle, confirm it is restored. No key or button is down after boot (AT-11) |
| AT-18 | **Persistence of Custom mappings and button behavior** | Values are restored after a power cycle **only if Save was selected**. Cancel, leaving the configuration UI without saving, or power-off before Save keeps the previously saved values |
| AT-19 | **Persistence of Spinner sensitivity** | After adjusting and waiting longer than the debounce interval, the value is restored after a power cycle. A power-off **within** the interval may restore the previous value; that is acceptable and is recorded. A burst of adjustments produces one write |
| AT-20 | **NVS data integrity** | The record is validated on every boot (magic, version, length, CRC, ranges). Deliberately corrupted, truncated, missing or older-version records lead to safe defaults, a visible notice and a normal start, never a crash or a stuck key. The device stays usable after repeated AT-16 cycles |
| AT-21 | **Coexistence with the installed Launcher** | After repeated MediaJog saves and power cycles: the Launcher still starts, its own settings (e.g. Wi-Fi) are intact, other apps still start, and switching apps through the Launcher does not corrupt MediaJog's record. Record the Launcher name/version if it can be identified |

AT-16 to AT-21 belong to the persistence sub-stage (1e-4) and the final regression (1e-5). They complement AT-10 to AT-14. None of them applies to 1e-1, which has no persistence.

### 6.9 Persistence timing policy (agreed 2026-10-13; for later sub-stages, not implemented in 1e-1)

| Setting | When it is written | Reason |
|---|---|---|
| **Selected profile** | **Immediately**, as soon as a profile switch is confirmed (the `Fn+1`…`Fn+4` chord has been accepted and the profile has changed) | The device must come back as the user's dedicated controller even if the power is switched off right after selecting. Profile switches are rare human actions, so the write count stays low |
| **Custom key mappings and button behavior** | **Only when the user explicitly selects Save** in the configuration UI | Partial edits are never persisted. Cancel, or leaving without saving, keeps the stored values |
| **Spinner sensitivity** | **After a short debounce interval following the last adjustment** (length to be fixed during 1e-4, for example a few seconds; **fixed in 1e-4: 2 s**, `SENS_SAVE_DELAY_MS`) | Several key presses while tuning produce one write. A power-off inside the interval may lose only the last adjustment |

Rules that apply to all three:
- Compare with the stored value and skip identical writes.
- Never write per detent, per mouse report or per key event.
- Validate on load (§6.3-5).
- Never restore a pressed key or button (FR-10).
- A write must not block encoder polling or USB sends long enough to be noticeable. Measure the `W` loop time during writes.

Required tests: AT-16 to AT-21 (§6.8).

## 7. Later development phases

### Phase 2 — User-configurable profiles

**Stage 1e (§6) is the first concrete step of this phase:** four profiles, local configuration of one Custom profile, and persistence. The broader target below remains future work.

Expand local mode selection (up to nine profiles as a design target), display actions clearly, support application-specific HID usages/modifiers, and save versioned configuration in NVS when stable. Avoid adding confusing duplicate profiles for one application. Host keyboard layouts and app shortcuts require validation.

### Phase 3 — BLE HID and host slots

> **Update 2026-10-13:** after a design review, BLE starts as **separate firmware** so the USB release candidate (1e-4) stays unchanged. **BLE-1** (`MediaJog_BLE_1/`, `docs/BLE_1.md`): core BLE library 3.3.9 (NimBLE), keyboard + mouse report map identical to the USB reports, fixed Right/Left/Space, Just Works bonding; built, not hardware-tested. Integration (shared transport layer, then possibly a manual USB/BLE selector with one active transport) only after BLE is proven on hardware; automatic switching is not planned while the USB reconnect issue (P1) is open.

3a: one BLE host; 3b: pairing storage/deletion for several hosts; 3c: reliable selection and reconnect of one host. Evaluate iPhone app shortcuts separately; do not assume desktop Chrome commands apply to mobile apps. Wi-Fi control is a separate transport requiring a receiving mechanism; not equivalent to native HID.

Known constraints from the source review (REPORT §11; none tested on hardware):
- **ESP32-S3 supports BLE only**, with no Bluetooth Classic.
- **Bond limit:** the precompiled `m5stack:esp32` 3.3.9 libraries set `CONFIG_BT_NIMBLE_MAX_BONDS 3` and `CONFIG_BT_NIMBLE_NVS_PERSIST 1`.
  - Three hosts fill the table exactly; behavior on a fourth pairing or a re-pairing is unverified.
  - The value cannot be changed from the Arduino IDE.
  - Bond deletion is therefore a required feature.
- **"Several hosts paired" and "one chosen host reliably connected" are separate problems.** A peripheral that advertises normally is taken by whichever bonded host connects first. Options:
  - An accept list (whitelist) for the chosen host. This needs IRK resolution, because macOS and iOS use resolvable private addresses.
  - A different own address per slot.
  - Both need hardware validation.
- **iOS:** a BLE mouse needs AssistiveTouch on iPhone. Media keys (consumer control) may be a more practical route than app shortcuts; untested.
- USB HID and BLE HID can coexist in one firmware in principle; an explicit output selector is needed.

### Phase 4 — Editing and emulation

Investigate shortcuts for video/audio editors and test individual emulators/cores (e.g., RetroArch with mouse-capable mappings) without assuming generic compatibility. Distinguish relative spinner input from absolute-position paddle controls and gamepad axes. Use lawfully obtained game content.

> **Update 2026-10-16 (hardware, author):** Arkanoid Tournament (US) in the Internet Archive web MAME is playable with the **existing SPINNER (Fn+2)** over BLE on a Mac (Chrome), firmware `MediaJog_2a_3_r2` unchanged. Mouse input had to be enabled with MAME's `-mouse` option (address-bar JavaScript; depends on the Internet Archive's page code). Esc once restores the hidden Mac pointer afterwards. Speed and responsiveness good; movement slightly stepped (cause not determined; no firmware change now). Details: `STAGE_2A_3_DESIGN.md` §12, REPORT §71.

### Phase 5 — Modular hardware ecosystem

After validating mechanical fit and operation, release multiple printable mounts and potentially swappable controller accessories. Do not sacrifice the successful compact form factor solely to accommodate speculative button layouts.

## 8. Repository organization

Actual layout as of 2026-10-13:

```text
Cardputer-Media-Jog/
├── README.md                  # English public introduction
├── LICENSE                    # MIT, covering all original project materials; scope in README "License"
├── .gitignore                 # Excludes listed build artifacts/temp files; not a guarantee (see README); deletes nothing
├── paddle.3mf                 # 3D-print project file at the root; role/final export to be confirmed
├── images/                    # Exists: 2 photos with camera-generated names (rename before linking)
├── hardware/                  # Exists but empty: for verified CAD/STL/3MF exports
├── docs/
│   ├── DEVELOPMENT_PLAN.md    # This English planning document
│   ├── REPORT.md              # Historical test/investigation report (Japanese, English addenda from §25)
│   ├── STAGE_1A.md            # Japanese stage reports (STAGE_1D has English §9–§10)
│   ├── STAGE_1B.md
│   ├── STAGE_1C.md
│   ├── STAGE_1D.md
│   ├── STAGE_1E_1.md          # Stage 1e-1 (four profiles): hardware-tested in part (§10 there)
│   ├── STAGE_1E_2.md          # Stage 1e-2 (Custom setup UI): r2 core functions hardware-tested; checklist incomplete
│   ├── STAGE_1E_3.md          # Stage 1e-3 (Custom HOLD): prioritized hardware tests passed (§10 there)
│   ├── STAGE_1E_4.md          # Stage 1e-4 (NVS persistence): USB release candidate (author's hardware report)
│   ├── BLE_1.md               # BLE-1 minimal BLE HID prototype: B7 fixed in r4 (§13-14)
│   ├── STAGE_2A_DESIGN.md     # Stage 2a USB/BLE integration: design (Edgar: conditional approval, applied)
│   ├── STAGE_2A.md            # Stage 2a implementation report; hardware: BLE mouse fails (§10)
│   ├── BLE_M1.md              # BLE mouse test builds (second HID service): M1 ms hFFFF; M1 r2 ms c0 (§7–§11)
│   ├── BLE_B1_DESIGN.md       # Proposal B design (one HID service, NimBLE C API)
│   ├── BLE_B1.md              # BLE-B1: hardware PASS
│   ├── STAGE_2A_2_DESIGN.md   # Stage 2a-2 integration design and decisions
│   ├── STAGE_2A_2.md          # Stage 2a-2: hardware results (macOS §7–§8; Windows / ChromeOS USB §9), unverified items, known open issues
│   ├── STAGE_2A_3_DESIGN.md   # Stage 2a-3: KEY STEP, Fn+5 C-SPINNER, ext record (three steps)
│   ├── STAGE_2A_4.md          # Stage 2a-4: C-SPINNER (PROFILE 5) implemented; host-tested, not hardware-tested
│   ├── STAGE_2A_5.md          # Stage 2a-5: colour-coded operation guides (display only); host-tested, not hardware-tested
│   └── STAGE_2A_3.md          # Stage 2a-3 step 1: built, host-tested, not hardware-tested
├── U135_Probe/                # Encoder probe v2 (moved from the root by 2026-10-13). Source has an attribution-only comment fix (SHA-256 3bb64b72…); binary unchanged (REPORT §28)
│   ├── U135_Probe.ino
│   └── U135_Probe.ino.bin
├── archive/U135_Probe/        # Byte-identical copy of the tested probe source (SHA-256 14aaca87…)
├── archive/MediaJog_BLE_1_review1/  # BLE-1 source as reviewed by Edgar (review 1), before the in-place fix (SHA-256 1abae1f9…)
├── archive/MediaJog_2a_review1/     # Stage 2a source as reviewed by Edgar (review 1), before the in-place fix (SHA-256 819b2e48…)
├── LICENSES/M5Stack-MIT.txt   # M5Stack MIT license for M5Stack-derived code (U135_Probe, small portions of MediaJog_*) and evidence
├── MediaJog_1a/               # Each: <stage>.ino, build/<stage>.ino.bin (app-only), other build outputs
├── MediaJog_1b/               #   + SHA256SUMS.txt (1b, 1c, 1d)
├── MediaJog_1c/
├── MediaJog_1d/               # Current reference firmware (latest hardware-used)
├── MediaJog_1e_1/             # Stage 1e-1 four profiles: sketch, build/, SHA256SUMS.txt (hardware-tested in part)
├── MediaJog_1e_2/             # Stage 1e-2 Custom setup UI: sketch, build/, SHA256SUMS.txt (hardware test 1 FAILED: Fn+S; kept as tested)
├── MediaJog_1e_2_r2/          # Stage 1e-2 revision 2: build tag + key diagnostics (hardware core functions passed; checklist incomplete)
├── MediaJog_1e_2_r3/          # Stage 1e-2 revision 3: larger key names on the setup overview (label sizing confirmed by the author)
├── MediaJog_1e_3/             # Stage 1e-3 Custom HOLD: sketch, build/, SHA256SUMS.txt (prioritized hardware tests passed; checklist incomplete)
├── MediaJog_1e_4/             # Stage 1e-4 NVS persistence: sketch, build/, SHA256SUMS.txt (USB release candidate)
├── MediaJog_BLE_1/            # BLE-1: separate BLE-only prototype (Right/Left/Space): sketch, build/, SHA256SUMS.txt (B1-B4, B6 PASS; B7 FAIL)
├── MediaJog_BLE_1_r2/         # BLE-1 r2: reconnection diagnostics (GAP trace, CCCD snapshot): used on hardware for the B7 diagnosis
├── MediaJog_BLE_1_r3/         # BLE-1 r3: connection-resolved diagnostics (screen tag B1r3): used on hardware; trace showed the cause
├── MediaJog_BLE_1_r4/         # BLE-1 r4: fix for subscribe events before CONNECT (tag B1r4): hardware PASS (B7 fixed)
├── MediaJog_2a/               # Stage 2a USB/BLE integrated firmware: hardware: USB OK, BLE keyboard OK, BLE mouse FAIL (STAGE_2A.md §10)
├── MediaJog_BLE_M1/           # BLE mouse feasibility test: second HID service (tag BM1): hardware: mouse not registered (ms hFFFF)
├── MediaJog_BLE_M1_r2/        # BLE-M1 r2: mouse service registered before the GATT server start (tag BM1r2): hardware: ms c0 (Mac did not subscribe)
├── MediaJog_BLE_B1/           # Proposal B: one HID service, static NimBLE GATT table (tag BB1): hardware PASS (2026-10-14)
├── MediaJog_2a_2/             # Stage 2a-2: Stage 2a + BLE-B1 GATT table (tags USB 2a2 / BLE 2a2): main functions hardware-verified; 2 known open issues
├── MediaJog_2a_3/             # Stage 2a-3 step 1: KEY STEP + NVS ext record (tags USB 2a3 / BLE 2a3): hardware: USB OK; BLE KEY STEP 10 lags at fast rotation
├── MediaJog_2a_3_d1/          # Stage 2a-3 diagnostic build d1 (BLE send measurement, tag 2a3d): hardware-measured
├── MediaJog_2a_3_r2/          # Stage 2a-3 r2: reversal purge for KEY STEP 2–10 (tag 2a3r): CURRENT; reversal behavior hardware-verified (BLE / USB)
├── MediaJog_2a_4/             # Stage 2a-4: Fn+5 C-SPINNER on top of r2 (tag 2a4): built and host-tested, NOT hardware-tested; r2 stays the verified base
├── MediaJog_2a_5/             # Stage 2a-5: 2a-4 with colour-coded guides (tag 2a5): built and host-tested, NOT hardware-tested; release candidate for GitHub after the author's display check
└── evidence/                  # Investigation materials; third-party images kept local (see .gitignore)
```

**Do not move or rename firmware folders as part of a documentation-only update.** Leave `paddle.3mf` in place until the model's role and final export files are confirmed. Older documents cite the probe at its previous root path; those references are historical (REPORT §26).

## 9. Workflow, safety, and review responsibilities

1. Preserve known-good artifacts and record SHA-256 before each new firmware branch.
2. **Claude Opus 5.5**: investigate and implement firmware changes, inspect low-level APIs, and provide reproducible build reports.
3. **Edgar (GPT-6)**: independently review design, diffs, test criteria, and consistency of user-facing claims. Documentation updates do not constitute firmware validation.
4. **Project author**: define requirements, design and print hardware, run real-device tests, and make release decisions.
5. Label every statement **tested**, **built/source-reviewed**, **modeled/printed**, or **planned**. Do not silently upgrade one evidence class to another.
6. Keep user-facing installation simple and transparent; avoid unnecessary host-side software and invasive permissions.

## 10. Next-session checklist

1. ~~Save the new `README.md` and replace `docs/DEVELOPMENT_PLAN.md` with this English update.~~ Done 2026-10-13. **Still open:** no dated backup of the Japanese plan exists in the repository. Add it as `docs/archive/` if a copy exists.
2. Add photographs of **both** printed bases (flat and 15°). Give them descriptive names (e.g. `flat-base-top.png`, `angled-15-side.png`) before linking them. Use photos of the printed parts, not CAD renders.
3. ~~Have Claude review the English documents.~~ Done 2026-10-13: review findings applied (README, this plan, `STAGE_1D.md` §9–§10, REPORT §25–§26, `LICENSE`, `.gitignore`).
4. When uploading firmware, record the SHA-256 of the exact file uploaded, and run the stage checklist item by item.
5. Investigate USB reconnect in a new working branch while preserving Stage 1d.
6. Test Windows/Linux before changing the README's supported-platform status.
7. ~~Finish the 15° wedge print.~~ Done 2026-10-13: printed and assembled; the author reports no substantial difference in operating feel versus the flat base. Prepare both bases for publication as alternatives. Systematic stability/fatigue checks are optional and not done.
8. Before publishing:
   - Choose how to distribute binaries (see README "Publishing binaries").
   - After initializing Git (not done yet; author's decision), inspect the staged files with `git status` and `git diff --cached --name-only`. `.gitignore` excludes the listed artifacts but does not guarantee that nothing unwanted is staged.
   - License: decided 2026-10-13. MIT covers all original firmware, 3D-print designs, documentation and photographs; third-party material keeps its own license (README "License").
9. **Stage 1e (planned):** review the §6 design (action model, key-state model, configuration entry key, persistence record) **before** writing code. Then verify `Fn+3`/`Fn+4` on hardware early (1e-1). Then confirm the Launcher's NVS partition behavior (R-2, R-7) before relying on persistence.
   - **2026-10-13:** 1e-1 is built (`docs/STAGE_1E_1.md`). Next: GPT review, then the §6 checklist in `STAGE_1E_1.md` on hardware, recording the SHA-256 of the uploaded binary.
   - **2026-10-13 (later):** 1e-1 is hardware-tested in part. Its open items are fast rotation then switching (inconclusive), the Profile 1 regression, Spinner → Profile 1 drag release, and re-selecting the current profile.
   - 1e-2 is built (`docs/STAGE_1E_2.md`). Next: GPT review, then the §6 checklist there, recording the SHA-256 of the uploaded binary.
   - **1e-2 test 1 failed** (`Fn+S`). Next: GPT review of r2, then the r2 test (`STAGE_1E_2.md` §10-3).
     - Remove older MediaJog builds from the Launcher, or check the `1e-2r2` tag on screen.
     - **Always record the uploaded binary's SHA-256.**
   - **Stage 2a hardware (2026-10-13): BLE mouse fails** (mouse report never registered by the BLE library). Test build BLE-M1 (second HID service) is ready: `BLE_M1.md` §5; report to Edgar before integration or proposal B.
   - **BLE-M1 hardware (2026-10-14): `ms hFFFF`** (mouse service started after the GATT server start; `BLE_M1.md` §7). BLE-M1 r2 built and host-tested; waiting for Edgar's review before installation (`BLE_M1.md` §8).
   - **BLE-M1 r2 hardware (2026-10-14): `ms c0`**, proposal A not working as implemented. **BLE-B1 (proposal B)** built and host-tested; waiting for Edgar's review before installation and any Mac re-pairing (`BLE_B1.md` §5).
   - **BLE-B1 hardware PASS (2026-10-14).** **Stage 2a-2** (B1 table in Stage 2a) built and host-tested; waiting for Edgar's review before installation (`STAGE_2A_2.md` §5). Known limitation: at most 2 BLE hosts (NimBLE CCCD store).
   - **Stage 2a-2 hardware test 1 (2026-10-14):** USB and BLE YouTube operation and both switch directions PASS. Open: the B1 bond was not taken over (new pairing needed), USB first `NOT MOUNTED`; BLE mouse untested (`STAGE_2A_2.md` §7).
   - **Stage 2a-2: main functions hardware-verified (2026-10-14)**: BLE mouse movement and drag, BLE → USB switch while dragging, restart in USB mode also PASS. Not exhaustive (`STAGE_2A_2.md` §8.3). Known open issues: B1 bond not reused, first USB recognition failed (§8.4).
   - **Stage 2a-2 on Windows and ChromeOS (2026-10-14):** USB, YOUTUBE and SPINNER (pointer) PASS on both. BLE on these OSs not verified; ChromeOS is not evidence for general Linux (`STAGE_2A_2.md` §9).
   - **Stage 2a-3 step 1 (2026-10-16):** KEY STEP (1–10 per key profile) and the `ext` NVS record built and host-tested (KEY STEP 1 output identical to 2a-2). Waiting for review and the hardware check (`STAGE_2A_3.md` §4); steps 2–3 (Fn+5 C-SPINNER) not started.
   - **Stage 2a-3 hardware (2026-10-16):** USB KEY STEP 10 fine; BLE KEY STEP 10 lags after fast rotation (`P100 D428`, `T22`). Diagnostic build d1 ready for measurement (`STAGE_2A_3.md` §5–§6). C-SPINNER on hold.
   - **Stage 2a-3 r2 (2026-10-16):** reversal purge (method A) built and host-tested; hardware evaluation pending (`STAGE_2A_3_DESIGN.md` §8). BLE pacing (method B) decided afterwards.
   - **Stage 2a-3 r2 hardware (2026-10-16):** reversal purge passes over BLE and USB; method B not implemented; r2 kept as the current build. Open issue: `BLE READY` without input (cause not determined, `STAGE_2A_3_DESIGN.md` §10). C-SPINNER (step 2) still on hold.
   - **r2 additional tests (2026-10-16):** BLE KEY STEP 1 and BLE CUSTOM HOLD with reversals PASS. Not verified: a knob press during a reversal, long-term use.
   - **Web MAME Arkanoid (2026-10-16):** the existing SPINNER (Fn+2, r2, BLE, Mac Chrome) plays Arkanoid Tournament with MAME `-mouse`; Esc restores the pointer; slightly stepped movement, cause not determined. Decisions: SPINNER SENS kept; Fn+2 settings and behavior unchanged; C-SPINNER (Fn+5) stays a general customizable SPINNER (own SENS, direction, key / mouse L / R button, TAP / HOLD, saved), not Arkanoid-specific, not started; Fn+2 unchanged is its top priority (`STAGE_2A_3_DESIGN.md` §12).
   - **Arkanoid baseline (2026-10-16):** SPINNER SENS **64** in that test (speed fine, slightly stepped); baseline for later comparisons (`STAGE_2A_3_DESIGN.md` §12.5).
   - **Stage 2a-4 (2026-10-16): C-SPINNER (Fn+5) implemented** in `MediaJog_2a_4/` (source `fc17bf73…`, bin `4e1dc5ed…`): own sensitivity (same 14 steps, default 8), direction, button key / mouse L / R / none with TAP / HOLD, SETUP, `ext` storage. Build OK, no warnings; host tests 80/80 + earlier suites as r2. **Not hardware-tested.** r2 kept unchanged as the verified base. UI points to confirm: DIR toggled with `2` / knob press (`ok` = SAVE), SENS also in SETUP (`STAGE_2A_4.md` §2, §10).
   - **Stage 2a-4 hardware (2026-10-16): PASS** on the author's checks (BLE pointer on Fn+2 / Fn+5, independent sensitivities 8 / 64, direction incl. power cycle, mouse L / R TAP / HOLD, key A TAP / HOLD, Shift+A, NONE, NVS, USB pointer; `STAGE_2A_4.md` §11). Note: the tree line above ("NOT hardware-tested") predates this result. Not covered: Arkanoid with Fn+5, Windows / ChromeOS, long-term use.
   - **Stage 2a-5 (2026-10-16):** colour-coded operation guides (cyan operation, white description), display code only; build OK; host tests as 2a-4 plus display comparison 270/270. **Not hardware-tested.** Public repository files prepared (`firmware/MediaJog_2a_5`, README, guides), not committed; commit / push by the author after the display check (`STAGE_2A_5.md`).
   - **Stage 2a code review 1 (2026-10-13):** owed-release completion limited to all-up / button-up reports; 85/85 end-to-end checks. Ready for the hardware test (`STAGE_2A.md` §7; binary `0b0dfc44…`).
   - **Stage 2a implemented (2026-10-13):** `MediaJog_2a/`, built and host-tested (66 end-to-end checks; `docs/STAGE_2A.md`). Next: Edgar's code review, then the §7 hardware checklist there.
   - **BLE-1 r4 passed on hardware (2026-10-13); Stage 2a design written** (`docs/STAGE_2A_DESIGN.md`). Next: Edgar's design review and decisions D1–D8; then implementation in `MediaJog_2a/`.
   - **B7 cause found (2026-10-13, r3 trace):** CCCD RESTORE arrives before the library's `onConnect()` (NimBLE posts CONNECT after the feature exchange); BLE-1 rejected and cleared it. BLE-1 r4 fixes the handle bookkeeping (`BLE_1.md` §13). Next: Edgar's review, then §13-6 on hardware.
   - **B7 r2 diagnosis (2026-10-13):** root cause not established; most supported chain: failed security procedure on reconnection → no CCCD restore. BLE-1 r3 (diagnostics, `BLE_1.md` §12) is built. Next: Edgar's review, the §12-6 one-shot run, then a fix only if the trace supports it.
   - **BLE-1 hardware: B1-B4, B6 PASS; B7 (Mac Bluetooth OFF→ON) FAIL.** BLE-1 r2 (diagnostics) is built (`BLE_1.md` §11). Next: Edgar's review, then the §11-6 procedure, then a targeted fix.
   - **BLE-1 is built** (`docs/BLE_1.md`). Next: Edgar's review, then the §8 checklist there (record the uploaded hash and the NVS line).
   - **1e-4 is built** (`docs/STAGE_1E_4.md`). Next: Edgar's review, **including acceptance of the pre-existing core NVS erase risk (§6.3 there)**; optional flash backup by the author; then the §9 checklist there, recording the uploaded binary's SHA-256.
   - **1e-3 is built** (`docs/STAGE_1E_3.md`). Next: GPT (Edgar) independent review, then the §7 checklist there (including the Profile 1–3 regression), recording the uploaded binary's SHA-256.
   - The persistence timing policy is fixed in §6.9.
