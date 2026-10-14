# Stage 2a — USB / BLE integrated firmware (report for review)

Created: 2026-10-13 · Implementation: Claude Opus 5.5 · Design: [`STAGE_2A_DESIGN.md`](STAGE_2A_DESIGN.md) (Edgar: conditional approval; D1–D8 adopted; both conditions applied)
**Status: implemented, built and host-tested. NOT installed on hardware. Submitted to Edgar for code review before any hardware test.**

> **Status update (2026-10-13, appended): Edgar's code review — minor fix, then hardware testing.**
> - Approved: the integration design, the common backends, the r4 port, NVS compatibility, and the "no warning without a host link" change.
> - Fixed in place (never installed): an owed release now completes only on an **all-keys-up** keyboard report and a mouse report with **no button pressed** (§9).
> - Hashes in §1 and §6 are the fixed build. The reviewed source is kept in `archive/MediaJog_2a_review1/`.

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

## 1. Files and hashes

| File | Size | SHA-256 |
|---|---|---|
| `MediaJog_2a/MediaJog_2a.ino` | 155,880 B | `eb42955bffba28afcc46c232db3c02d31b40bc8efbd33d69da781d1a2f0c7fc7` |
| `MediaJog_2a/build/MediaJog_2a.ino.bin` (app-only, Launcher Wi-Fi upload) | 896,320 B | `0b0dfc44cf5cd5da20304e650750308de5bace475d2fd31a5d728981d347deb1` |

- Both hashes are recorded in `MediaJog_2a/SHA256SUMS.txt`; `shasum -a 256 -c` → OK.
- **Unchanged and verified:** `MediaJog_1e_4/` (USB release candidate) and `MediaJog_BLE_1_r4/` (BLE reference), both `SHA256SUMS.txt` OK.

## 2. How it was built from the two sources

| Part | Source | How |
|---|---|---|
| Profiles, U135, HOLD, queue, CUSTOM SETUP, NVS settings, display | `MediaJog_1e_4` | The file was copied and edited. **54 of 71 functions are byte-identical** to 1e-4, including `loadSettings`, `writeSettings`, `serviceSettings`, `sendQueued`, `releaseHoldNow`, `setProfile`, `enterSetup`, `setMouseButton` and the CUSTOM editor |
| BLE backend | `MediaJog_BLE_1_r4` | **Cut out of the r4 file by markers, not retyped.** All 27 moved functions and classes are **byte-identical** to r4 (§4) |
| Transport layer, switch, `link` record, CONNECTION screen | new | §3 |

**Changed 1e-4 functions (17), all intended:**
- **Send path:** `sendKeyReport` (now returns SENT / NOT_SENT / UNKNOWN via the active backend), `sendRelease`, `tapKey` and `sendMouse` (same rules on top of the 3 results), `pushEvent` (BLE SKIP texts).
- **Link state:** `updateUsbState` (USB host state in either mode; drives `hidReady` only when USB is active).
- **Routing:** `onRotation`, `onButton` (CONNECTION screen routing; `usbReady` renamed `hidReady`); `processSetupKeys` (dispatch to the CONNECTION screen); `pollKeyboard` (`Fn + B`).
- **Display:** `drawBuildTag`, `drawStatus`, `drawSpinnerFull` (transport state, right-aligned tag, warning).
- **NVS:** `nvBackendRead` / `nvBackendWrite` (now thin wrappers of keyed versions, same behavior).
- **Startup and main loop:** `setup`, `loop`.

## 3. What is new

### 3.1 Backends and the send rule

| Result | USB | BLE |
|---|---|---|
| `TX_SENT` | `SendReport()` true (IN transfer completed) | `SUCCESS_NOTIFY` (accepted by NimBLE) |
| `TX_NOT_SENT` | `hidOut.ready()` false | not accepted, or no status |
| `TX_UNKNOWN` | `SendReport()` false after `ready()` | — |

- `hidSendKeyboard()` / `hidSendMouse()` go to the **active** backend only.
- `usbSend*` / `bleSend*` are called by name only for the switch release and for an owed release.
- On top of the three results the 1e-4 rules are unchanged. A press that was NOT_SENT stays queued; an UNKNOWN press is not resent; a release is retried until SENT; movement is removed unless NOT_SENT.
- These rules reproduce 1e-4 exactly for USB and BLE-1 r4 exactly for BLE.

