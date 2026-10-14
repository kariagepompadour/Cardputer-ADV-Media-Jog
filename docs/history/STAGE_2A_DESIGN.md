# Stage 2a — USB / BLE integrated firmware: design (for review)

Created: 2026-10-13 · Author: Claude Opus 5.5
**Status: design only. No integration code exists. Implementation starts only after Edgar's review and the author's decisions in §12.**

> **Review update (2026-10-13, appended):**
> - **Edgar: conditional approval. D1–D8 adopted as proposed.**
> - The two conditions are reflected in **§4.3** (release on the old transport within 200 ms, warning, priority release when the old transport is READY again, no carry-over, no guarantee for the host side after a disconnect) and in **§4.1 / §6** (BLE mode shows `USB HID INACTIVE`, not "power only").
> - Implementation proceeds per §11.

Legend: ✅ checked in source, build or probe · ❓ not verified on hardware · **[decision]** needs review / author decision

## 1. Goal and sources

One firmware for the Cardputer ADV + U135 that works as **USB HID or BLE HID**, selected on the Cardputer itself. All four profiles (YouTube, Spinner, Video Editing, CUSTOM) work on both transports, including Spinner movement, HOLD-drag, CUSTOM modifiers and TAP/HOLD.

| Source | Role | Status |
|---|---|---|
| `MediaJog_1e_4/` (2,123 lines) | **Functional base:** profiles, U135 input, HOLD, CUSTOM SETUP UI, NVS settings, USB HID safety | USB release candidate (author hardware report) |
| `MediaJog_BLE_1_r4/` (1,603 lines) | **BLE backend:** NimBLE HID-over-GATT, security, link-ready rules, CCCD-before-CONNECT handling, diagnostics | Hardware: start → `BLE READY`; Mac Bluetooth OFF→ON 3× auto-reconnect; Cardputer restart reconnect; Right/Left and one Space per press |

Both stay unchanged. Proposed new folder: **`MediaJog_2a/`** **[decision D3]**.

## 2. Facts from the comparison

### 2.1 HID reports are identical ✅

| | USB 1e-4 | BLE-1 r4 |
|---|---|---|
| Keyboard | Report ID 1, `hid_keyboard_report_t` = modifier, reserved, keycode[6] (8 bytes, TinyUSB `hid.h`) | Report ID 1, `KeyboardReport`, the same 8 bytes; LED output report, 1 byte |
| Mouse | Report ID 2, `hid_mouse_report_t` = buttons, x, y, wheel, pan (5 bytes) | Report ID 2, `MouseReport`, the same 5 bytes (`static_assert`) |
| Report map | TinyUSB `TUD_HID_REPORT_DESC_KEYBOARD/MOUSE` (via USBHIDKeyboard/USBHIDMouse) | The same descriptors written out byte by byte |

So the common code builds one report and hands the same bytes to either backend.

### 2.2 Where 1e-4 depends on USB ✅

The dependence on USB is narrow:
- **`hidOut` (sending):** `sendKeyReport()` :548/557, `tapKey()` :576, `sendMouse()` :731/736/745.
- **Link state:** `onUsbEvent()` :529 (suspend/resume), `updateUsbState()` :766 (`ready = USB && !usbSuspended`).
- **Readiness checks:** `usbReady` is read in `onRotation()` :1907, `onButton()` :1972/1985, `pushEvent()` :654 and `loop()` :2068.
- **Display:** the USB state lines in `drawStatus()` :1568 and `drawSpinnerFull()` :1691.
- **Setup:** USB start in `setup()`.

Everything else is transport-independent: profiles, `actionFor()`, the HOLD base state, the queue, CUSTOM SETUP, the settings record and write policy, U135 polling, sensitivity, display.

### 2.3 Delivery semantics differ, but map onto one rule ✅

| Result | USB (`hidOut`) | BLE (`notify()` + `onStatus()`) |
|---|---|---|
| **SENT** | `SendReport()` true (IN transfer completed) | `SUCCESS_NOTIFY` (accepted by the NimBLE host) |
| **NOT_SENT** (nothing left the device) | `ready()` false | any other status, or none |
| **UNKNOWN** (may still arrive) | `SendReport()` false after `ready()` (100 ms timeout) | — (does not occur) |

