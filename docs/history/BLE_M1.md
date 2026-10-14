# BLE-M1 — Feasibility test: BLE mouse in a second HID service

Created: 2026-10-13 · Implementation: Claude Opus 5.5 · Decision basis: Edgar (proposal A first, as a separate small test build)
**Status: built and host-tested. NOT installed on hardware.** Test firmware only. Stage 2a, BLE-1 r4 and USB 1e-4 are unchanged; their `SHA256SUMS.txt` files still verify.

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

## 1. Why this test exists

In the Stage 2a hardware test, the BLE mouse did not work: CONNECTION showed `bond=1 kb=1 ms=0`, and `rep` stayed at 0 (`STAGE_2A.md` §10).

**Cause (source-checked, m5stack:esp32 3.3.9, NimBLE; also unchanged in upstream master):**
- `BLEService::addCharacteristic()` does not add a characteristic whose UUID already exists in the same service.
- `BLEService::start()` builds the GATT table from that map.
- `BLEHIDDevice` creates every Report characteristic with the same UUID 0x2A4D: keyboard input, then keyboard output, then mouse input. Only the keyboard input reaches GATT; the other two keep handle `NULL_HANDLE` (0xFFFF).
- So the Mac cannot subscribe to a mouse report: there is none.
- BLE-1 r4 has the same construction. Its hardware tests used the keyboard only.

**Proposal A (Edgar: first candidate):** put the mouse into its own HID service, where no UUID repeats.

**Question this build answers:** does macOS use a second HID service, so that keyboard and mouse both work?

## 2. What the test build does (`MediaJog_BLE_M1/`, from BLE-1 r4)

| Item | Implementation |
|---|---|
| Keyboard | `BLEHIDDevice` exactly as BLE-1 r4. Only its Report Map changed: keyboard only (`KB_REPORT_MAP`, the unchanged keyboard bytes) |
| Mouse | `createMouseService()`: `bleServer->createService(0x1812, 20, inst_id 1)`, with HID Information (0x2A4A: bcdHID 1.11, country 0, flags 0x01), Report Map (0x2A4B: `MOUSE_REPORT_MAP` = the unchanged mouse bytes, Report ID 2), HID Control Point (0x2A4C, write without response), and Report (0x2A4D: read + notify, encrypted access, Report Reference 0x2908 = {2, input}; NimBLE adds the CCCD). Started before the first advertising starts the GATT server |
| Library facts used ✅ | `createService()` accepts a second 0x1812 (warning only); the service map is keyed by service pointer, so the first HID service is kept; `start()` registers each service with `ble_gatts_add_svcs()`; `BLEServer::start()` puts every notify characteristic of every service into its subscribe dispatch, so the mouse report's subscribe events reach BLE-1's `ReportCallbacks` → `handleSubscribe()` |
| Knob | Cardputer key **`m`** toggles **KEYS** (Right / Left / Space, as BLE-1) and **MOUSE** (X movement, 8 counts per detent; knob held = left button / drag). Leaving MOUSE mode releases the button |
| Re-discovery | Cardputer key **`g`** sends a GATT **Service Changed** indication (`ble_svc_gatt_changed(0x0001, 0xFFFF)`) while connected, so a bonded Mac can re-read the changed service table. No bond or NVS data is changed |
| Diagnostics | Status page line `kb h<handle> c<CCCD>  ms h<handle> c<CCCD>  mr<mouse reports> g<indications>` (hex handles; `FFFF` = not in GATT). Trace (`t`) subscribe lines show the attribute handle of unknown characteristics (`SUB x51 R1`) |
| Tags | Screen `BM1` (top right), build id `BLE-M1`; device name unchanged (`MediaJog BLE`) |

**Unchanged from BLE-1 r4 (byte comparison, 47 functions):**
- the B7 handling: `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`;
- the server and report callback classes, `updateLinkState`, `onLinkLost`, `onLinkReady`, `notifyReport`, `sendMouse`;
- the GAP trace, security settings and NVS use (read-only statistics).

**Changed:** `bleBegin` (keyboard-only map, the mouse lines moved into `createMouseService`), `onRotation` / `onButton` (MOUSE mode), `pollPageKey` (`m`, `g`), `drawStatus`, `formatTraceLine`.

## 3. Host tests ✅ (logic only, not hardware)

- **The stubs model the library's duplicate-UUID drop:** per service, a repeated UUID is not registered and keeps handle 0xFFFF; handles are assigned when a service starts; a client cannot subscribe to an unregistered characteristic.
- **Stub validated against the hardware observation:** with this model, **BLE-1 r4 and Stage 2a** (their own preprocessed sources) give `kb handle 0xF registered`, `mouse handle 0xFFFF not registered`, 2 dropped duplicates. This is exactly `ms=0`.
- **BLE-M1 end to end (preprocessed sketch, fresh process per scenario): 33/33.**