### 3.2 Link up / down

- `onLinkDown()` holds the 1e-4 not-ready actions: HOLD released, queue cleared, mouse button released, movement dropped. It also clears the BLE all-up repeat.
- `onLinkUp()` sets all-up first. BLE (only) adds the 100 ms repeat, as in BLE-1.
- USB drives them from `updateUsbState()`. BLE drives them from r4's own `onLinkLost()` / `onLinkReady()`, which in 2a act only while BLE is the active transport and no switch is running.

### 3.3 Switch (design §4.3, Edgar's condition 1)

`loop()` runs a short state machine (`serviceSwitch()`); it never blocks.

0. **Release needed only with a host link** on the old transport: USB mounted (also when suspended), BLE connected. Otherwise the switch commits at once, without a warning. This was added during implementation, because every USB → BLE switch on battery would otherwise warn.
1. Each loop, up to **200 ms**, send all-up and a released mouse report on the old transport, if it is READY. For BLE, a successful release is followed by a **100 ms** flush before the link is terminated. Hard bound 300 ms.
2. **Commit:**
   - `onLinkDown()`;
   - BLE: `stopAdvertising()`, `advertiseOnDisconnect(false)`, `ble_gap_terminate(handle, 0x13)`; no bond change;
   - set the new transport; the `link` record is saved at once; the screen closes.
3. **Release not confirmed:**
   - `releaseOwed[old]` is set; a red `!` appears in the tag (`!BLE 2a`) and in the Spinner header; the CONNECTION screen shows `! OLD <X> RELEASE UNCONFIRMED`.
   - **USB owed while BLE is active:** when USB becomes READY, exactly one all-up and one released mouse report go to USB. This is the only USB traffic in BLE mode.
   - **BLE owed:** cleared by the first all-up on BLE after it becomes active again.
4. **Nothing is carried over:** the internal state is all-up from the start of the switch, and HOLD needs a new physical press.
5. **Limit:** after a link is gone, the host's key state cannot be guaranteed by the firmware (design §4.3 item 4).

### 3.4 BLE on / off (D1: runtime, lazy)

- The BLE stack starts only when BLE is used: at boot if the saved transport is BLE, otherwise at the first switch to BLE. In USB-only use it never starts, so there is no radio and no BLE/PHY NVS activity.
- Off = no advertising and the link terminated; the stack is not de-initialized.

### 3.5 Saved transport (design §7)

- NVS `mediajog` / `link`, 11 bytes: `MJLK`, version 1, length, transport, CRC-32. It is validated like `cfg`.
- **Default USB.** A newer-version record or a read error makes the record read-only for the session.
- It is written at once after a confirmed switch, only if it differs, using the same guarded backend (space check, read-back, no erase).
- **`cfg` (version 1) is unchanged:** 1e-4 and 2a read each other's settings, and a downgrade to 1e-4 ignores `link`.

### 3.6 Screens (Edgar's condition 2)

| Screen | Content |
|---|---|
| Status line 1 | `USB HID READY` / `USB SUSPENDED` / `USB NOT MOUNTED`, or `BLE READY` / `BLE ADVERTISING` / `BLE PAIRING` / `BLE SUBSCRIBING` / `BLE STARTING` |
| Top-right | `USB 2a` / `BLE 2a` (green / cyan), red `!` prefix while a release is owed; right-aligned by measured width |
| CONNECTION (`Fn + B`) | The selection (`1 USB*` / `2 BLE`, `*` = active); the active state; the inactive side: **`USB HID INACTIVE (mounted / not mounted)`** in BLE mode, `BLE OFF` in USB mode; BLE details (bond, CCCDs, handle, adopted, foreign); switch counters; warnings; `t` = BLE trace pages (r3/r4 diagnostics, frozen, 13 lines per page) |

## 4. BLE-1 r4 regression guard (top priority)