1e-4 and BLE-1 r4 already follow these common rules:
- A press that was **NOT_SENT** is retried in order.
- An **UNKNOWN** press is not resent, because it could double a step.
- A failed release is retried every loop before any further tap.
- Mouse movement is removed **unless NOT_SENT**. For USB this is exactly 1e-4: it checks `ready()` first and removes on SENT or UNKNOWN. For BLE it is exactly r4 after review 1.
- The mouse button state is resent until SENT.

**Consequence: the 1e-4 send logic can stay as it is; only the three `hidOut` calls become backend calls returning SENT / NOT_SENT / UNKNOWN.**

### 2.4 Link readiness

| | USB 1e-4 | BLE-1 r4 |
|---|---|---|
| Ready | `USB` mounted and not suspended | connected **and** encrypted **and** keyboard CCCD subscribed |
| Link-down actions | `releaseHoldNow()`, queue cleared, mouse button released, pending movement dropped | the same set (`onLinkLost()`) |
| Link-up actions | all-up keyboard report, released mouse report | the same, plus **one repeated all-up after 100 ms** (no delivery confirmation) |

Both become one pair, `onLinkDown()` / `onLinkUp()`, driven by the active backend.

### 2.5 Libraries and build ✅ (probe, 2026-10-13)

- **Probe:** 1e-4 plus the BLE stack (`BLEDevice::init`, server, `BLEHIDDevice`, input/output reports, security, GAP listener, advertising, `notify`), linked but not run. It was built in the scratchpad, not in the repository.
  - **exit 0, flash 882,595 B (67%), RAM 65,828 B (20%)** of the default 1,310,720-byte app partition.
  - Warnings: only the long-known `KEY_TAB` / `KEY_BACKSPACE` redefinitions. **No new header conflicts.**
- **Expected integrated size:** about 0.90 MB (≈ 69%), adding the BLE backend, r4 diagnostics and the connection screen.
  - ❓ The Launcher's real app-slot size is still unverified (plan §6.6). BLE-1 r4 (870 KB) already ran under the Launcher, so a size near 0.9 MB is likely but not proven.
- **Runtime coexistence:** BLE-1 r4 ran NimBLE with TinyUSB active (CDC) on hardware. ✅ TinyUSB **HID** and NimBLE active at the same time is ❓.
- **Linking the BLE library:** `bleInUse()` is then true, so `initArduino()` no longer releases the BT controller memory (≈36 KB, `esp32-hal-bt.c` comment). This costs heap even in USB mode; it was harmless for BLE-1.
- **USB HID is always enumerated.** The USB HID devices must be constructed at static initialization (1e-4 comment: with CDC On Boot the core has already started USB before `setup()`). In BLE mode, a USB cable to a Mac therefore still shows a (silent) USB keyboard + mouse.

## 3. Architecture

```
U135 / Cardputer keys ─► profiles, HOLD, queue, CUSTOM SETUP, settings (from 1e-4, unchanged logic)
                              │  keyboard report (8 B)   mouse report (5 B)
                              ▼
                  HID transport interface (active backend only)
                    ready() · sendKeyboard() · sendMouse() → SENT / NOT_SENT / UNKNOWN
                    linkState() for the display
                     │                                   │
          USB backend (1e-4 hidOut, TinyUSB)      BLE backend (BLE-1 r4: NimBLE HOGP,
          ready = mounted && !suspended           security, adoption logic, CCCD, trace)
```

- **One active backend at a time** (`activeTransport`). There is no code path that sends to both, so a double send cannot happen by construction.
- **Interface:** plain functions with a `switch (activeTransport)`, not virtual classes (Arduino prototype rules, minimal change).
- **Renaming:** `usbReady` becomes `hidReady` (the active backend is READY). The 8 call sites keep their logic.
- **The BLE backend carries over BLE-1 r4 verbatim:**
  - `adoptConnHandle()`, `appConnect()`, `appDisconnect()`, `appConnectFailed()`, `handleSubscribe()`;
  - the GAP listener and trace; `notifyReport()`;
  - the all-up repeat; the READY rule.
  - **Regression risk for the r4 event-order fix:** low if these functions are moved unchanged. The r4 host tests (70) are reused against the new file.

## 4. Transport selection

### 4.1 Behavior matrix

| Mode | USB cable to a computer | BLE | Sends to |
|---|---|---|---|
| **USB** | connected | **off** (stack not started, or stopped advertising and disconnected) | USB only |
| USB | not connected | off | nothing; inputs counted as SKIP; display `USB NOT MOUNTED` (as 1e-4) |
| **BLE** | connected (charging) | advertising / connected | BLE only. The USB HID interface **is enumerated** but never sends (except a pending release, §4.3); display **`USB HID INACTIVE`** |
| BLE | not connected | advertising / connected | BLE only |