| Scenario | Checks |
|---|---|
| M01 | Two HID services, both started; keyboard report in service 1, **mouse report in service 2 with its own handle**; service 2 drops nothing; Report Reference {2, input}; notify; service 1 map = keyboard map, service 2 map = mouse map; keyboard output still dropped (as r4) |
| M02 | **B7 order** (encryption and RESTORE of both CCCDs before CONNECT): adopted, kb=1, **ms=1**, READY; keyboard all-up and mouse released first; KEYS mode Right on the keyboard report |
| M03 | MOUSE mode: CW → X +8 on the mouse report; knob held → button down, movement with the button held (drag); release → button up; `m` back to KEYS |
| M04 | Leaving MOUSE mode with the knob held sends button up |
| M05 | Mouse CCCD absent: READY with ms=0, status shows the mouse handle and `c0`, nothing sent in MOUSE mode, KEYS still works |
| M06 | `g`: no indication without a link; one indication with a link; bonds untouched |
| M07 | Disconnect during drag: button released internally; reconnect (RESTORE first): ms=1, first mouse report released; the old knob release sends no button-down |
| M08 | Keyboard RESTORE before CONNECT adopted (r4); mouse subscribe on another handle is FGN; trace shows `x51 R1` / `m R1`; no subscribe to an unregistered characteristic |

**Mutation check:** with the mouse put back into the keyboard's HID service (`bleHid->hidService()`), 8 checks fail, including the B7 order and the GATT layout. The tests depend on the separate service.

## 4. Build (2026-10-13)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, 0 sketch warnings; `#error` guards effective (hwcdc, CDC off). Flash 871,427 B (66 %), RAM 68,708 B (20 %) |
| Source `MediaJog_BLE_M1/MediaJog_BLE_M1.ino` | 73,146 B, SHA-256 `7e98f370c742a8e99792c147dadf0f31e1443df612aed9d3130ff07baacbf13b` |
| App-only binary `MediaJog_BLE_M1/build/MediaJog_BLE_M1.ino.bin` | 871,568 B, SHA-256 `90bc4e5510af1f36c152e7b5b482b3b0d0365457df5cf69b7a68d274cc1c0861` |
| Host-tested source | Equals the final preprocessed source |

## 5. Hardware procedure (proposed; not run)

**Rules:** do not delete bonds, NVS or Launcher settings. Steps 1–5 need no Mac-side "Forget". Step 6 needs Edgar's and the author's approval first.

Install `MediaJog_BLE_M1.ino.bin` (app-only) via the Launcher Wi-Fi upload and **record its SHA-256** (`90bc4e55…0c0861`). A Serial log (USB CDC, 115200) is optional and useful: it prints `[BLE] advertising; kb handle …, mouse handle …` and every subscribe event with its attribute handle.

| # | Do | Look for |
|---|---|---|
| 1 | Start the app | Tag `BM1`. Status line `kb h… c? ms h… c?`: **both handles are not FFFF** (the GATT layout on the device) |
| 2 | Let the Mac reconnect with the existing bond (as with BLE-1 / 2a) | `BLE READY`; **`kb … c1`**; note `ms … c?`. Trace (`t`): which `SUB` lines appear (`k R1`, `m R1`, `x… R…`) |
| 3 | If `ms c0`: press **`g`** (Service Changed), wait about 10 s; then, if needed, Mac Bluetooth OFF → ON | Does `ms` become `c1`? (`SUB m W1` = the Mac wrote the mouse CCCD) |
| 4 | With `ms c1`: press **`m`** (MOUSE), turn the knob in Finder / the Breakout game; hold the knob and turn (drag) | Pointer moves left/right; drag works; `mr` increases. Press `m` again: KEYS mode Right / Left / Space still work |
| 5 | B7 regression with the mouse: Mac Bluetooth OFF → ON (3×), Cardputer restart | Back to `BLE READY` with **`kb c1` and `ms c1`** each time; keyboard and mouse both work afterwards |
| 6 | **Only with approval:** if `ms` stays `c0` after steps 3 and 2 restarts | Mac: "Forget This Device", then pair again. Note: on re-pairing, the BLE library replaces the old bond **of this Mac only** (`ble_store_util_delete_peer`, BLE-1 §2) |

