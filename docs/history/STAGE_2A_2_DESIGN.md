# Stage 2a-2 — Design: BLE-B1's GATT table in Stage 2a (integration)

Created: 2026-10-14 · Design: Claude Opus 5.5 · Decision basis: Edgar (BLE-B1 hardware PASS; integrate into Stage 2a; design and risk analysis first)
**Status: design only. No code, no build.**
- `MediaJog_2a/`, `MediaJog_BLE_B1/`, `MediaJog_BLE_1_r4/`, `MediaJog_1e_4/` and the BLE-M1 builds stay unchanged.
- The integration is a new folder.

**Proposed name:** `MediaJog_2a_2/`, build id `2a-2`, screen tags `USB 2a2` / `BLE 2a2`. Edgar may choose another name.

## 1. What BLE-B1 proved on hardware (author, 2026-10-14)

- One HID service with three Report characteristics, all registered.
- New pairing with the Mac; `ki c1`, `mi c1`.
- Pointer movement and drag.
- Mac Bluetooth OFF → ON 3/3.
- Restart with automatic reconnection and both CCCDs restored.

## 2. Differences between Stage 2a and BLE-B1 (function comparison)

- Stage 2a = BLE-1 r4 BLE code + USB 1e-4 + the 2a integration. BLE-B1 = BLE-1 r4 BLE code + the static GATT table + test-only additions.
- **Identical in both (r4 code):** `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`, `ServerCallbacks`, `SecurityCallbacks`, `onGapEvent` (trace listener), `countCccd`, the trace buffer functions.
- **In 2a only (keep as they are):**
  - USB backend (`usbSend*`, `onUsbEvent`, `updateUsbState`);
  - the transport layer (`hidSend*`, `transportReady`, `onLinkDown` / `onLinkUp`, `bleStart` / `bleStop`, the switch state machine, `releaseOwed[]`, `noteReleaseSent`, `serviceOwedUsbRelease`);
  - profiles, CUSTOM, HOLD, SETUP and CONNECTION screens;
  - NVS settings and link record (`mediajog/*`);
  - the 2a versions of `sendKeyReport`, `sendRelease`, `tapKey`, `sendMouse`, `onLinkLost`, `onLinkReady`, `formatTraceLine`, `drawStatus`.
- **In B1 only:**
  - **to port:** `GattValue` and constants, the table (`DIS_CHRS`, `HID_CHRS`, `BAS_CHRS`, `GATT_SERVICES`, descriptor arrays, `B1_CHR` macros), `gattConstAccess`, `gattValueAccess`, `setGattValue`, `registerGattTable`, `onSubscribeEvent`, the B1 `notifyReport`, the B1 part of `bleBegin`;
  - **not to port** (test features): `m` (MOUSE mode toggle), `g` (Service Changed), `pollPageKey`, the B1 status line. 2a has its own profiles with mouse actions and its own UI keys.
- **Report map:** 2a's `HID_REPORT_MAP` is byte-identical to r4's, which B1 serves (checked). It is used only by BLE; USB uses the TinyUSB descriptors of `USBHIDKeyboard` / `USBHIDMouse`.

## 3. Change scope (port from B1 into a copy of 2a)

| # | Where (2a) | Change | Source |
|---|---|---|---|
| 1 | includes | Add `<host/ble_gatt.h>`, `<host/ble_hs_mbuf.h>`; `<BLEHIDDevice.h>` no longer needed | B1 |
| 2 | BLE objects | `BLEHIDDevice* bleHid` and the three `BLECharacteristic*` → `GattValue` objects with the same names `kbInput` / `kbOutput` / `mouseInput` (pointers). `bleSendKeyboard` / `bleSendMouse` then compile unchanged | B1, verbatim |
| 3 | `ReportCallbacks` | Removed; its jobs: subscribe → `onSubscribeEvent`, status → `notifyReport`, LED write → `gattValueAccess` | B1 |
| 4 | Static table + access callbacks + `registerGattTable` + `onSubscribeEvent` | Inserted **verbatim** (byte-identical block, checked by script) | B1 |
| 5 | `notifyReport` | B1 version verbatim (`ble_gatts_notify_custom()`, CCCD check, `NOTIFY_*` status, rc). Contract for 2a unchanged: true ⇔ `TX_SENT`, false ⇔ `TX_NOT_SENT` (BLE never "unknown") | B1 |
| 6 | `bleBegin` | B1's body: subscribe listener **before** the trace listener; security and server as r4; `registerGattTable()` after `createServer()`; one `bleServer->start()`; advertising UUID 0x1812, appearance and intervals as r4; handles printed. `bleStart()` still calls it once (`bleStarted`) | B1 |
| 7 | Diagnostics | CONNECTION / diagnostics page: add the live `ki / ko / mi` handles. Registration rc ≠ 0 → visible error, BLE never READY | new, small |
| 8 | Tags | `BUILD_TAG "2a-2"`, screen `USB 2a2` / `BLE 2a2` | new |

