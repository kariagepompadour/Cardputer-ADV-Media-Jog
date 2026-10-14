# BLE-B1 — Proposal B test build: one HID service, static NimBLE GATT table

Created: 2026-10-14 · Implementation: Claude Opus 5.5 · Decision basis: Edgar (B1 approved after two source checks)
**Status: built and host-tested. NOT installed on hardware; no Mac re-pairing done.**
- Stage 2a, BLE-1 r4, BLE-M1, BLE-M1 r2 and 1e-4 are unchanged; their `SHA256SUMS.txt` verify.
- No NVS erase, no bond deletion.

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

## 1. Source checks requested before implementation ✅

**Sources:** m5stack:esp32 3.3.9 BLE library; ESP-IDF 5.5 NimBLE (`esp-nimble` `nimble-1.5.0-idf`, `ble_gap.c`, `ble_gatts.c`).

### 1.1 C API GATT registration vs the C++ `BLEServer` (GAP, security)

| Point | Finding |
|---|---|
| Table reset | `BLEDevice::createServer()` calls `ble_gatts_reset()`, `ble_svc_gap_init()`, `ble_svc_gatt_init()` once, when the server is created (`BLEDevice.cpp:194–199`). **The table must be queued after `createServer()`**; B1 does this |
| Registration | `ble_gatts_count_cfg()` + `ble_gatts_add_svcs()` only queue a definition (mutable: no advertising, no connection). `BLEServer::start()` runs `ble_gatts_start()` once (`m_gattsStarted`), which registers everything queued, including our table, and writes the `val_handle` variables. `startAdvertising()` starts the server only if it is not started |
| Library loop in `start()` | It walks only `m_serviceMap` (library services: none in B1) for `ble_gatts_find_svc()` and `m_notifyChrVec`, so nothing touches our table |
| Runtime reset | `resetGATT()` (which would drop our table) runs only on disconnect with `m_svcChanged`. That flag is set only by `createService()` / `removeService()` after the start, which B1 never calls. `onSync()` / `onReset()` do not touch GATT |
| GAP and security | Unchanged: `BLEServer::handleGATTServerEvent` (CONNECT → `startSecurity`, `ServerCallbacks`, DISCONNECT, REPEAT_PAIRING, `advertiseOnDisconnect`), `BLESecurity`, the bond store. They do not depend on GATT objects |
| Library SUBSCRIBE path | Matches `attr_handle` against its own `m_notifyChrVec` (empty), so it does nothing for our reports and cannot double-handle them. NOTIFY_TX: same, no match, returns 0 |
| `ble_gatts_notify_custom()` | Sends a free-form notification on any connection and attribute; **consumes the mbuf regardless of the outcome** (header comment); rc 0 = queued by the host |

**Result: no conflict.**

### 1.2 Subscribe events before CONNECT (B7) reach `handleSubscribe()` correctly