### 4.2 Switching **[decision D1]**

**Option A (recommended): switch at runtime, no restart.**

| Item | Behavior |
|---|---|
| BLE stack start | **Lazily**: at boot only if the saved mode is BLE, otherwise at the first switch to BLE. **In pure USB use the radio never starts**, so there is no PHY calibration write, no bond store activity, no RF; this is the same as 1e-4 |
| BLE → USB | The BLE backend sends all-up, then terminates the link (`ble_gap_terminate`, no bond change) and stops advertising. The stack stays initialized until the next power cycle |
| USB → BLE | The USB backend sends all-up, then becomes inactive; BLE starts or resumes advertising; the bonded Mac reconnects (as in B6/B7) |
| Pros | No USB re-enumeration, no dependence on Launcher reboot behavior, a few seconds faster |
| Cons | More state transitions to test |

**Option B: save the mode and restart (`ESP.restart()`).**
- **Pros:** the simplest state, since each boot initializes one transport only.
- **Cons:**
  - Every switch re-enumerates USB, which touches the open USB replug issue (P1).
  - It depends on the boot partition staying on MediaJog after a software restart. A power cycle does keep it (B6), but `ESP.restart()` under the Launcher is unverified.

### 4.3 Switch procedure (Option A) — key and button safety (revised after review)

1. Open the CONNECTION screen (§6). Entry performs the same release as CUSTOM SETUP entry: `releaseHoldNow()`, queue cleared, mouse button released, all-up requested on the current backend.
2. **Apply a different mode.** The switch runs in `loop()` as a short state machine and **never blocks**.
   - **0. Is a release needed?** Only if the old transport has a **host link** when the switch starts: USB **mounted** (also when suspended), BLE **connected**. Without a link, no host can hold a key from this device, and a later link starts all-up anyway (fresh enumeration / BLE link-up). The switch then commits at once, without a warning. *(Added during implementation: otherwise every USB→BLE switch on battery would show a false warning.)*
   - **a. RELEASING, at most 200 ms (`SWITCH_RELEASE_MS`):** each loop, if the old backend is READY, send the keyboard all-up report and a released mouse report on it. The mouse report goes only if BLE's mouse CCCD is on. Success depends on the transport:
     - **USB:** both reports SENT (IN transfer completed).
     - **BLE:** both reports accepted (`SUCCESS_NOTIFY`), then a further **100 ms** (`BLE_RELEASE_FLUSH_MS`, inside the same 200 ms budget) so that the stack can transmit them before the link is terminated.
   - **b. COMMIT** when the release succeeded (BLE: after the flush) **or** 200 ms have passed without success, whichever comes first. A BLE release accepted near the end of the budget still gets its 100 ms flush, so the hard bound is 300 ms:
     - `onLinkDown()`: queue cleared, HOLD cleared, mouse state cleared, pending movement dropped.
     - The old backend is deactivated: BLE terminates the link (`ble_gap_terminate`, no bond change) and stops advertising; USB just stops being used.
     - `activeTransport` is set and the `link` record is saved (§7).
     - The screen closes. **The switch is never delayed beyond 200 ms by a failed release.**
   - **c. If the release was not confirmed** (NOT_SENT, USB UNKNOWN, or BLE not accepted within the budget):
     - `releaseOwed[old]` and a **warning** are set. The status screen and the CONNECTION screen show `! OLD <USB|BLE> RELEASE UNCONFIRMED` in red; the last-event line shows it once.
     - The warning does not block anything.
   - **d. Priority release when the old transport is READY again:**
     - **USB inactive (BLE mode):** whenever USB HID becomes READY (mounted, not suspended) and `releaseOwed[USB]` is set, the USB backend sends one keyboard all-up and one released mouse report. This is the only USB traffic in BLE mode. Once both are SENT, the flag and the warning clear.
     - **BLE inactive (USB mode):** BLE is stopped, so it cannot become READY by itself. When BLE is selected again, its READY transition sends all-up first (as always). That success clears the flag and the warning.
     - **If the owed transport becomes the active one again**, its first successful all-up release clears the flag.
   - **e. The new backend** becomes READY by its own rules. `onLinkUp()` sends all-up first (BLE: plus the 100 ms repeat).