**Not changed:**
- USB code, transport and switch logic, `sendMouse` / `sendKeyReport` (they call `hidSend*` → `bleSend*` → `notifyReport`), profiles, CUSTOM, HOLD, SETUP, CONNECTION logic;
- NVS settings and link record, including the link record format (`MJLK` v1).
- The 2a trace formatter stays: its labels use `kbValueHandle` / `mouseValueHandle`, set from the table handles as in B1.

## 4. Impact analysis

| Area | Impact | Risk and handling |
|---|---|---|
| USB HID | None in code. USB descriptors, `usbSend*` and USB state are untouched | Regression run on hardware (§7, H1) |
| BLE HID keyboard | Same report bytes; sent with `ble_gatts_notify_custom()` instead of `BLECharacteristic::notify()`. Same acceptance meaning (host accepted) | B1 hardware PASS; 2a host tests re-run on the new stubs |
| **BLE mouse** | **Works for the first time inside 2a** (2a hardware: `ms=0`). Paths that only ran in host tests now run on hardware: profile mouse movement and drag on BLE; `switchMouseNeeded(BLE)` = true (the switch now also waits for the mouse button-up on BLE); `noteReleaseSent()` on BLE needs the mouse half (`msDone` no longer short-cut by `!mouseSubscribed`); the owed release with `!` | **Main functional risk.** Hardware tests H3 (switch with the knob held during drag, both directions; 300 ms bound; `!` behavior). Host tests already cover the logic with ms=1 (2a e2e) and get re-run |
| Transport switch / lazy start | `bleBegin()` still runs once, at the first BLE start (boot in BLE, or the first switch to BLE). `bleStop()` stops advertising and terminates; `bleStart()` re-advertises. No GATT re-registration, so the table is registered exactly once per boot | NimBLE accepts `add_svcs` only while no advertising or connection exists; true at the first `bleBegin()`. Host test: repeated BLE ↔ USB switching gives one `count_cfg`, one `add_svcs`, one server start |
| Profiles / CUSTOM / HOLD | None in code. HOLD over BLE now also holds a real mouse button in drag profiles; release rules unchanged | Re-run HOLD tests (16) and the HOLD scenarios of the 2a e2e tests |
| NVS (app) | None: no new key, same `mediajog/settings` and `mediajog/link` records, same write policy | Re-run NVS tests (47) unchanged |
| **NimBLE bond store (NVS `nimble_bond`)** | Per bonded Mac, NimBLE now stores **3 CCCD records** (keyboard, mouse, battery) instead of 2. Limits in this core: `MAX_BONDS` 3, **`MAX_CCCDS` 8**. With 3 hosts × 3 = 9 > 8, the library's store-full handler (`ble_store_util_status_rr`) **deletes the oldest other peer's bond** when the 9th record is written | **Risk against "do not delete bonds".** Not reachable with one Mac (3 records) or two hosts (6). Options for Edgar: (a) keep the B1 table byte-identical (hardware-verified; the Mac's B1 bond keeps working, no re-pairing) and document the limit "at most 2 BLE hosts"; or (b) remove NOTIFY from Battery Level (2 records per host, 3 hosts = 6), which changes the table → one more Mac re-pairing and a new hardware check. **Recommendation: (a) now, (b) only if more than 2 hosts are needed** |
| Mac pairing | If the table and everything registered before it (GAP / GATT services from `createServer()`, same core) are identical to B1's, **all handles are the same as in B1**, and the Mac's current bond (made with B1) and its CCCD records fit | No re-pairing expected. **Do not reinstall the old Stage 2a / r4 / M1 builds afterwards**: their tables differ, and the Mac would again see a changed table |
| Flash / RAM | 2a was 896 KB; the table is small and `BLEHIDDevice` objects go away | Check at build |

## 5. Keeping the B7 fix and the registration order

1. **B7 functions byte-identical:** `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`, `ServerCallbacks`, `onGapEvent`. Checked by the function-comparison script against BLE-1 r4, 2a and B1 (all three already identical).
2. **Subscribe path** exactly as B1 (hardware-verified):
   - a listener registered **before** the trace listener (NimBLE calls the newest first);
   - the library's `conn_find` check;
   - value bit 0 = `cur_notify`;
   - the same `handleSubscribe()` in the NimBLE host task.
3. **Registration order** (the handles depend on it):
   - `BLEDevice::init()` → listeners → security → `createServer()`, which resets GATT and adds GAP / GATT → `registerGattTable()`, which queues DIS, HID, BAS → `bleServer->start()`, once → advertising.
   - **Rules:**
     - no library `createService()` anywhere, which would also arm `resetGATT()`;
     - no second `bleBegin()`, and no `BLEDevice::deinit()`;
     - the table block is copied byte for byte.
   - A script compares the table block (from `struct GattValue` to `onSubscribeEvent`) with B1's, and the build fails the review if it differs.
4. **READY rule unchanged:** connected + encrypted + keyboard CCCD. The mouse is sent only with its CCCD (`bleSendMouse` and `notifyReport` both check).

## 6. Host test plan (before any hardware)

| Group | Content |
|---|---|
| Stubs | B1's NimBLE C API stubs (queue, single start, CCCD after the value, listeners newest first, `notify_custom` consumes the mbuf) + 2a's USB / NVS / UI stubs |
| Table identity | Script: the table block of 2a-2 equals B1's byte for byte. In the stub, all handles equal those B1 gets |
| 2a end-to-end (85 checks) | All re-run, now on the corrected BLE model (the old 2a stub gave each report its own handle, which hid the duplicate drop); expectations for BLE mouse stay as designed |
| B1 scenarios | Table layout, B7 order with both CCCDs restored before CONNECT, first pairing, FGN, trace order, `conn_find` check, host error with retry, mbufs, host writes (LED) |
| New for 2a-2 | (1) Lazy start: boot in USB, switch to BLE → table registered once; BLE → USB → BLE several times → still one `count_cfg` / `add_svcs` / server start. (2) Switch BLE → USB with the drag button held and ms=1: the mouse button-up is sent on BLE before the switch completes; with a host error, `!` and owed release; completion only after keyboard all-up **and** mouse button-up on BLE. (3) Owed BLE release done after reconnection with RESTORE first. (4) Profile mouse movement and drag over BLE. (5) HOLD over BLE (keyboard and mouse button) released on link loss and on switch. (6) Registration rc ≠ 0: error shown, never READY, USB unaffected |
| NVS (47) and HOLD (16) | Re-run unchanged |
| Regression | BLE-1 r4 and 2a still reproduce mouse 0xFFFF on the C++ model |
| Mutants | Table queued after the start; registration repeated on each `bleStart()`; listener order swapped; no `conn_find`; mouse CCCD sets the keyboard flag; `switchMouseNeeded()` ignoring the BLE mouse; `noteReleaseSent()` ignoring the mouse half |
| Build | Clean build, 0 sketch warnings, `#error` guards, SHA-256 of source and app-only binary |

## 7. Hardware test plan (after review; the Mac keeps its B1 bond)

**Rules:**
- No NVS erase and no bond deletion.
- No Mac re-pairing unless the Mac does not subscribe, and then only with approval.
- Record the SHA-256 of the installed binary.

| # | Test | Pass |
|---|---|---|
| H0 | Install, start in BLE (the last transport stored by 2a; else switch with `Fn+B`) | Tag `BLE 2a2`; `ki / ko / mi` handles **equal to B1's values** (please note B1's numbers); Mac reconnects with the existing bond, `kb=1 ms=1`, READY |
| H1 | USB regression: Profiles 1–3, HOLD, CUSTOM, settings change and restart (persistence), USB unplug / replug | As 1e-4 / 2a |
| H2 | BLE: keyboard profiles; mouse profile movement and drag | Pointer moves; drag works; keys work |
| H3 | Switching without restart: BLE ↔ USB (5× each way); **while dragging** (knob held) both ways; then check that no button or key is stuck on either host; `!` only when a release could not be sent | No stuck button or key; switch within the bound; `!` clears after the owed release |
| H4 | B7 regression in BLE: Mac Bluetooth OFF → ON ×3, Cardputer restart ×2, and boot in USB then switch to BLE (lazy start) | READY with `kb=1 ms=1` each time (RESTORE path) |
| H5 | Transport restore after restart (link record) both ways; NVS statistics stable (no writes without a change) | As 2a |
| H6 | Not testable with one Mac: more than 2 bonded hosts (CCCD limit, §4) | Documented limitation |