- **Byte comparison, function by function (r4 file vs 2a file):** **identical** — `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`; classes `ServerCallbacks`, `ReportCallbacks`, `SecurityCallbacks`; `notifyReport`, `updateLinkState`, `toIdAddr`, `countCccd`, `onGapEvent`, `subChar`, `subReasonChar`, `formatTrace`, `formatTraceLine`, `drainTrace`, `bleBegin`, `traceSecOf`, `pushTrace`, `traceNewest`, `traceRead`, `countConnections`, `freezeTrace`, `tracePages`.
- **Intended differences:** `onLinkLost` / `onLinkReady` (connected to the common link-down / up and active-transport check) and `drawTracePage` (variable and tag function names).
- **The RESTORE-before-CONNECT path is tested end to end** (§5, S11–S13), and a mutant that restores r3's "clear on connect" behavior fails 6 checks.

## 5. Host tests ✅ (logic only, not hardware)

### 5.1 End-to-end: the real sketch on stubs — 66/66

- **What was compiled:** the Arduino-preprocessed sketch (`arduino-cli --preprocess`, i.e. exactly what the compiler sees, with the generated prototypes), checked to equal the final source. It was compiled on the Mac with stubs for the Arduino core, M5Cardputer (display, keyboard), Wire (U135 registers), TinyUSB HID, NVS (in memory) and the BLE library / NimBLE.
- **The NimBLE stub follows the documented order:** listener first, then the library's callbacks. TERM before DISCONNECT; RESTORE after encryption; the library's `notify()` statuses; a link that exists before CONNECT is posted.
- **How it runs:** each scenario runs in a fresh process through `setup()` and `loop()`.

| Scenario | What is checked |
|---|---|
| S01 | Boot without `link` → USB; **BLE never started**; USB all-up first; Profile 1 `.` on USB; `USB HID READY`, tag `USB 2a` |
| S02 | CUSTOM HOLD Shift + CW Right → Shift, Shift+Right, Shift, all-up (USB) |
| S03 | HOLD active → `Fn + B` releases Shift at once; after closing, the old knob release sends nothing; a new press holds again |
| S04 | CONNECTION + esc: no switch, no NVS write |
| S05 | USB → BLE: USB all-up + released mouse; BLE started once; `link` = BLE; BLE READY with all-up first; `.` over BLE; **no USB report while BLE is active**; `BLE READY`, `BLE 2a`, **`USB HID INACTIVE`** |
| S06 | USB not mounted: switch at once, no warning |
| S07 | USB suspended: commit after 200 ms with USB owed, `!BLE 2a`, nothing on USB; on resume exactly the owed all-up + released mouse, then cleared; USB silent afterwards |
| S08 | BLE → USB: release accepted, **no termination before the 100 ms flush**, then terminated, advertising off, no re-advertise, ≤ 300 ms; no warning; `link` = USB; USB all-up first; the later BLE disconnect does not affect USB |
| S09 | BLE release rejected: commit after 200 ms with BLE owed (`!USB 2a`), link still terminated; back to BLE → first all-up accepted → cleared |
| S10 | HOLD active across a switch: BLE starts all-up, no Shift; the old knob release sends nothing |
| S11 | **RESTORE before CONNECT** (B7 order): adopted, kept, not FGN, READY after CONNECT with all-up first, trace `aADP` / `aCON keep`, HID works |
| S12 | BLE disconnect / reconnect (restore first): inputs while down dropped, no replay, all-up repeated once |
| S13 | Failed connect before CONNECT drops the adoption; a reused handle does not inherit; another live handle is FGN while the current link stays READY |
| S14 | Spinner over BLE: drag button, movement, release on disconnect, released first after reconnect, old knob release sends nothing |
| S15 | BLE notify rejected: the press is retried once; no movement lost (8 counts) |
| S16, S17 | Corrupt `link` → USB; newer `link` → USB at boot and never overwritten |
| S18 | `cfg` record byte-identical after two transport switches |

### 5.2 Mutation check

The suite was run against deliberately broken copies of the preprocessed sketch:

| Mutant | Failing checks |
|---|---|
| No adoption (r4 fix removed) | 9 |
| r3 behavior (connect always clears) | 6 |
| No release on the old transport | 2 |
| Keyboard also sent to USB (double send) | 3 |
| No owed USB release | 1 |
| Termination without the BLE flush | 1 |
| HOLD release removed at CONNECTION entry and switch start | 1 |
| HOLD release removed at switch start only | 0 (expected: entry and commit still release it) |

### 5.3 Earlier suites on the 2a code