| Point | Finding |
|---|---|
| Delivery | `ble_gap_subscribe_event()` calls `ble_gap_event_listener_call()` (all listeners, **for every reason**: WRITE, TERM, RESTORE) and then `ble_gap_call_conn_event_cb()` (library). Listeners are not gated by the connection state, so the RESTORE events that `ble_gatts_bonding_restored()` posts after a restored ENC_CHANGE (that is, before CONNECT for a peripheral) reach the listener |
| Listener order | `ble_gap_event_listener_register()` uses `SLIST_INSERT_HEAD`: the newest listener is called first. B1 registers the subscribe listener **before** the trace listener, so per event: trace (`SUB`), then subscribe listener (`aSUB`), then the library handler. Before B1, `aSUB` came from the library handler after the listener: same order |
| Handle and value | The library path was `setSubscribe(event)`: `ble_gap_conn_find(event->subscribe.conn_handle)` must succeed, then `subVal` (bit 0 = `cur_notify` for a NOTIFY characteristic), then `onSubscribe(this, &desc, subVal)` → `handleSubscribe(flag, desc->conn_handle, subVal)`. The B1 listener does exactly this: same `conn_find` check, the same `conn_handle` (`desc.conn_handle` = the event's handle), the same value, the same NimBLE host task. `attr_handle` is the characteristic **value** handle for WRITE, TERM and RESTORE (as the library compared with `getHandle()`) |
| `handleSubscribe()` and the B7 functions | Byte-identical to BLE-1 r4 (§3). `adoptConnHandle()` adopts the RESTORE event's handle while no CONNECT has been seen, as in r4 |

**Result: `handleSubscribe()` receives the same connection handle and subscription value as in r4, including before CONNECT.**

## 2. Implementation (`MediaJog_BLE_B1/`, from BLE-M1 r2)

**GATT table (static `ble_gatt_svc_def[]`, every field initialized):**

| Service | Characteristic | Flags | Descriptors |
|---|---|---|---|
| 0x180A | Manufacturer 0x2A29 "M5Stack Cardputer" | READ | — |
| | PnP ID 0x2A50 `02 30 3A 10 01 01 00` | READ | — |
| 0x1812 | HID Information `11 01 00 01` | READ | — |
| | Report Map: BLE-1 r4 map, 146 B, byte-identical | READ | — |
| | HID Control Point | WRITE_NO_RSP | — |
| | Protocol Mode `01` | READ, WRITE_NO_RSP | — |
| | Report: keyboard input (8 B) | READ, NOTIFY | CCCD (NimBLE), 0x2908 `{1,1}` |
| | Report: keyboard output (1 B, LEDs) | READ, WRITE, WRITE_NO_RSP | 0x2908 `{1,2}` |
| | Report: mouse input (5 B) | READ, NOTIFY | CCCD, 0x2908 `{2,1}` |
| 0x180F | Battery Level `100` | READ, NOTIFY | CCCD, 0x2904 `04 00 AD 27 01 00 00` |

**Values and flags are those BLE-1 r4 really registered through the library's NimBLE path**, so that only the layout changes. Deviations from the design table (`BLE_B1_DESIGN.md` §2), for Edgar's review:
- **No `READ_ENC` on the reports.** In the NimBLE build, `BLECharacteristic::setAccessPermissions()` does nothing (`#ifdef CONFIG_BLUEDROID_ENABLED`), and `BLEHIDDevice` deliberately sets no encryption flag. r4's reports therefore had none, and macOS accepted them.
  - The READY rule (encrypted link + keyboard CCCD) still applies before anything is sent.
  - The library calls `startSecurity` on SUBSCRIBE only for characteristics with an ENC/AUTHEN/AUTHOR flag, so B1's listener does not call it either, as r4.
- **PnP ID kept as r4 sent it:** `BLEHIDDevice::pnp()` writes VID/PID/version high byte first. The specification is little-endian; a fix belongs to a later, separate change.
- **Descriptors read-only.** The library gave them READ | WRITE; HIDS defines the Report Reference as read-only.

**Code changes (function comparison):**
- **Against BLE-1 r4:** 46 functions are identical, including:
  - `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`;
  - `ServerCallbacks`, `SecurityCallbacks`, `onGapEvent`, `updateLinkState`, `onLinkLost`, `onLinkReady`, `sendMouse`.
- **Against BLE-M1 r2:**
  - Changed: `notifyReport` (now `ble_gatts_notify_custom()`, sent only with that report's CCCD on; `NOTIFY_OK` / `NO_CLIENT` / `NO_SUBSCRIBER` / `HOST_ERROR` with rc), `bleBegin` (subscribe listener first; table queued after `createServer()`; one `bleServer->start()`; advertising UUID 0x1812; Serial prints all handles and the registration rc), `drawStatus` (`ki<h> c<cccd> ko<h> w<LED writes> mi<h> c<cccd> mr… g…`, live).
  - Removed: `ReportCallbacks`, `createMouseService`.
  - New: `gattConstAccess`, `gattValueAccess` (reads under a spinlock; writes length-checked, LED writes counted), `setGattValue`, `registerGattTable`, `onSubscribeEvent`.
- **Unchanged:** MOUSE mode (`m`), `g` (Service Changed), the trace pages (`t`), U135 reading, security settings.
- **Not included:** USB HID, profiles, HOLD, CUSTOM, NVS writes.

## 3. Host tests ✅ (logic only, not hardware)

**Stubs** (new harness; the earlier models are kept):
- NimBLE C API: `count_cfg` must precede `add_svcs`; `add_svcs` only queues, and returns `EBUSY` while advertising or connected.
- The single `ble_gatts_start()` registers in queue order: service declaration, then per characteristic its declaration and value, the CCCD right after the value for NOTIFY, then its descriptors. There is no duplicate-UUID check.
- `notify_custom` consumes the mbuf; host errors can be injected; ATT reads and writes check the flags.
- GAP listeners are called newest first, then the library handler. SUBSCRIBE events carry WRITE, TERM or RESTORE.

**BLE-B1 end to end: 55/55.**

| Scenario | Checks |
|---|---|
| B01 | Table counted and queued after `createServer`, registered by the one server start; no library service; **exactly one HID service** (DIS, HID, BAS); three distinct report handles in table order, all in the HID service; CCCD only on keyboard input, mouse input and battery; Report References `{1,1}` `{1,2}` `{2,1}`; HID characteristic order; Report Map = r4 map byte for byte; r4 flags; DIS and battery values; initial values |
| B02 | **B7 order** (ENC, RESTORE of both CCCDs, then CONNECT): adopted, **kb=1, ms=1**, READY; all-up and released on their own handles; no mbuf leak |
| B03 | First pairing: CCCD writes give kb=1, ms=1, READY; stored for restore |
| B04–B05 | MOUSE mode: X +8, drag, button up, host read of the last report; mode change releases the button |
| B06 | Without the mouse CCCD: nothing sent in MOUSE mode, keyboard works; status line shows live `ki/ko/mi` handles |
| B07 | `g` with and without a link |
| B08 | Disconnect during drag (TERM clears both flags), RESTORE-first reconnect (kb=1, ms=1, released), no stale button-down; one server start |
| B09 | Keyboard RESTORE before CONNECT adopted; **trace order SUB → aSUB**; foreign handle (FGN); trace label `m` |
| B10 | SUBSCRIBE for an unknown connection: `handleSubscribe()` not called; battery CCCD ignored; keyboard CCCD off → not READY |
| B11 | Injected host error: press not sent and counted, retried and sent next loop; all mbufs consumed |
| B12 | LED write (1 byte; with and without response) accepted and counted; 2 bytes rejected; Protocol Mode reads 1; Control Point write; input reports and descriptors not writable |

**Regression of the earlier finding (same new stubs):** BLE-1 r4 and Stage 2a still give mouse handle 0xFFFF (2 duplicates dropped).

**Mutation checks (each must fail):**

| Mutant | Failures |
|---|---|
| m1: mouse report removed from the table | 15 |
| m2: mouse report writes its handle into the keyboard's variable | 15 |
| m3: listener without the `conn_find` check | 2 |
| m4: table queued after `bleServer->start()` | 30 |
| m5: subscribe listener registered after the trace listener | 1 (trace order) |
| m6: `ble_gatts_count_cfg()` omitted | 30 |
| m7: mouse CCCD sets the keyboard flag | 9 |

Some counts include a scenario that aborts when a handle is missing; it counts as a failure.

## 4. Build (2026-10-14)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, **0 sketch warnings** (all table fields explicitly initialized); `#error` guards effective (hwcdc, CDC off). Flash 864,379 B (65 %), RAM 68,916 B (21 %) |
| Source `MediaJog_BLE_B1/MediaJog_BLE_B1.ino` | 86,709 B, SHA-256 `7f5ab35dec573d51e9b373946b96654d1715fa60e14278248872ff60de32c95b` |
| App-only binary `MediaJog_BLE_B1/build/MediaJog_BLE_B1.ino.bin` | 864,528 B, SHA-256 `d550cc5534aea4f199e27e9d225aaa69753ad5e4953a697c4694c8525660812e`; contains `BLE-B1` and the `ki%X …` status format |
| Host-tested source | Equals the final preprocessed source |

## 5. Hardware procedure (proposed; after Edgar's review; not run)

**Rules:**
- Do not delete bonds, NVS or Launcher settings.
- Mac-side re-pairing only with Edgar's and the author's approval. The library then replaces the bond of this Mac only.

| # | Do | Look for |
|---|---|---|
| 1 | Install `MediaJog_BLE_B1.ino.bin` (app-only, Launcher Wi-Fi); record its SHA-256 (`d550cc55…`) | Tag `BB1`; status `ki<h> … ko<h> … mi<h> …`, all three not `FFFF`. Record the values; they are fixed by the table. Optional Serial: `[BLE] advertising; gatt rc 0 …` with all handles |
| 2 | Let the Mac reconnect with the existing bond | Expected (BLE-M1 r2 history): the table differs from the Mac's cache, so it may not subscribe (`ki c0` / `mi c0`) or may drop the link. Trace page (`t` after the connection): `SUB` lines |
| 3 | Optional: `g` while connected and encrypted; Mac Bluetooth OFF → ON | Whether `ki c1` and `mi c1` appear without re-pairing |
| 4 | **Only with approval:** Mac "Forget This Device", pair again with B1 | `BLE READY`, `bond=1`, `ki … c1`, `mi … c1` |
| 5 | KEYS: Right / Left / Space; `m` MOUSE: pointer left/right, drag; `m` back | Pointer moves (`mr` increases); keys work |
| 6 | B7 regression: Mac Bluetooth OFF → ON ×3, Cardputer restart | Each time `BLE READY` with `c1` on both inputs (RESTORE before CONNECT is the B7 path) |

**Outcomes to report to Edgar:**
- **B1 works:** the layout for the Stage 2a integration (a separate revision with the same static table, so no further GATT change).
- **The Mac still does not subscribe to the mouse after a fresh pairing:** report the trace (`SUB` lines, handles) before any next step.

## 6. Limitations

- The table and its handles are new to the Mac; one re-pairing is expected (§5 step 4).
- The keyboard LED output report is accepted but not used.
- Mouse: X only, 8 counts per detent; no sensitivity setting (test build).
- PnP ID byte order kept as r4 (§2).