3. **No carry-over:** the queue is empty, HOLD needs a new physical press (`holdArmed` false), the mouse button needs a new press, and pending movement is dropped. Nothing pressed on the old transport is ever sent on the new one.
4. **Limit stated plainly:**
   - The firmware can only send release reports **while a link exists**.
   - After a BLE link is terminated, or a USB host is unplugged or suspended, the **host's** key state is the host's: macOS normally releases the inputs of a removed HID device, but this is ❓ not verified and **cannot be guaranteed by the firmware**.
   - The warning tells the user when a release was not confirmed.

### 4.4 Abnormal cases

| Case | Handling |
|---|---|
| HOLD Shift active, BLE link lost | `onLinkDown()` → `releaseHoldNow()` (as 1e-3 USB-not-ready); on reconnect all-up first, no resume |
| Drag (Spinner) active, BLE link lost | Mouse button released internally; `mouseSyncNeeded` on link-up. ❓ Mac-side release on HID disconnect is unverified |
| HOLD during transport switch | Impossible: the CONNECTION screen releases on entry (§4.3) |
| USB plugged in while in BLE mode | No effect on HID; display only |
| USB suspended (Mac sleep) in USB mode | 1e-4 behavior unchanged |
| Mac Bluetooth OFF / power-off of the Cardputer | r4 behavior unchanged; nothing persisted about pressed state |
| Inputs while not READY | Dropped and counted (`SKIP`), never queued: both 1e-4 and r4 already do this |

## 5. Spinner and drag over BLE

- **Same code path:** `onRotation()` (Profile 2) accumulates `mousePendingDx`, and `sendMouse()` sends up to `MAX_MOUSE_REPORTS_PER_LOOP` (4) chunks of ±127 per 20 ms loop. The drag button is the state-based `mouseWanted` / `mouseHost` / `mouseSyncNeeded`. Only the final send call changes.
- **Throughput ❓:**
  - BLE notifications are limited by the connection interval negotiated by the Mac (r3 trace: `UPD ok` at 2.7 s; value not recorded) and by NimBLE buffers.
  - A notification the stack does not accept is NOT_SENT: movement is kept and retried, and clipping at `MOUSE_PENDING_MAX` is counted as before.
  - **Plan:** start with the USB values, log `mouseClipped` / `FAIL` and the loop time on hardware, and add a per-backend report limit only if the measurements require it **[decision D8]**.
- **Drag button:** sent as a state, resent until SENT. On BLE link loss it is released internally, and on link-up a released state is sent first.

## 6. UI