- **1e-4 NVS suite: 47/47.** The 13 settings functions are byte-identical to 1e-4.
- **1e-3 HOLD report suite: 16/16**, through 2a's `sendKeyReport` / `sendRelease` / `tapKey` with the USB backend's logic.

## 6. Build (2026-10-13)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, **0 sketch warnings**. One `-Wextra` enum warning in `loadLink()` was fixed during development. Core/library warnings only (pin macros, `KEY_TAB`/`KEY_BACKSPACE`, `cdc_line_control_state_t`, SPI register structs) |
| Size | **Flash 896,179 B (68 %)**, RAM 69,228 B (21 %) of the default 1,310,720-byte partition (after review 1). ❓ Launcher slot unverified |
| Guards | ✅ hwcdc and CDC-off builds stop with `#error`; non-NimBLE configuration stops |
| Binary | Contains `MediaJog BLE`, `mediajog`, `link`, `USB HID INACTIVE (…)` |
| NVS calls in the sketch | `nvs_open/get_blob/set_blob/commit/get_stats/close` on `mediajog` only; no erase, no bond deletion |

## 7. Hardware test checklist (proposed; after Edgar's code review)

Install `MediaJog_2a.ino.bin` (app-only) via the Launcher Wi-Fi upload and **record its SHA-256** (`0b0dfc44…47deb1`; the pre-review binary `ddd52d99…` is superseded and must not be installed). The first start is in USB mode, because there is no `link` record yet.

| # | Do | Pass |
|---|---|---|
| A1 | USB mode: the 1e-4 checks (four profiles, Spinner drag, CUSTOM Save/Cancel, HOLD Shift + Right, release on `Fn + 1`, sensitivity, power cycle restores) | As 1e-4; tag `USB 2a` |
| A2 | `Fn + B`, `2`, ok | Switches to BLE; `BLE ADVERTISING` → the Mac reconnects the existing bond (or pair once) → `BLE READY` |
| A3 | BLE: Profiles 1, 3, CUSTOM (with Command, HOLD Shift + Right) | Same results as over USB |
| A4 | BLE: Spinner movement and drag in the Breakout game | Usable; note `clip` / `FAIL` / `W` |
| A5 | Mac Bluetooth OFF → ON (3×); Cardputer restart | Reconnects; restart comes back in BLE mode |
| A6 | USB cable to the Mac in BLE mode | CONNECTION shows `USB HID INACTIVE (mounted)`; no USB key output |
| A7 | HOLD Shift active, then `Fn + B`, switch | No stuck Shift on either host; no `!` (or, if `!`, it clears when that host is READY again) |
| A8 | Switch BLE → USB → BLE several times | Each switch < 1 s; no stuck keys; `link` restored after power cycle |
| A9 | Launcher Wi-Fi settings, SOUND TERRARIUM, 1e-4 afterwards | Still working; 1e-4 reads the same settings |
| A10 | Note the flash size the Launcher reports, if any | Recorded |

## 8. Known limitations and residual risks

- **Not hardware-tested.** TinyUSB HID and NimBLE are active together only during a switch, and a USB owed release can be sent in BLE mode. ❓
- BLE mouse throughput is ❓ (design §5, D8). The send limits are the same as for USB.
- The host side after a link loss cannot be guaranteed. The `!` warning reports unconfirmed releases.
- The BLE library's Security Request at every CONNECT still produces the harmless `ENC !D` 30 s timeout (BLE-1 §13-3; D6).
- No "forget pairing" (D7); one BLE host; advertising runs whenever BLE is on and not connected.
- With a USB cable in BLE mode, the Mac sees a silent USB keyboard + mouse as well as the BLE device.
- The Arduino core's boot-time NVS erase path is unchanged (`STAGE_1E_4.md` §6.3).

## 9. Edgar's code review 1 (2026-10-13) and fix

**Finding:** `noteReleaseSent()` was also called from the normal `sendRelease()` and `sendMouse()`. `sendRelease()` sends the **base** report, which can contain a CUSTOM HOLD, and `sendMouse()` can send the drag button pressed. Either could have marked an owed release as done.

**Fix** (only the completion test; HOLD behavior and send order unchanged; 3 code lines plus a comment):

