# BLE-B1 — Design: one HID service with all reports (proposal B, NimBLE C API)

Created: 2026-10-14 · Design: Claude Opus 5.5 · Decision basis: Edgar (proposal B next; design first, no code or build yet)
**Status: design only.** Stage 2a, BLE-1 r4, BLE-M1, BLE-M1 r2, the pairing data and NVS are not changed.

## 1. Goal and principle

- One HID Service (0x1812) with **keyboard input (ID 1), keyboard output (ID 1, LEDs) and mouse input (ID 2)**, all registered with distinct handles.
- The GATT table is a **static NimBLE C definition** (`ble_gatt_svc_def[]`), so the attribute order and all handles are fixed by the source. The library's pointer-ordered maps (`BLE_M1.md` §11.2) no longer decide the layout.
- **Only the GATT definition and its two data paths (subscribe in, notify out) change.** The GAP connection handling, security, bonding and the B7 logic stay as in BLE-1 r4.

## 2. GATT table (static, registered before the server start)

| Service | Characteristic (UUID) | Flags | Descriptors | Value / access |
|---|---|---|---|---|
| Device Information 0x180A | Manufacturer 0x2A29 | READ | — | "M5Stack Cardputer" |
| | PnP ID 0x2A50 | READ | — | `02 3A 30 01 10 00 01` (as r4) |
| HID 0x1812 | HID Information 0x2A4A | READ | — | `11 01 00 01` |
| | Report Map 0x2A4B | READ | — | the BLE-1 combined map (keyboard ID 1 + mouse ID 2), bytes unchanged |
| | HID Control Point 0x2A4C | WRITE_NO_RSP | — | accepted, ignored |
| | Protocol Mode 0x2A4E | READ, WRITE_NO_RSP | — | `01` (Report Protocol), as `BLEHIDDevice` |
| | Report 0x2A4D (kb in) | READ, READ_ENC, NOTIFY | 0x2908 `{1,1}` (+ CCCD by NimBLE) | last keyboard report |
| | Report 0x2A4D (kb out) | READ, READ_ENC, WRITE, WRITE_ENC, WRITE_NO_RSP | 0x2908 `{1,2}` | LED byte stored, counted, not used |
| | Report 0x2A4D (mouse in) | READ, READ_ENC, NOTIFY | 0x2908 `{2,1}` (+ CCCD) | last mouse report |
| Battery 0x180F | Battery Level 0x2A19 | READ, NOTIFY | 0x2904 (uint8, unit 0x27AD), as `BLEHIDDevice` | 100 |

- NimBLE's own table does not drop repeated UUIDs; each Report has its own `val_handle` variable. The library's duplicate drop happens only in the C++ `BLEService` map, which is not used here.
- One `access_cb` serves all of these characteristics, dispatching on `arg`. Reads return the current value with `os_mbuf_append()`. Writes to the keyboard output are length-checked and counted. Descriptor reads return the fixed bytes.
- Registration order: `ble_gatts_count_cfg()` and `ble_gatts_add_svcs()` with the whole table, **before** `bleServer->start()`, the single `ble_gatts_start()` (`BLE_M1.md` §7–8).
- `BLEHIDDevice` is not created, so the library registers no services: its `m_serviceMap` is empty and its `start()` loop has nothing to do.
- The advertising data still lists 0x1812 (`addServiceUUID`), with appearance 0x03C1 as in r4.

## 3. What is kept from BLE-1 r4 (B7 and link handling)