| Item | Proposal |
|---|---|
| **Key** **[decision D4]** | **`Fn+B`** opens a **CONNECTION** screen in every profile. Not used so far: `Fn+1…4`, `Fn+;`, `Fn+.`, `Fn+S` (P4) are taken; `Fn+C/S/A/G/H` act only inside CUSTOM SETUP |
| CONNECTION screen | Shows the current mode, USB state (`USB HID READY` / `SUSPENDED` / `NOT MOUNTED` in USB mode; **`USB HID INACTIVE`** plus mounted/not mounted in BLE mode), BLE state (`BLE OFF` in USB mode; advertising / pairing / subscribing / READY, bonded, handle), and any release warning. Keys: `1` = USB, `2` = BLE, ok (Enter) = apply, esc (`` ` ``) = cancel, `t` = BLE trace pages (r3/r4 diagnostics). It is "leak-proof" like CUSTOM SETUP: no HID output while open |
| Status line 1 (all profile screens) | `USB HID READY` / `USB SUSPENDED` / `USB NOT MOUNTED` in USB mode; `BLE READY` / `BLE ADVERTISING` / `BLE PAIRING` / `BLE SUBSCRIBING` in BLE mode |
| Mode marker | `USB` or `BLE` next to the build tag, so the mode is visible on every screen, including the Spinner and CUSTOM SETUP screens |
| Build tag | Top-right, right-aligned by measured width (r3/r4 method) **[decision D3]** |
| Unchanged | CUSTOM SETUP and its key names; letters are shown as uppercase key names; the case handling is not changed. The README note in §10 |

## 7. NVS

| Item | Proposal |
|---|---|
| Settings record `mediajog`/`cfg` | **Unchanged, version 1**, same validation and write policy as 1e-4: profile at once after a confirmed switch, with read-back; CUSTOM on Save; sensitivity 2 s after the last step. 1e-4 and Stage 2a can read each other's record |
| **Transport mode** | A **separate key** `mediajog`/`link`: magic, version 1, mode byte (0 = USB, 1 = BLE), CRC-32. Written **at once after a confirmed switch**, with read-back, through the same guarded `nvBackendWrite()` (free-space check, no erase). Default when absent or invalid: **USB** **[decision D2]**. Kept out of `cfg` so the 1e-4 record stays byte-compatible and a downgrade to 1e-4 never becomes read-only |
| BLE bonds | Stored by NimBLE (`nimble_bond`), unchanged from BLE-1. No deletion by the firmware. A "forget pairing" function is out of scope **[decision D7]** |
| PHY calibration | Written by the radio driver only when BLE starts; with lazy start, USB-only use adds nothing |
| Forbidden | No `nvs_flash_erase`, no `nvs_erase_*`, no bond deletion, no partition change. The core's boot-time erase path (`STAGE_1E_4.md` §6.3) remains a known, pre-existing risk |

## 8. Risks

| Risk | Mitigation |
|---|---|
| r4 event-order fix lost while moving code | Move r4 functions unchanged; reuse all 70 host tests; keep the `aADP` / `aCON keep` trace |
| USB regression | Only the three send calls and `updateUsbState()` change, plus the `usbReady` rename. Re-run the 1e-4 hardware checks in USB mode |
| TinyUSB HID + NimBLE active together ❓ | In Option A they are only active together during a switch; measure loop time and failures |
| BLE mouse throughput ❓ | §5 measurements |
| Library's Security Request at every CONNECT (30 s `ENC !D`) | Harmless in r4 (`BLE_1.md` §13-3); unchanged **[decision D6]** |
| Flash size vs Launcher slot ❓ | Report the size; the Launcher slot is still unverified |
| Two keyboards visible on a Mac (USB silent + BLE) when cabled in BLE mode | Documented; only one sends |

## 9. Tests (after implementation)

1. **Host tests:**
   - the 47 NVS tests (1e-4), the 1e-3 HOLD tests and the 70 BLE tests, run against the integrated source;
   - new tests for the transport interface (SENT / NOT_SENT / UNKNOWN mapping), the switch procedure (all-up on the old backend first, nothing carried over), and the `link` record (validation, defaults, write policy).
2. **Hardware, USB mode:** the 1e-4 regression (four profiles, drag, CUSTOM, HOLD, NVS restore, Launcher Wi-Fi, SOUND TERRARIUM).
3. **Hardware, BLE mode:**
   - the BLE-1 r4 acceptance (start, Mac Bluetooth OFF→ON ×3, restart);
   - plus all four profiles: YouTube keys, Spinner movement and drag, Video Editing, CUSTOM with modifiers and HOLD Shift + Right.
4. **Switching:** USB↔BLE several times; HOLD and drag active before opening the CONNECTION screen; the mode restored after a power cycle; no key or button stuck on either host.

## 10. Documentation after implementation

- README (Japanese section) will include verbatim: 「SETUP画面のアルファベットはキー名として大文字で表示されます。実際に大文字を入力するには、Shift修飾キーを追加してください。」
- REPORT, DEVELOPMENT_PLAN and a new `STAGE_2A.md` will be updated.

## 11. Implementation steps (after approval)

1. Copy 1e-4 to `MediaJog_2a/`.
2. Introduce the transport interface with the USB backend only. The host tests and the hardware USB regression must be unchanged.
3. Add the BLE backend from r4 unchanged, plus lazy start.
4. Add `onLinkDown()` / `onLinkUp()` for both backends.
5. Add the CONNECTION screen, the switch procedure and the `link` record.
6. Add display changes.
7. Clean build, host tests, hashes, docs. Then Edgar's review, then hardware.

## 12. Decisions for Edgar and the author

| ID | Question | Proposal |
|---|---|---|
| D1 | Runtime switch or restart? | Runtime switch with lazy BLE start (§4.2 A) |
| D2 | Default mode without a saved mode | USB |
| D3 | Folder and tags | `MediaJog_2a/`, build id `2a`, screen tag `2a` (then `2a r2` → `2ar2` …) |
| D4 | Key for the CONNECTION screen | `Fn+B` |
| D5 | Keep the r3/r4 trace in the release | Yes, reachable only from the CONNECTION screen (`t`) |
| D6 | Library Security Request at CONNECT | Unchanged in 2a |
| D7 | Bond deletion / "forget pairing" | Out of scope for 2a |
| D8 | BLE mouse report limit per loop | Same as USB first; change only after measurement |