| Call site | Counts as release only if |
|---|---|
| `sendRelease()` | the base carried no HOLD: `kbHoldMods == 0 && kbHoldUsage == 0`, i.e. the report SENT was all keys up |
| `sendMouse()` | `r.buttons == 0` in the report that was SENT |
| `serviceOwedUsbRelease()`, switch release | unchanged: these send explicit all-up / button-up reports |

**New regression scenarios (19 checks), all passing:**

| Scenario | Checks |
|---|---|
| S19 | USB owed, then the same transport (USB) is active again: still owed while suspended. On resume, in the same loop as a CW detent, the first USB keyboard report is the all-up and then the `.` tap; the first mouse report is released; owed cleared, no `!` |
| S20 | CUSTOM HOLD Shift: a base report `[Shift]` SENT does **not** complete the owed keyboard release; the all-up base after HOLD up completes the keyboard half only; the released mouse report completes it |
| S21 | Spinner drag: mouse reports with the button down do **not** complete the owed mouse release; the button-up report does |
| S22 | USB owed in BLE mode, mouse release fails once on resume: keyboard all-up sent, still owed, `!` stays; next loop the mouse release is sent, then cleared and `!` gone; the keyboard all-up is not sent twice |
| S23 | BLE owed with the mouse CCCD on: a keyboard all-up alone does not clear it; the accepted released mouse report does |

**Results:**
- **End to end 85/85:** the 66 earlier checks, including B7 S11–S13 and USB/BLE operation, plus the 19 new.
- **The pre-fix source** (`archive/MediaJog_2a_review1/`) **fails exactly 2 of them** (S20 HOLD base, S21 drag), so the tests detect the reported defect.
- **1e-4 NVS suite 47/47; 1e-3 HOLD suite 16/16.**
- **Clean build:** exit 0, 0 sketch warnings, guards effective; flash 896,179 B.
- **Unchanged:** 1e-4 and BLE-1 r4 (checksums OK).

| Build | Source SHA-256 | App-only binary SHA-256 |
|---|---|---|
| Reviewed (superseded, never installed) | `819b2e484f5a972ca25a5e15533ece9bf31d90d381ec9cd6c2f6a24436f8ce39` | `ddd52d997e57e000ea35eca6e54f6aa7cd0852153589b9eb5069b4ff58a11c33` |
| **Current** | `eb42955bffba28afcc46c232db3c02d31b40bc8efbd33d69da781d1a2f0c7fc7` | `0b0dfc44cf5cd5da20304e650750308de5bace475d2fd31a5d728981d347deb1` |

## 10. Hardware test (2026-10-13, author) — BLE mouse not working; cause; test build M1

| Check | Result |
|---|---|
| USB HID READY; USB Profile 3 (cursor left/right); USB Profile 2 (pointer left/right) | **PASS** |
| BLE READY; BLE Profile 3 (cursor left/right) | **PASS** |
| BLE → USB → BLE without restart | **PASS** |
| Transport saved in NVS, restored after restart (USB and BLE) | **PASS** |
| **BLE Profile 2 (Spinner)** | **FAIL:** the pointer does not move and `rep` does not increase; CONNECTION shows `bond=1 kb=1 ms=0`; unchanged after Mac Bluetooth OFF → ON |

BLE trace (author): `CONN ok`, `aCON new`, `LEST ok`, `ENC ok`, `SUB x R0`, `SUB k R1`, `aSUB k1 ok`, `SUB x R1`, `RDY`. Uploaded SHA-256 not reported. Other §7 items not reported.

**Cause (source; reproduced in the host-test model):**
- The BLE library (NimBLE build) registers only the first characteristic of a UUID per service.
- `BLEHIDDevice`'s keyboard output and mouse input Report characteristics (UUID 0x2A4D, like the keyboard input) are never in the GATT table; the mouse handle is 0xFFFF.
- The Mac therefore has no mouse report to subscribe to. `x` = characteristics other than keyboard and mouse (probably Service Changed and Battery Level; not verified).
- This was not caught by the §5 host tests, because their stub gave every report characteristic its own handle. That stub model was wrong; it has been corrected for the BLE-M1 tests.

**Decision (Edgar):** proposal A (mouse in a second HID service) first, verified in a separate test build: `docs/BLE_M1.md`. Stage 2a is unchanged until that result is known.