**Outcome reporting (to Edgar before any next step):**
- **A works:** `ms c1`, pointer and drag work, B7 still passes → integrate the second HID service into Stage 2a (a separate revision).
- **The Mac ignores the second HID service** (after steps 3 / 6 the mouse CCCD is never written, or the pointer does not move with `ms c1`) → report first; proposal B (one HID service registered with the NimBLE C API) is the next candidate.

## 6. Limitations

- The keyboard output report (LEDs) is still dropped in service 1, as in BLE-1 r4 and Stage 2a; keyboard input is unaffected.
- macOS behavior with two HID services and its GATT cache after the service-table change are ❓; step 3 / 6 address them.
- Mouse movement is fixed at 8 counts per detent (no sensitivity in this test build).
- Test firmware: no profiles, no USB HID, no settings storage.

## 7. Hardware result 1 and cause analysis (2026-10-14; no code change yet)

**Observed on hardware (author, checked by Edgar):** after start, the status line showed `kb h10 c0` and `ms hFFFF c0`. The keyboard report has a GATT handle; the mouse report does not. No Mac re-pairing, no bond or NVS change.

**Cause (source-checked, m5stack:esp32 3.3.9 BLE library + NimBLE `ble_gatts.c`): the mouse service is added after the GATT server was already started.**

1. `BLEHIDDevice::startServices()` (`BLEHIDDevice.cpp:118`) starts the three standard services **and then calls `m_server->start()`**. `BLEServer::start()` (`BLEServer.cpp:200`) calls `ble_gatts_start()` and sets `m_gattsStarted = true`.
2. `ble_gatts_start()` is the only place where NimBLE registers the queued service definitions and writes each characteristic's handle through `val_handle` (`= &chr->m_handle`, `BLEService.cpp:637`).
3. BLE-M1 calls `createMouseService()` **after** `startServices()` (`MediaJog_BLE_M1.ino:1226–1227`). Its `mouseHidService->start()` calls `ble_gatts_count_cfg()` and `ble_gatts_add_svcs()`; these only queue the definition (no advertising and no connection yet, so no `EBUSY`). Nothing calls `ble_gatts_start()` again: `BLEServer::start()` returns at once because `m_gattsStarted` is true, and `startAdvertising()` only starts the server when it is not started (`BLEAdvertising.cpp:1566`). The mouse report keeps `NULL_HANDLE` 0xFFFF.
4. Also, `m_notifyChrVec` (subscribe dispatch) was filled in step 1, without the mouse report.

**Answers to the questions:**

| # | Question | Finding |
|---|---|---|
| 1 | Does `createMouseService()` run? | Yes, as far as the source shows: the sketch runs without abort and the mouse characteristic object exists (`getHandle()` returns its initial 0xFFFF; a missing object would crash at `bleBegin`). It runs too late, not never |
| 2 | Is the second HID service in GATT? | **Not at boot.** Its definition is queued but not registered (see 3 above) |
| 3 | `start()` and handle timing | Handles are written only by `ble_gatts_start()`, which runs inside `bleHid->startServices()`, before `createMouseService()` |
| 4 | Does `ms hFFFF` mean "not in GATT"? | **At boot, yes.** But the screen shows a **snapshot**: `kbValueHandle` / `mouseValueHandle` are copied once in `bleBegin()` (`:1237–1238`), not read live. See the side effect below: after the first disconnect the real handle can change while the screen still shows FFFF |
| 5 | Stub vs library | **The stub was wrong.** The stub's `BLEService::start()` assigns handles immediately, and its `startServices()` does not start the server. The real library assigns all handles once, in `ble_gatts_start()`, called by `startServices()`. The stub modeled the duplicate-UUID drop correctly but not this timing, so the tests passed |

**Side effect in the current BLE-M1 (source analysis, ❓ not observed):**
- `createService()` after the server start sets `m_svcChanged` (`BLEServer::serviceChanged()`).
- On the next disconnect, when no connection remains, the library's disconnect handler calls `resetGATT()` (`BLEServer.cpp:842`). This resets NimBLE's GATT table, re-adds all services (in `m_serviceMap` order, which is pointer order) and starts the server again with the next advertising.
- From then on, the mouse report would really be in GATT, but the screen would still show `ms hFFFF` (stale snapshot), and the handles of all services may differ from the boot layout without the Mac being told.
- `resetGATT()` does not touch the bond store. But a GATT table that changes during a session is not a test condition we want.