## 8. Open decisions for Edgar

1. The folder and tag names (`MediaJog_2a_2`, `2a-2`).
2. The CCCD limit (§4): (a) keep the B1 table, limit to 2 BLE hosts (recommended), or (b) remove battery NOTIFY, with re-pairing.
3. Whether the PnP ID byte order (kept as r4/B1) and other B1 deviations (`BLE_B1_DESIGN.md` §6) stay as they are in 2a-2. Recommendation: keep them, so that the table stays identical to the verified B1.

## 9. Edgar's decisions and implementation (2026-10-14)

- **Decisions** (recorded in `STAGE_2A_2.md` §1):
  - `MediaJog_2a_2`, `2a-2`, `USB 2a2` / `BLE 2a2`;
  - CCCD option (a): at most 2 BLE hosts, known limitation recorded;
  - B1 permissions, PnP order and read-only descriptors kept;
  - B7, registration order, Report Map and CCCD handling identical to B1;
  - existing builds unchanged;
  - the Mac's B1 bond reused, no re-pairing in the initial test.
- **Implemented as designed in §3**, with one adjustment to §3 #7: the CONNECTION screen has no free line, so the two help lines were merged into one. The bottom line now shows `t:trace knob:select ki… ko… mi…`, or `GATT REG rc<n>` in red. Results are in `STAGE_2A_2.md`.