| Part | Kept how |
|---|---|
| `BLEDevice::init`, `BLESecurity` settings, `BLEServer` with `ServerCallbacks` (`appConnect` / `appDisconnect`), `advertiseOnDisconnect`, the GAP listener (`appConnectFailed`, trace) | Unchanged (byte comparison with r4 as in M1) |
| `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, **`handleSubscribe`** | Byte-identical |
| Subscribe input | Before: library `BLEServer` SUBSCRIBE → `m_notifyChrVec` match → `ReportCallbacks::onSubscribe` → `handleSubscribe(flag, conn, value)`. **B1:** the existing GAP listener gets the same `BLE_GAP_EVENT_SUBSCRIBE`. A small adapter matches `attr_handle` to the keyboard or mouse input `val_handle`, applies the library's check (`ble_gap_conn_find()` must succeed), and calls the same `handleSubscribe()` with `cur_notify`. Same task (NimBLE host), same event, same arguments |
| Event order (B7: ENC and RESTORE before CONNECT) | Unchanged. The adapter runs per SUBSCRIBE event, as the library handler did; `adoptConnHandle()` covers RESTORE before CONNECT exactly as in r4 |
| Encryption and READY rule | Unchanged: READY = connected + encrypted + keyboard CCCD. All reports need encryption (`READ_ENC`); CCCDs are restored by NimBLE only for bonded, encrypted peers |
| Disconnect | Unchanged (`appDisconnect` for the current connection only, link-lost clearing, release on reconnect) |
| Notify output | `notifyReport()` uses `ble_gatts_notify_custom(conn, val_handle, ble_hs_mbuf_from_flat(...))` instead of `BLECharacteristic::notify()`. rc 0 = accepted by the host, which is the meaning `SUCCESS_NOTIFY` had (not a delivery confirmation). Any other rc = not sent. The mbuf is consumed by NimBLE in both cases. Sends only to `bleConnHandle` and only when that report's CCCD flag is set: the library checked its subscriber list, and B1 checks the flag that `handleSubscribe()` maintains |
| Battery notify | Not sent (as r4) |

**Risks to review:**
- The GAP listener runs **before** the library's own handler for the same event. For SUBSCRIBE, the library now does nothing for our characteristics, so no order changes between handlers.
- For CONNECT and DISCONNECT, nothing changes.
- The library's SUBSCRIBE path also called `BLESecurity::startSecurity()` when a client subscribed on an unencrypted link. NimBLE itself refuses unencrypted CCCD access to `READ_ENC` characteristics. To keep the r4 behavior, the adapter does the same: if the link is not encrypted, it calls `startSecurity`.

## 4. Pairing and the Mac's GATT cache

- B1's table differs from every earlier build, and NimBLE here has no Database Hash. The bonded Mac's cached table will not match.
- From BLE-M1 r2's history, the Mac then drops or does not use the new services.
- Expect **one Mac-side re-pairing** with B1 (approval needed; the library replaces this Mac's bond only).
- After that, the static table stays the same for every later build that keeps it, including the Stage 2a integration. This is the last forced layout change.
- Optionally, B1 keeps the `g` key (Service Changed indication, whole range) for a bonded Mac that does connect and encrypt.

## 5. Independent test build `MediaJog_BLE_B1/` (tag `BB1`, build id `BLE-B1`)

- **Base:** BLE-M1 r2 without the second service. It is BLE-1 r4 plus MOUSE mode (`m`), `g`, and the live handle diagnostics.
- **BLE only**, as BLE-1:
  - no USB HID (`USBHID` not created);
  - no profiles, no HOLD, no CUSTOM;
  - **no NVS writes** (NimBLE's own bond store only; statistics read-only).
- **Unchanged:** `MediaJog_1e_4/`, `MediaJog_2a/`, `MediaJog_BLE_1_r4/`, `MediaJog_BLE_M1*/`. USB, NVS (`mediajog/*`) and HOLD code paths are not part of this build, so they cannot be affected. Integration into Stage 2a is a later, separate revision.
- **Diagnostics:**
  - status line `ki h… c… ko h… w… mi h… c…` (live `val_handle`s; CCCD flags; LED writes);
  - last notify rc;
  - trace `SUB` lines name `k`, `m`, `x<handle>`.
  - Serial prints the whole handle table once at start.
- **Host tests (planned):**
  - NimBLE C API stubs: `count_cfg` / `add_svcs` queue a table; `ble_gatts_start()` assigns handles in table order, with no dedupe (unlike the C++ library model, which is kept for regression); `notify_custom` with rc and mbuf ownership; the GAP listener receives SUBSCRIBE.
  - Checks:
    - three distinct Report handles, Report References `{1,1}` `{1,2}` `{2,1}`, CCCDs only on the inputs, the same handles on every run;
    - the B7 order with **both** CCCDs restored before CONNECT (kb=1, ms=1, READY);
    - FGN on a foreign handle; no notify without a CCCD; notify rc ≠ 0 means not sent;
    - LED write accepted; mouse movement, drag and release; disconnect during drag;
    - the duplicate-UUID repro kept for r4 / 2a.
  - Mutants: the mouse report removed or sharing the keyboard's handle; the adapter dropping the `conn_find` check.
- **Hardware plan (after review):**
  - install B1;
  - with approval, re-pair once;
  - check `ki`, `ko`, `mi` handles, `kb c1 ms c1`, keys, pointer and drag;
  - B7 regression (Mac Bluetooth OFF→ON ×3, restart).

## 6. Implementation notes (2026-10-14, after Edgar's approval)

- The two source checks Edgar asked for (C API registration vs `BLEServer`; SUBSCRIBE delivery before CONNECT) are recorded in `BLE_B1.md` §1. Neither found a conflict.
- Implemented as designed, with three deviations for Edgar's review (`BLE_B1.md` §2):
  1. **Reports without `READ_ENC`, and no `startSecurity` in the subscribe listener.** These are the flags BLE-1 r4 effectively had: the library ignores `setAccessPermissions()` on NimBLE, and it calls `startSecurity` only for characteristics with an encryption flag. This replaces §2's `READ_ENC` and §3's "the adapter calls `startSecurity`".
  2. **PnP ID bytes as r4 sent them** (high byte first, the library's `pnp()` order). §2 listed the little-endian form.
  3. **Descriptors read-only** (the library had READ | WRITE).
- Listener registration order: the subscribe listener is registered before the trace listener. NimBLE inserts at the head, so the trace runs first, as before.