**Minimal fix proposal (for Edgar's review; not implemented):**

1. **Register the mouse service before the GATT server starts, and after the keyboard's services, so that the keyboard handles stay as in BLE-1 r4 (`kb h10`).** In `bleBegin()`, replace `bleHid->startServices(); createMouseService();` with the same calls that `startServices()` makes, with the mouse service added before the server start:
   ```cpp
   bleHid->deviceInfo()->start();
   bleHid->hidService()->start();
   bleHid->batteryService()->start();
   createMouseService();   // creates the service and calls its start(): queued last
   bleServer->start();     // ble_gatts_start(): all handles assigned, notify list includes the mouse
   ```
   NimBLE registers services in the order they were queued, so the device info, keyboard HID and battery services keep their handles, and the mouse service comes after them. `createService()` before the server start does not set `m_svcChanged`, so no `resetGATT()` occurs at the first disconnect.
   - Not chosen: calling `createMouseService()` before `startServices()`. It would queue the mouse service first and move the keyboard's handles, which a bonded Mac has cached.
2. **Diagnostics:** read the handles live (`kbInput->getHandle()`, `mouseInput->getHandle()`) in the status line, so a later change cannot be hidden by the snapshot. Optionally also show the mouse service's start result.
3. **Host stub:** model the library timing: handles assigned only by a server start; the server start latches; a service started after it stays unregistered (and sets "service changed"). Required check: the current BLE-M1 source reproduces `ms 0xFFFF` on the corrected stub; the fixed source gives the mouse a handle and keeps the keyboard handle as with BLE-1 r4.
4. **Delivery:** a new folder (`MediaJog_BLE_M1_r2`, tag `BM1r2`), because BLE-M1 was installed. Stage 2a, BLE-1 r4 and 1e-4 unchanged.

**Notes for the next hardware run:**
- A one-time GATT change is still expected for the Mac: compared with BLE-1 r4 and 2a, a service is added at the end. The `g` key (Service Changed) covers this without re-pairing; Mac re-pairing stays excluded unless approved.
- Whether macOS uses a second HID service is still the open question of proposal A; this result does not answer it.

## 8. BLE-M1 r2 — mouse service registered before the GATT server start (2026-10-14)

**Status: built and host-tested. NOT installed on hardware.** Edgar approved the analysis and the minimal fix (§7). Stage 2a, BLE-1 r4, USB 1e-4 and BLE-M1 are unchanged (their `SHA256SUMS.txt` verify).

### 8.1 Library check: what `startServices()` does

`BLEHIDDevice::startServices()` (3.3.9, `BLEHIDDevice.cpp:118–123`) is exactly:

```cpp
m_deviceInfoService->start();
m_hidService->start();
m_batteryService->start();
m_server->start();
```

- `deviceInfo()`, `hidService()` and `batteryService()` return these same three members (`:326–342`).
- r2 makes the same four calls, in the same order, with the mouse service's `start()` added before the server start.
- All other initialization stays where it was: the services and their mandatory characteristics are created in the `BLEHIDDevice` constructor, and the protocol mode value is set there too.
- `BLEServer::start()` is the same function as before. It runs `ble_gatts_start()`, sets the device name again, looks up the service handles, builds `m_notifyChrVec` and sets `m_gattsStarted`.
- **Nothing is lost.**

Side note (harmless): `BLEServer::start()` looks up each service's own handle with `ble_gatts_find_svc(uuid)`. For the second 0x1812 service, this returns the first HID service's handle (rc 0, no abort). The library uses the service handle only for logs; characteristic handles are written separately.

### 8.2 Changes (BLE-M1 → r2; nothing else changed)

| Where | Change |
|---|---|
| `bleBegin()` | `bleHid->startServices(); createMouseService();` → device info, HID, battery `start()`, then `mouseSvcStarted = createMouseService()`, then `bleServer->start()` once |
| `createMouseService()` | Returns `mouseHidService->start()` (true = definition queued) |
| Status line | `kb h<handle> c<cccd> ms h<handle> c<cccd> s<mouse start> mr… g…`. Handles are read **live** with `getHandle()`; they are no longer copied once at boot. No handle value is assumed. |
| Serial | `[BLE] advertising; kb handle …, mouse handle …, mouse service start …, server started …` |
| Tags | Screen `BM1r2`, build id `BLE-M1r2` |

Unchanged:
- the keyboard (`BLEHIDDevice`, keyboard-only Report Map), the mouse Report Map and the mouse service contents;
- all B7 functions, the callback classes, MOUSE mode, and the `m` / `g` keys;
- the trace, security settings and NVS use.

### 8.3 Host tests ✅ (logic only, not hardware)

**Stub corrected to the library timing:**
- `BLEService::start()` only queues the service.
- `BLEServer::start()` runs once. It assigns the handles of the queued services in queue order and builds the subscribe dispatch list.
- A service created after the start sets "service changed".
- `startServices()` starts the server, as the library does.
- `startAdvertising()` starts the server only if it has not started yet.
- A subscribe event reaches a characteristic's callbacks only through the dispatch list.

**Stub validation with this model:**

| Source | kb | mouse | Server starts | Service changed |
|---|---|---|---|---|
| BLE-1 r4 | 0xF | **0xFFFF** | 1 | 0 |
| Stage 2a | 0xF | **0xFFFF** | 1 | 0 |
| BLE-M1 | 0xF | **0xFFFF** (as on hardware) | 1 | **1** |
| BLE-M1 r2 | 0xF | 0x1E | 1 | 0 |

The stub's handle numbers are not the device's numbers. The device starts with the GAP and GATT services, so the keyboard showed `h10` on hardware. Only equality and relative order are tested.

**r2 end to end: 43/43.** The run includes the BLE-M1 scenarios M01–M08 and the new R01:

| Scenario | Checks |
|---|---|
| R01 (new) | Server started once. Mouse service queued 4th, after device info, HID and battery, before the start. `s1`. Mouse handle valid. **Keyboard handle equal to BLE-1 r4's** on the same stubs. Mouse handles after the battery service. No "service changed" (no `resetGATT()`). Mouse report in the dispatch list. Status line reads live handles: a corrupted boot snapshot does not reach it |
| M02 | B7 order: kb=1, **ms=1**, READY |
| M03 / M04 | Mouse movement, drag and button release |
| M05 | Without the mouse CCCD, the keyboard is unaffected |
| M06 | `g` (Service Changed) |
| M07 | Disconnect during drag, reconnect with RESTORE first; **one server start and no "service changed" after the disconnect** |
| M08 | Keyboard B7 regression, FGN, trace |

**Reproduction and mutation checks (same stubs):**

| Source and suite | Result |
|---|---|
| BLE-M1 (unchanged) with its own suite | **8 failures**: mouse not registered, B7 order without ms=1, drag scenario aborts on an empty mouse report list, no subscribe possible. This reproduces `ms hFFFF` |
| r2 mutant: mouse service queued first | 3 failures: queue order, **keyboard handle moved**, mouse after battery |
| r2 mutant: BLE-M1 order (`startServices()` then mouse) | 12 failures, including the mouse handle, "service changed", the dispatch list and the B7 order |

The original BLE-M1 harness (old stubs, 33/33) is kept for reference.

### 8.4 Build (2026-10-14)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, 0 sketch warnings; `#error` guards effective (hwcdc, CDC off). Flash 871,591 B (66 %), RAM 68,708 B (20 %) |
| Source `MediaJog_BLE_M1_r2/MediaJog_BLE_M1_r2.ino` | 75,248 B, SHA-256 `315cc7cf3bb5f97fbe668426229fb64d750876c157b62b4488d00792ff7dd6ff` |
| App-only binary `MediaJog_BLE_M1_r2/build/MediaJog_BLE_M1_r2.ino.bin` | 871,744 B, SHA-256 `252d61419a715b053b886d69fd46ba0a96ee53c2e317951540e0ebb63cf8e47e`; contains `BLE-M1r2` / `BM1r2` |
| Host-tested source | The preprocessed final source |

### 8.5 Hardware procedure (after Edgar's review; not run)

Use the procedure of §5 with the `r2` binary.
- At step 1, check that the screen shows `BM1r2` and that the status line shows `kb h<x> … ms h<y> … s1` with both handles different from `FFFF`. Record the actual values; `kb` is expected to stay the same as on BLE-M1 (`h10`).
- After that, check the mouse subscription (`ms c1`, steps 2–3), MOUSE-mode movement and drag (step 4), and the B7 regression (step 5).
- Step 6 (Mac-side re-pairing) still needs approval.
- Keep all bonds, NVS and Launcher settings.
- For the Mac, the GATT table now has one more service than with BLE-1 r4 / 2a / BLE-M1. Its cache may need the `g` key (step 3).

## 9. BLE-M1 r2 hardware result 1: repeated connect / disconnect (2026-10-14; analysis only)

**Observed (author, checked by Edgar):**
- `ms h2D`: the mouse report is registered.
- The Mac connects and drops the link at once, repeatedly: `CONN ok`, `aCON new`, `LEST ok`, `ENC !7`, `DISC 213`, `aDIS cur 1`.
- No code change, build, NVS erase or bond deletion followed.

**Codes:** all trace codes are printed in **hex**: `"!%X"` and `"%X"` in `formatTraceLine()`, the full value since r3.

| Token | Value | Meaning (NimBLE, ESP-IDF 5.5) |
|---|---|---|
| `ENC !7` | 0x007 = `BLE_HS_ENOTCONN` | A security procedure was in progress and was aborted because the connection ended. In NimBLE, `ble_gap_conn_broken()` first calls `ble_sm_connection_broken()`, which reports the pending procedure through an ENC_CHANGE event with `BLE_HS_ENOTCONN`; only then is DISCONNECT posted. So `ENC !7` is a **consequence** of the disconnect, not a failed key check. The pending procedure is the Security Request that the library sends on every CONNECT (`BLEServer.cpp:814`) |
| `DISC 213` | **0x213**, not 213 decimal (0xD5 is not a NimBLE reason) = `BLE_HS_HCI_ERR(0x13)` | HCI 0x13 *Remote User Terminated Connection*: **the Mac closed the link.** It is not a timeout (0x208), not a local termination (0x216) and not an encryption error |

**What did not happen:**
- No `ENC ok` and no key error: a missing or wrong key would show `ENC !206` (HCI 0x06, PIN or key missing) or a pairing error 0x3xx / 0x4xx.
- No `PAIR` and no repeat-pairing event.
- So the Mac **did not start encryption at all** before it disconnected. The Cardputer neither rejected a key nor lost its bond in this trace: it never got a key request.

**Relation to the B7 fix (BLE-1 r4):**
- B7 was a link that stayed up, with encryption and CCCD RESTORE arriving before CONNECT, stuck at SUBSCRIBING.
- Here, CONNECT comes first (`aCON new` is correct), nothing is restored, and `aDIS cur` correctly ends the current link.
- The B7 functions are byte-identical to r4 and behave as designed in this trace. **B7 is not involved.**

**Can the second HID service affect encryption or bond restore?**
- **On the Cardputer: no.** The security manager and the bond store (`nimble_bond`) do not depend on the GATT table. CCCD restore happens only after encryption and is keyed by handle; a changed table can only make a restored CCCD miss (`ms c0`), not end the link.
- **On the Mac: plausibly yes (hypothesis, ❓).**
  - BLE-1 r4, Stage 2a and BLE-M1 at boot all presented **the same GATT table**: in BLE-M1 the mouse service was never registered.
  - **r2 is the first build that changes the table of a device that the Mac has bonded and cached**: a fifth service, with new handles after the battery service.
  - NimBLE here has no GATT caching / Database Hash (`CONFIG_BT_NIMBLE_GATT_CACHING` is not set in the 3.3.9 sdkconfig). The only notice of a change is a Service Changed indication, which needs an encrypted link with the Mac subscribed; the link never gets that far. The `g` key cannot reach the Mac either.
  - A second possibility with the same symptom: macOS reads the unencrypted characteristics (the services, the Report Maps) and rejects a device with two HID services.
  - The trace cannot tell these two apart.

**Assessment:**
- Not a firmware bug in the B7 logic, and not a lost bond on the Cardputer, as far as this trace shows.
- The trigger is most likely on the Mac side, caused by the new GATT table.
- Any working mouse fix changes the GATT table that the Mac has cached: proposal A here, and proposal B too (it adds a mouse Report characteristic). So a one-time Mac-side refresh is probably unavoidable for any mouse fix, unless the Mac recovers by itself.
- The firmware cannot force it without an encrypted link. Database Hash support is fixed in the precompiled core, which we do not change.

**Non-destructive checks first (no bond or NVS change, no re-pairing):**
1. **From the r2 trace page (`t`):** the times of `CONN` and `DISC` (how fast the Mac drops), the `E/B` flags of `DISC` (expected `E0`), and whether `MTU` / `UPD` events appear in between. From the status page: the `ENC g… !7 … i… o… c… b…` line. `b` = bonds still stored (expected ≥ 1); `i` / `o` = identity and over-the-air address types, which show whether the Mac's address was resolved to the bonded identity. Also the actual `kb h…` value. A Serial log (115200) gives the same with full codes.
2. **Re-install BLE-1 r4 (`02be4af0…`, hardware PASS) via Launcher.** App-only; no NVS change.
   - If the Mac reconnects normally, the bonds on both sides are intact and the cause is the changed table (or the two HID services).
   - If r4 also fails, the bond state changed earlier (for example during the BLE-M1 test), independent of r2.
   - The installation order and results go to Edgar.
3. **Only then, and only with approval (Edgar and the author):** Mac "Forget This Device" and re-pair with r2.
   - The library then replaces the old bond of this Mac only.
   - If r2 then works, the cause was the Mac's cache: a one-time re-pairing is the cost of adding the mouse (to be decided before integration into Stage 2a).
   - If r2 still drops the link after a fresh pairing, macOS rejects the two-HID-service layout: report to Edgar and consider proposal B.

**Firmware-side options (none recommended now):**
- A new device identity (address or name), so that the Mac sees a new device. This needs pairing anyway and adds a bond; with `CONFIG_BT_NIMBLE_MAX_BONDS` 3, a full store would drop the oldest bond.
- Database Hash: not available without changing the precompiled core.

## 10. BLE-1 r4 reinstalled: stays `BLE ADVERTISING`, `bond=0` (2026-10-14; analysis only)

**Observed (author):**
- BLE-1 r4 reinstalled after BLE-M1 r2. The screen stays `BLE ADVERTISING` with `bond=0`.
- On the Mac, "MediaJog BLE" is listed as a known device but not connected.
- No NVS erase, no bond deletion, no re-pairing.

**What `bond=0` means (r4 source):**
- `bond=` comes from `linkBonded` (`MediaJog_BLE_1_r4.ino:1426`).
- `updateLinkState()` sets it from `ble_gap_conn_find(bleConnHandle)` → `sec_state.bonded` **of the current connection only** (`:799–810`). Without a connection, it is always `false`.
- **So `bond=0` while advertising is normal and says nothing about the bond store.** It was also 0 while advertising in all earlier BLE-1 tests; it becomes 1 only after an encrypted link with a bonded peer.
- The number of stored bonds is the `b<n>` field of the `ENC …` line. It is read with `ble_store_util_count(PEER_SEC)` (`:924`) only when an ENC_CHANGE event occurs; without a connection, the line stays `ENC -`.
- No screen value shows the bond store while nothing is connected.

**Can the bond store have been cleared? Source check of every path that deletes NimBLE bonds (3.3.9 library + sketches):**

| Path | When it runs | Happened? |
|---|---|---|
| `BLEServer` REPEAT_PAIRING handler → `ble_store_util_delete_peer()` (`BLEServer.cpp:968–986`) | A bonded peer starts a **new pairing** | No `PAIR` and no repeat-pairing event in the r2 trace |
| `ble_store_util_status_rr()` (`BLEDevice.cpp:1642`, store-full callback) | **Writing** a new bond (more than `MAX_BONDS` 3) or a new CCCD record (more than `MAX_CCCDS` 8) deletes the oldest **other** peer | No new pairing; no CCCD write (r2 was never encrypted, so no subscribe) |
| `BLEClient` repeat pairing | Client role only | Not used |
| `ble_store_clear()`, `ble_gap_unpair()`, `nvs_erase*` | — | Not called by the library server path or by any MediaJog sketch (grep) |

- **Conclusion from source:** nothing in BLE-1 r4, BLE-M1 or BLE-M1 r2 could have deleted the Cardputer's bond during these tests.
- The bond records stay in NVS (`nimble_bond`) across app installs: the Launcher upload replaces the app only.
- ❓ This is source evidence, not a reading of the store. A direct check needs a connection (the `b` field) or a read-only diagnostic build (not proposed now).

**So what is wrong?**
- The Mac does **not attempt** a connection to r4: there is no `CONN` in the trace. In the r2 test, it connected and closed the link at once, 10+ times.
- The current problem is on the **Mac side**: it no longer auto-connects to the device.
- Plausible causes, not distinguishable from the Cardputer:
  - macOS stops automatic reconnection after repeated failed connections.
  - Its cached GATT data for the device changed when it saw the r2 table.
- The Cardputer side (advertising, bond store, B7 logic) shows no fault.

**Is re-pairing needed? Not yet decided. Non-destructive steps first (no bond or NVS change):**
1. With r4 running: Mac Bluetooth OFF → ON. Then, in System Settings → Bluetooth, use **Connect** on "MediaJog BLE" (**not** "Forget").
2. Read the result on the r4 trace page (`t`) and status page:

| Result | Meaning | Next |
|---|---|---|
| `CONN` → `ENC ok` → `SUB k R1` → `BLE READY`, `bond=1`, ENC line `b≥1` | Both bonds intact; r2's failure was the Mac's reaction to the new GATT table | **No re-pairing needed for r4.** For any mouse build (A or B), a one-time Mac-side refresh is still expected; decide with Edgar |
| `CONN` then `ENC !206` (key missing) or a `PAIR` attempt | One side lost its key | Re-pairing needed (approval) |
| `CONN` then `ENC !7` / `DISC 213` again | The Mac rejects the device even with the r4 table: Mac-side state damaged | Re-pairing needed (approval) |
| Still no `CONN` after **Connect** | The Mac does not use its pairing record | Re-pairing needed (approval) |

- **Re-pairing (only with Edgar's and the author's approval):** Mac "Forget This Device", then pair again. The library then replaces the old bond **of this Mac only** (REPEAT_PAIRING handler); other bonds are kept unless the store exceeds 3 bonds.
- Do the re-pairing with **the build the next test needs**. Re-pairing with r4 and then installing a mouse build would change the GATT table again.

## 11. BLE-M1 r2 hardware result 2 and analysis (2026-10-14; no code change)

**Observed (author):**
- `BLE READY`, `bond=1`.
- `kb h1D c1`, `ms h2D c0`.
- In MOUSE mode (`m`), turning the knob does not move the Mac's pointer.
- "`t` shows no change on the TRACE screen."

**Edgar:** proposal A does not work as implemented. Proposal B (one HID service with several Report characteristics, NimBLE C API) comes next.

### 11.1 Why the pointer does not move

- `ms c0`: the Mac never enabled notifications on the mouse report.
- The firmware sends mouse reports only with the mouse CCCD set (host test M05: nothing is sent), so the pointer cannot move.
- This is the first-level cause; why the Mac did not subscribe is the open point.

### 11.2 r2 GATT contents against HIDS 1.0 / HOGP 1.0 (source review)

| Item | r2 | Assessment |
|---|---|---|
| Service | Second primary 0x1812 (instance 1) | HOGP allows more than one HID Service instance. **Whether macOS uses a second instance is not documented** and not shown here; this is the most likely reason for `c0` |
| HID Information 0x2A4A | `{0x11,0x01,0x00,0x01}` = bcdHID 1.11, country 0, flags RemoteWake | Mandatory, correct; same bytes as `BLEHIDDevice::hidInfo(0x00,0x01)` |
| Report Map 0x2A4B | Mouse map, Report ID 2: 5 buttons + 3 padding bits, X, Y, wheel (relative −127..127), AC Pan | Mandatory; valid HID descriptor; matches the 5-byte `MouseReport` |
| HID Control Point 0x2A4C | Write without response | Mandatory, correct |
| Protocol Mode 0x2A4E | Absent | Mandatory only for Boot Protocol; correct to omit |
| Report 0x2A4D | Read + Notify, encrypted read; Report Reference 0x2908 = `{2, 1}` (ID 2, Input) | Correct; the value is sent without the Report ID, as HOGP requires |
| CCCD | Added by NimBLE for Notify | Correct |

- **No specification error was found in the second service.**
- What remains is host behavior. Either macOS ignores or does not enumerate a second HID instance, or the Mac still works from GATT data cached before r2 (it never learned of the new service).
- These two cannot be told apart without the trace (see 11.3).

**New finding — handle layout depends on heap addresses:**
- The keyboard was `kb h10` on BLE-M1 but `kb h1D` on r2, although the keyboard part is identical.
- The library keeps the characteristics of a service in a `std::map` keyed by the **object pointer** (`BLEService.h:113`), and `BLEService::start()` builds the GATT table in that map order. The service map is pointer-keyed too.
- The order of attributes, and with it every handle, therefore depends on heap addresses. These change with unrelated code or allocation changes.
- Each such change looks to a bonded Mac like a changed GATT table, without notice.
- This alone argues for proposal B with a **static** C table, whose layout is fixed by the source.

### 11.3 Why `t` "shows no change"

- `pollPageKey()` and the trace page code are **unchanged from BLE-1 r3/r4**, where `t` worked on hardware. r2 only adds `m` and `g` after the `t` handling, each with its own edge detection.
- **No conflict with sending to the Mac.** In the BLE builds the Cardputer keyboard is never sent to the Mac; only the knob produces HID reports. `t`, `m` and `g` are read from `M5Cardputer.Keyboard` in the loop task and act locally.
- Expected behavior, by design (r3):
  - The first `t` opens **TRACE 1/n**, a **frozen** copy of the last 64 events. It does not update while it is shown.
  - The next `t` goes to page 2 … n and then back to the status page. With few events, n = 1, so the second `t` returns at once.
  - A frozen page that does not change is therefore expected.
- If, however, the screen **never** showed "TRACE 1/…" after `t`, that is not explained by the source. Please check:
  - whether the header "TRACE x/y" appears;
  - whether `m` works at the same time (`MODE MOUSE` on screen);
  - the Serial log `[UI] page …` (one line per `t` press).
- To read SUB events: press `t` once **after** the Mac has connected, then look for `SUB … m W1` (Mac wrote the mouse CCCD) or its absence.
