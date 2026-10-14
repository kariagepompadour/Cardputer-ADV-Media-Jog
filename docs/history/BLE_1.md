# BLE-1 — Minimal BLE HID prototype (separate firmware)

Created: 2026-10-13 · Implementation: Claude Opus 5.5
**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) independent review before hardware testing.**

> **Status update (2026-10-13, appended):** Edgar's independent review of the first build found two issues, now fixed in place (BLE-1 had never been installed): mouse movement was subtracted before the report was accepted, and the all-up repeat after connection was skipped when taps were queued and was not retried. Details, old and new hashes: §10. The sections below describe the fixed implementation.

> **Status update 2 (2026-10-13, appended):** BLE-1 (binary `3196bffd…`) was tested on hardware. B1, B2, B3, B4 and B6 passed; **B7 (Mac Bluetooth OFF → ON) failed**: the Mac shows "connected", the Cardputer stays at `BLE SUBSCRIBING`. Investigation and the diagnostic revision **BLE-1 r2** (`MediaJog_BLE_1_r2/`): §11.

> **Status update 3 (2026-10-13, appended):** the r2 diagnosis of B7 was run on hardware (§12-1). The root cause is **not** established. **BLE-1 r3** (`MediaJog_BLE_1_r3/`, diagnostics only, screen tag `B1r3`) records each event with its connection generation, handle, full status code and link flags (§12).

> **Status update 4 (2026-10-13, appended):** the r3 trace showed the CCCD RESTORE of a new connection arriving **before** the library's `onConnect()`. BLE-1 rejected it as foreign and then cleared it. This is confirmed in the BLE-1 and NimBLE source and reproduced in a host test; it also explains the earlier r2 B7 photo (§12, whose open points are resolved in §13-2). **BLE-1 r4** (`MediaJog_BLE_1_r4/`, tag `B1r4`) fixes the connection-handle bookkeeping. **Not yet hardware-tested; B7 is not declared fixed.**

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

BLE-1 checks that the Cardputer ADV works as a BLE HID keyboard + mouse for macOS, **without changing the USB release candidate** (`MediaJog_1e_4/`, unchanged; its `SHA256SUMS.txt` still verifies). BLE-1 is a separate, BLE-only firmware. It has no USB HID and no USB/BLE switching.

## 1. Scope

| Item | BLE-1 |
|---|---|
| Hardware | Cardputer ADV (ESP32-S3) + Unit Encoder U135 (same I2C code as 1e-4) |
| Host | macOS |
| Mapping (fixed, no profiles) | CW = **Right Arrow** (0x4F), CCW = **Left Arrow** (0x50), knob press = **Space** (0x2C), TAP only. These are the same usages as the hardware-verified Profile 3 of the USB builds |
| HID | Keyboard (Report ID 1) + mouse (Report ID 2). The mouse is prepared (report map, characteristic, `sendMouse()`) but sends only the released state |
| USB | No USB HID: no `USBHID` object. USB-C still powers the device and carries the CDC serial log; the app is started from the Launcher |
| NVS | Nothing written by the sketch. Bonds are stored by NimBLE itself. The sketch only reads `nvs_get_stats()` for diagnostics |
| Not included | Profiles, CUSTOM, HOLD, Spinner movement, MediaJog NVS settings, bond deletion UI, USB/BLE coexistence |

## 2. BLE stack and API (source-checked, m5stack:esp32 3.3.9)

- **Stack ✅**
  - Precompiled ESP32-S3 libraries: `CONFIG_BT_NIMBLE_ENABLED=y` (Bluedroid off).
  - `CONFIG_BT_NIMBLE_NVS_PERSIST=y`, `MAX_BONDS=3`, `MAX_CONNECTIONS=3`.
  - The sketch stops with `#error` if `CONFIG_NIMBLE_ENABLED` is not defined.
- **Library ✅:** the core's own `BLE` library v3.3.9: `BLEDevice`, `BLEServer`, `BLEHIDDevice`, `BLESecurity`. No third-party library.
- **Security ✅:** as in the core example `Server_Gamepad`:
  - `setCapability(ESP_IO_CAP_NONE)` ("Just Works");
  - `setAuthenticationMode(true, false, true)` (bonding, no MITM, LE Secure Connections);
  - `onSecurityRequest()` returns true.
- **Connection events ✅:** `BLEServer` calls both `onConnect`/`onDisconnect` overloads (`BLEServer.cpp`). BLE-1 implements only the `ble_gap_conn_desc*` overloads. Advertising restarts through `BLEServer::advertiseOnDisconnect(true)`.
- **CCCD ✅:** handled by NimBLE. `BLECharacteristic::setSubscribe()` calls `BLECharacteristicCallbacks::onSubscribe(…, subValue)`. For a bonded host, NimBLE restores the stored CCCD state when the link is encrypted (subscribe reason "restore"). The restore path is ❓ on hardware.
- **Repeat pairing ✅:** if a bonded Mac pairs again (e.g. after "Forget This Device" on the Mac), `BLEServer` deletes the old bond **of that peer only** (`ble_store_util_delete_peer`) and retries pairing (`BLEServer.cpp:968-987`).
- **Bond table full ✅:** the default `BLEDeviceCallbacks::onStoreStatus()` calls `ble_store_util_status_rr` (NimBLE's round-robin handling). Behavior on a 4th host is ❓.
- **`notify()` ✅:**
  - It returns `void`. In NimBLE mode it reports through `onStatus()`, called **synchronously inside `notify()`**:
    - `SUCCESS_NOTIFY` once `ble_gatts_notify_custom()` returned 0, i.e. the NimBLE host accepted the notification;
    - `ERROR_NO_CLIENT`, `ERROR_NO_SUBSCRIBER` or `ERROR_GATT` (with the NimBLE error code).
  - If every subscriber is skipped inside the loop (e.g. a stale connection), **no status is reported at all**.
  - `SUCCESS_NOTIFY` is not a delivery confirmation from the Mac.
- **Encryption of input reports:** in NimBLE mode, `BLEHIDDevice::inputReport()` does not mark the characteristic as encryption-required, so `notify()` does not check encryption. BLE-1 therefore checks the link's encryption state itself (`ble_gap_conn_find()` → `sec_state.encrypted`) before sending.

## 3. HID report map and reports

- **Report map:** TinyUSB's `TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1))` + `TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2))`, written out byte by byte in the sketch.
  - **Keyboard, ID 1:** 8 modifier bits, 1 reserved byte, 6 key codes (input, 8 bytes); 5 LED bits + 3 padding (output, 1 byte).
  - **Mouse, ID 2:** 5 buttons + 3 padding, X, Y, wheel (relative, −127…127), AC Pan (input, 5 bytes).
  - The layouts are identical to the reports the USB builds send (`hid_keyboard_report_t`, `hid_mouse_report_t`). The Stage 1e logic can therefore later produce the same bytes for either transport.
- **No Report ID in the value:** per HID over GATT, the characteristic value is the report **without** the Report ID; the ID is in the Report Reference descriptor that `BLEHIDDevice::inputReport(id)` creates. The core's `Server_Gamepad` example puts the ID into the value; BLE-1 deliberately does not.
- **Characteristics:**
  - keyboard input (notify), keyboard output (LEDs; host writes are accepted and ignored), mouse input (notify);
  - Protocol Mode set to Report Protocol (0x01), because `BLEHIDDevice` leaves it unset;
  - HID Information country 0, flags 0x01 (as `Server_Gamepad`);
  - Battery Level fixed at 100 (not measured).
- **Identity:**
  - name `MediaJog BLE`;
  - appearance 0x03C1 (HID keyboard);
  - PnP ID = USB-IF source, the board's own USB VID/PID 0x303A/0x1001 (`boards.txt`), version 1.0. The example's Apple IDs are not used.

## 4. Connection and pairing

| State (screen line 1) | Condition |
|---|---|
| `BLE ADVERTISING` | not connected (advertising; restarts after every disconnect) |
| `BLE PAIRING` | connected, link not yet encrypted |
| `BLE SUBSCRIBING` | encrypted, keyboard input notifications not enabled yet |
| `BLE READY` | **connected AND encrypted AND keyboard input subscribed**: the only state in which reports are sent |

The diagnostics show:
- bond state of the link, keyboard and mouse subscription;
- tap counters, notify failures, inputs skipped;
- connects, disconnects, link-ups and link-losses, queue, loop time;
- U135 state;
- NVS statistics: used entries at boot (before BLE starts) and now, available entries, namespace count, refreshed every 5 s (read-only).

**First pairing (macOS):** System Settings → Bluetooth → "MediaJog BLE" → Connect. Just Works, no code. **Reconnect:** after a restart the device advertises and the bonded Mac reconnects; the link is encrypted with the stored keys and the CCCD state is restored.

Thread model: BLE callbacks run in the NimBLE host task and only change `std::atomic` flags and counters (connected, connection handle, connection generation, subscriptions). All decisions are made in `loop()` (`updateLinkState()`).

## 5. HID safety

| Requirement | Implementation |
|---|---|
| No key state carried over at boot | All key/mouse state is statically all-up. Nothing is sent before READY. The U135 knob state at start is the reference (a knob held at boot does nothing until released and pressed again) |
| Start every connection all-up | On every transition to READY (`onLinkReady()`): an all-keys-up keyboard report first. **At 100 ms it is repeated exactly once, whatever is queued**: the repeat sets `releasePending`, so `sendQueued()` sends it **before any further tap** and **retries it every loop until it is accepted** (`serviceReports()`). A released mouse report as soon as the mouse report is subscribed |
| Clear internal HOLD state on disconnect | On link loss (`onLinkLost()`): queue cleared; base key state (`kbHeldMods/kbHeldUsage`, the 1e-3 HOLD state, empty in BLE-1), pending release, mouse button and movement, and detent remainder all cleared |
| Do not resend old presses after reconnect | The queue is cleared on loss; only the all-up state is sent at the next READY |
| BTN TAP only | Press → one Space tap; release sends nothing |
| No queueing before connection | `pushEvent()` drops and counts inputs while not READY |
| `notify()` gives no delivery guarantee | A report counts as sent only with `SUCCESS_NOTIFY` (accepted by the NimBLE host). A press that was not accepted was not sent at all, so its event stays queued and is retried in order. A release (including the initial all-up and its repeat) that was not accepted is retried every loop before any further tap. Mouse movement is removed from `mousePendingDx` **only when its report was accepted**, so a rejected report loses no movement (unlike USB, where a failed send has an unknown outcome and is not resent). No status at all counts as "not accepted" |
| Link loss detection | Disconnect callback; **a disconnect + reconnect between two loops** (connection generation changed); loss of encryption; keyboard CCCD disabled. Each is treated as a full link loss |

**Host test ✅ (logic only, not hardware):** the real functions (`notifyReport`, `sendKeyReport`, `sendRelease`, `tapKey`, `sendMouse`, the queue, `sendQueued`, `serviceReports`, `onLinkLost`, `onLinkReady`, `updateLinkState`, `onRotation`, `onButton`) were extracted from the sketch and run with a mock characteristic (which can reject the next N notifications) and a mock connection table. The test loop calls the sketch's own `serviceReports()`. **36/36 passed**:
- dropped inputs before connection, before encryption and before CCCD;
- all-up first plus a single repeat;
- Right/Left/Space sequences; mouse released once after a late subscription;
- GATT rejection and "no status" keep the event; release retried before the next tap;
- disconnect clears all state; no replay after reconnect; reconnect between loops; CCCD off while READY;
- (review 1) input in the loop of connection dropped; taps right after READY follow the all-up; the 100 ms repeat is sent although taps are queued, and only once; a rejected repeat is retried before the queued tap; an initial all-up rejected three times holds the taps back, then all-up, Right, Space follow in order;
- (review 1) mouse: a rejected report keeps the movement (300 → 300), accepted reports remove 127 + 127 + 46; a rejected button-state report stays pending.

The same tests run against the pre-review source (`archive/MediaJog_BLE_1_review1/`) fail 7 checks (repeat skipped or not retried; movement lost), so they detect both defects.

## 6. NVS and coexistence

- **Sketch:** only `nvs_get_stats(NULL, …)` (read-only). No `nvs_flash_erase`, `nvs_erase_*`, `Preferences`, `ble_store_clear` or `ble_gap_unpair` (grep ✅).
- **NimBLE bond store ✅:** namespace `nimble_bond` (string in `libbt.a`). The precompiled `ble_store_nvs.c` references `nvs_open/get_blob/set_blob/commit/erase_key/close`. NimBLE's NVS port also references `nvs_erase_all`, which acts on one opened namespace. No `libbt.a` object references `nvs_flash_erase` or `esp_partition_erase*` (symbol check).
- **`BLEDevice::init()` ✅:** in the NimBLE configuration it calls `nimble_port_init()` and `ble_store_config_init()`, with no erase. The `nvs_flash_erase()` in `BLEDevice.cpp:312-316` belongs to the Bluedroid, non-Arduino branch and is not compiled here.
- **PHY calibration:** `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=y`. Starting the radio may read/write calibration data in NVS. The USB builds never start the radio. ❓ The size and namespace are to be observed with the NVS line (boot vs. now).
- **Core boot-time erase path** (`esp32-hal-misc.c:331-344`, `STAGE_1E_4.md` §6.3): unchanged, and present in BLE-1 as in every Arduino build.
- **MediaJog settings record** (`mediajog`/`cfg`): not read or written by BLE-1, so the USB firmware's record is untouched.

## 7. Build results and checksums (2026-10-13)

| Item | Result |
|---|---|
| Environment | arduino-cli 1.5.1, m5stack:esp32 3.3.9, FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc` (the established settings); M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2; core BLE library 3.3.9 |
| Result (clean build, after review 1) | ✅ exit 0. **Flash 862,187 B (65%)** of the default 1,310,720-byte app partition; global RAM **65,412 B (19%)** |
| Size vs 1e-4 | +254,008 B flash (608,179 → 862,187), +5,184 B global RAM. Fits the default partition. ❓ The Launcher's actual app slot size is still unverified |
| Warnings | **0 in the sketch.** An earlier build had 5 C++20 `-Wvolatile` deprecations (`++` on `volatile` counters); fixed by making the values shared with the BLE task `std::atomic`. The clean build shows only core-internal warnings (pin macros redefined, `missing-field-initializers` in core/IDF headers) |
| `#error` guards | ✅ hwcdc build stops; CDC-off build stops; non-NimBLE configuration stops |
| Source `MediaJog_BLE_1/MediaJog_BLE_1.ino` | 34,787 B, SHA-256 `40bde390f0431dd446cc5b62c67bea907dc623797ae3d2df725f8bcbe64898c0` |
| App-only binary `MediaJog_BLE_1/build/MediaJog_BLE_1.ino.bin` | 862,336 B, SHA-256 `3196bffdf19fd4902ae3247313f95ca661a76e5190eb07483f3fadb2a47730e0` |
| Binary check | Contains `BLE-1`, `MediaJog BLE`, `nimble_bond`; does not contain `1e-4` |

Both hashes are in `MediaJog_BLE_1/SHA256SUMS.txt` (`shasum -a 256 -c` → OK).

## 8. Hardware test checklist (proposed; not run)

Install `MediaJog_BLE_1.ino.bin` (app-only) through the Launcher Wi-Fi upload and **record the SHA-256 of the uploaded file** (`3196bffd…7730e0`; the pre-review binary `faaaa40f…` must not be installed). Note the NVS line before the first pairing.

| # | Do | Pass condition |
|---|---|---|
| B1 | Start the app | Tag `BLE-1`; `BLE ADVERTISING`; no HID device appears on the Mac over the USB cable; turning/pressing increments `SKIP` only |
| B2 | Mac: System Settings → Bluetooth → connect "MediaJog BLE" | Pairs without a code; screen goes `BLE PAIRING` → `BLE READY`; `bond=1 kb=1` (and `ms=1` if macOS subscribes to the mouse) |
| B3 | TextEdit with text: turn CW / CCW one detent at a time | Cursor moves right / left by one character per detent; no repeated or stuck arrow |
| B4 | Press the knob | One space per press; holding the knob does not repeat |
| B5 | Fast rotation | All steps or a plausible count; no stuck key; `FAIL` count noted |
| B6 | Restart the Cardputer (power OFF/ON) | Reconnects without pairing again; first action works; nothing typed by itself |
| B7 | Mac: Bluetooth OFF, turn the knob, Bluetooth ON | `BLE ADVERTISING` while off, inputs skipped; reconnects; no old movement replayed |
| B8 | Move away / out of range until disconnect, return | As B7 |
| B9 | Mac sleep and wake | Record whether the link survives or reconnects; no stuck key |
| B10 | Hold the knob while disconnecting (Bluetooth OFF), release, reconnect | Nothing pressed after reconnect (Space is TAP only; this checks no replay) |
| B11 | Mac: "Forget This Device", then pair again | Pairing succeeds (repeat-pairing path); B3 works |
| B12 | NVS line after pairing and after B6 | Record `used` boot → now and the namespace count (bond + possible PHY calibration data) |
| B13 | Launcher, Wi-Fi settings, SOUND TERRARIUM, then MediaJog 1e-4 | All start and keep their settings; **1e-4 still restores its profile/CUSTOM/sensitivity** |
| B14 | USB cable to the Mac while BLE-1 runs | Power/charging only; no USB keyboard/mouse appears (System Information → USB may show a CDC serial device) |
| B15 | Battery: note the time on battery with BLE active (optional) | Recorded only |

## 9. Known limitations

- Fixed mapping; no profiles, CUSTOM, HOLD, Spinner movement or saved settings in BLE-1.
- No bond deletion on the device. Re-pairing after "Forget" on the Mac uses the library's repeat-pairing handling. A 4th host uses NimBLE's round-robin handling (❓).
- No delivery confirmation over BLE (`SUCCESS_NOTIFY` = accepted by the local stack). Safety relies on state reports: all-up at every READY, repeated once at 100 ms, each retried until accepted.
- Advertising runs whenever not connected (no timeout); this affects battery life.
- Any bonded host may connect (no host selection; plan Phase 3c).
- The Launcher app-slot size, macOS reconnect behavior, Mac-side key release on disconnect, BLE latency and NVS growth are unverified until the hardware test.
- The pre-existing core NVS erase path (`STAGE_1E_4.md` §6.3) applies to BLE-1 as to every Arduino build.

## 10. Revision after Edgar's review 1 (2026-10-13)

The first build was reviewed by Edgar before any installation. It was fixed **in place**, as requested, because it had never been installed or tested. The reviewed source is kept byte-identical in `archive/MediaJog_BLE_1_review1/MediaJog_BLE_1.ino`.

| # | Finding | Change |
|---|---|---|
| 1 | `sendMouse()` subtracted the chunk from `mousePendingDx` **before** `notifyReport()` reported acceptance; a rejected report lost its movement | The chunk is subtracted only after `SUCCESS_NOTIFY` |
| 2 | The 100 ms all-up repeat ran only when the queue was empty, and a rejected repeat was not retried. The design text said "repeated once 100 ms later" without these exceptions | The repeat sets `releasePending` whatever is queued. `sendQueued()` then sends it before any further tap and retries it every loop until it is accepted. The repeat logic moved, otherwise unchanged, from `loop()` into `serviceReports()` so the host test runs the real code |

- **Diff vs the reviewed source:** 23 insertions, 11 deletions. Only `sendMouse()`, the new `serviceReports()` (the former `loop()` block), `loop()` (calls it) and the header comment changed.
- **Clean build:** exit 0, 0 sketch warnings, core warning set identical to the first clean build; flash 862,187 B, RAM 65,412 B.
- **Host test:** 36/36 (21 earlier + 15 new).

| Build | Source SHA-256 | App-only binary SHA-256 |
|---|---|---|
| Reviewed (superseded, never installed) | `1abae1f95da45d951d8192c37a599802c743b2c25c3ee607b9136b19fb656ad3` | `faaaa40fdc056882fe03bdb2065e3ce578070a07f33f6a17d6fa2bee7de459bb` |
| **Current** | `40bde390f0431dd446cc5b62c67bea907dc623797ae3d2df725f8bcbe64898c0` | `3196bffdf19fd4902ae3247313f95ca661a76e5190eb07483f3fadb2a47730e0` |

## 11. Hardware test (2026-10-13) and revision r2 (reconnection diagnostics)

### 11-1. Hardware results (author; Cardputer ADV + U135, macOS)

| # | Result |
|---|---|
| B1 | **PASS**: `BLE ADVERTISING`; "MediaJog BLE" visible on the Mac |
| B2 | **PASS**: first pairing; `BLE READY` / `LINK READY` |
| B3 | **PASS**: CW / CCW move the TextEdit cursor right / left over BLE (no USB cable) |
| B4 | **PASS**: one space per press; a 2–3 s hold still gives one space; no unintended repeat |
| B6 | **PASS**: Cardputer OFF → ON reconnects without re-pairing; rotation works |
| **B7** | **FAIL**: Mac Bluetooth OFF → Cardputer shows `LINK LOST`. Bluetooth ON → the Mac lists "MediaJog BLE" as connected, the Cardputer stays at `BLE SUBSCRIBING` and shows `SKIP (no link)`; rotation does nothing. In this state, "Disconnect" in the Mac's Bluetooth settings: the Cardputer shows `BLE ADVERTISING` briefly, then `BLE SUBSCRIBING` again, and the Mac shows "connected" again shortly afterwards |
| B5, B8–B15 | Not reported |

The uploaded binary's SHA-256 was not reported.

### 11-2. Facts (source, headers, symbols; not hardware)

- **What `BLE SUBSCRIBING` means in BLE-1:** connected **and** encrypted (`ble_gap_conn_find()` → `sec_state.encrypted`) **and not** `kbSubscribed`.
  - `kbSubscribed` is set only by `onSubscribe()` of the keyboard input report, for the current connection handle, from a NimBLE `BLE_GAP_EVENT_SUBSCRIBE` (library path: `BLEServer::handleGATTServerEvent` → `BLECharacteristic::setSubscribe()` → `onSubscribe()`).
  - It is cleared on connect and on disconnect. There is no other path and no timeout.
  - So in B7 **no keyboard subscribe event with the notify bit arrived for the current connection.** BLE-1 then waits indefinitely, by design.
- **`SKIP (no link)`** (`pushEvent()`): shown for any input while `linkReady` is false. In BLE-1 it does **not** distinguish "not connected" from "connected but no CCCD". This is a misleading label, not the cause.
- **Bit test:** `NIMBLE_SUB_NOTIFY` = 0x0001 (`BLECharacteristic.cpp:66`), so BLE-1's `subValue & 0x0001` is correct. B2 and B6 also prove the path works.
- **NimBLE (esp-nimble 5c1a43c, the version in ESP-IDF 5.5.4):**
  - **Disconnect order:** `ble_gap_conn_broken()` calls `ble_gatts_connection_broken()` (SUBSCRIBE events with reason **TERM**) **before** the connection is deleted and **before** `BLE_GAP_EVENT_DISCONNECT`.
  - **On encryption:** `ble_gap_enc_event()` delivers `ENC_CHANGE` first. Then, **only if the security was restored from a stored bond**, it calls `ble_gatts_bonding_restored()`, which reads the stored CCCDs and reports each as SUBSCRIBE reason **RESTORE** (prev 0 → stored flags).
  - **After a fresh pairing**, `ble_gatts_bonding_established()` runs instead: it stores the current CCCDs and emits **no** RESTORE.
  - **Storage key:** CCCDs are stored and looked up under `conn->bhc_peer_addr` (type converted to identity).
  - **When a record is stored or deleted:** a record is written only for a bonded connection whose CCCD value changed; **writing 0 deletes the record**.
- **Identity keys:** `MYNEWT_VAL_BLE_LL_CFG_FEAT_LL_PRIVACY` = 1 in this build (`esp_nimble_cfg.h`), so `BLEDevice::init()` requests identity-key (IRK) distribution in both directions.
- **BLE library on disconnect:**
  - It calls `BLESecurity::resetSecurity()`, which only clears two library flags.
  - It calls `resetGATT()` only if services changed after the GATT server started; not the case in BLE-1, where all services are created before the first advertising.
  - It restarts advertising through `advertiseOnDisconnect(true)` → `BLEDevice::startAdvertising()`. This is the same path as at boot, with the same GAP callback (`BLEServer::handleGATTServerEvent`).
- **Repeat pairing:** if a bonded peer pairs again, the library deletes that peer's bond (`ble_store_util_delete_peer`, which includes its CCCD records) and continues the pairing (`BLEServer.cpp:968-987`).
- **Stale library state:** the library keeps a per-characteristic list of subscribed connections (`m_subscribedVec`). It removes an entry only on a SUBSCRIBE event with value 0. This affects `notify()` only, not `kbSubscribed`.

### 11-3. Hypotheses (not established; each needs the r2 trace)

| ID | Hypothesis | Consistent with B6 PASS / B7 FAIL because … | r2 evidence that would confirm it |
|---|---|---|---|
| H1 | The stored CCCD is not found on reconnection: the lookup key (`bhc_peer_addr`) differs from the key used when it was stored (e.g. a different address form after the Mac's Bluetooth restart), so no RESTORE. macOS does not rewrite the CCCD because it expects the server to keep it for a bonded client | After a Cardputer restart the Mac keeps its address; after its own Bluetooth restart it may not | `ENC … cc0/0/n` with n ≥ 1 (records exist for some peer, none for this one); address type/bytes differ from the first connection; `E0` without `SkR1` |
| H2 | macOS unsubscribed (wrote CCCD 0) while turning Bluetooth off. NimBLE then deleted the stored record, and macOS does not subscribe again after Bluetooth on | The Cardputer restart in B6 involves no Mac-side unsubscribe | `SkW0` before `D…` when Bluetooth is turned off; afterwards `cc0/0/0` |
| H5 | After Bluetooth ON, the Mac pairs again instead of resuming encryption. The library's repeat-pairing handling deletes the old bond with its CCCDs, the new bond starts without CCCDs (no RESTORE), and the Mac does not write the CCCD | B6 resumed encryption with the existing keys | `RP` in the trace; `E0` without `SkR1`; `cc0/0/0` |
| H3 | A subscribe event arrives but for another connection handle | — | `fs` > 0 together with `Sk?1` |

None of these is established. The fix depends on which one the trace shows, and no fix that changes behavior is made before that.

### 11-4. r2 changes (`MediaJog_BLE_1_r2/`, BLE-1 unchanged)

| Change | Reason |
|---|---|
| **GAP event listener** (`ble_gap_event_listener_register`, read-only). Records CONNECT, DISCONNECT (HCI reason), ENC_CHANGE (status), SUBSCRIBE (keyboard `k` / mouse `m` / other `x`; reason W/T/R; notify bit), REPEAT_PAIRING, IDENTITY_RESOLVED in a 32-entry ring. `loop()` logs each event on Serial and shows the last 6 tokens: line `EV`, e.g. `EV C1 E0 SkR1 SmR1` | Shows which of H1/H2/H3/H5 happens. Independent of the library's own callbacks |
| **Encryption snapshot** (host task, at each successful `ENC_CHANGE`, which is delivered before RESTORE). It records: bonded flag, peer identity and over-the-air address types and last 2 bytes, and how many CCCD records NimBLE has stored for this peer (all / keyboard) and for any peer (keyboard), plus the bond count. Line `ENC id<t>..<xx> ota<t>..<xx> cc<peer>/<peer-kb>/<any-kb> b<bonds>`; full addresses on Serial | Shows whether a restorable record exists for the key NimBLE uses |
| Connection handle set to "none" on disconnect; a subscribe event for any other handle is ignored and counted (`fs`) | Source-level robustness; makes H3 visible |
| SKIP text: `SKIP (no link)` / `SKIP (not encrypted)` / `SKIP (no CCCD)` | `SKIP (no link)` was shown in B7 although the link was up |
| Build tag `BLE-1r2`; the static "USB: power + serial only" line was replaced by the two diagnostic lines | Screen space |

**Unchanged:** security settings (Just Works, bonding, SC), bonding/NVS behavior (no erase, no bond deletion by the sketch), HID reports, READY condition, all-up handling, queue, U135 code. The listener and snapshot only read (`ble_gap_conn_find`, `ble_store_read_cccd`, `ble_store_util_count`); store reads run in the host task. Diff vs BLE-1: 268 insertions, 12 deletions.

### 11-5. Build and hashes (2026-10-13)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, 0 sketch warnings. Flash 864,287 B (65%), RAM 66,044 B (20%). Guards unchanged |
| Source `MediaJog_BLE_1_r2/MediaJog_BLE_1_r2.ino` | 46,532 B, SHA-256 `8e5f824bb27de0f8a497a76a08c8795b26ed57f6be34a6e74e45a6721c7943a8` |
| App-only binary `MediaJog_BLE_1_r2/build/MediaJog_BLE_1_r2.ino.bin` | 864,432 B, SHA-256 `69962f8268eedf233268b7ff5b2e721420d42e99e9504e239cbe801682af488c` |
| Host test | ✅ 48/48: the 36 BLE-1 tests, plus 12 r2 tests: subscribe for no / another connection ignored and counted; the B7 state (encrypted, no CCCD) stays not READY with `SKIP (no CCCD)` and no timeout; a subscribe event on the current connection gives READY with all-up first and no replay; TERM → link lost; SKIP texts; trace tokens; trace order and overflow. `onGapEvent()` / `countCccd()` use NimBLE structures and are compile-checked only |

### 11-6. Hardware procedure for r2 (diagnosis; B7 is NOT claimed fixed)

Install `MediaJog_BLE_1_r2.ino.bin` and record its SHA-256 (`69962f82…f488c`). The existing bond can be kept. At each step, photograph the `EV` and `ENC` lines; a Serial log (USB CDC, 115200) is helpful but optional.

1. **Baseline (B6 path):** restart the Cardputer and wait for `BLE READY`. Expected trace: `C… E0 SkR1 …` (restore) or `… SkW1` (the Mac writes). Note the `ENC` line.
2. **B7:** turn the Mac's Bluetooth OFF and note `EV` (is there a `SkW0` before `D…`? which `D` reason?). Turn it ON and note `EV` (`RP`? `E0`? any `Sk…`?), the `ENC` line (`cc` values, address types/bytes compared with step 1) and `fs`.
3. If stuck, use "Disconnect" in the Mac's settings once and note `EV` / `ENC` again.
4. Quick regression: rotation and press in TextEdit after step 1.

| Observation | Points to |
|---|---|
| `SkW0` before `D…`, then `cc0/0/0` | H2 |
| `RP` after Bluetooth ON, `E0` without `SkR1`, `cc0/0/0` | H5 |
| `cc0/0/n` (n ≥ 1) and different address type/bytes than in step 1, `E0` without `SkR1` | H1 |
| `Sk?1` and `fs` increasing | H3 |
| Something else | New finding; report the trace |

### 11-7. Residual risks

- The B7 failure persists in r2 by design; r2 only diagnoses it.
- The listener runs in the NimBLE host task. Its store reads are read-only, but they are additional work in that task (bounded: at most 3 × 32 record reads per encryption).
- macOS behavior (unsubscribe on Bluetooth off, re-pairing, CCCD rewrite) is unverified.
- Remaining checklist items (B5, B8–B15) are still to be run.

## 12. r2 diagnosis of B7 (2026-10-13) and revision r3

### 12-1. Hardware observations with r2 (author; transcribed from photos)

| Moment | Screen |
|---|---|
| A. After start (B6 path) | `BLE READY` / `LINK READY`, h1, bond=1, kb=1; `EV C1 E0 SkR1 SkR1`; `ENC … cc3/1/1 b1` |
| B. Mac "Disconnect" of MediaJog BLE | Reconnected immediately (as in §11-1) |
| C. Mac Bluetooth OFF → ON | Mac: "connected". Cardputer: `BLE SUBSCRIBING`, `SKIP (no CCCD)`, h1, kb=0, ms=0 |
| C. Final photo | `EV E0 SkR0 SkR1 SkR1 C1 E!0D`; `ENC id0..D0B0 ota0..D0B0 cc3/1/1 b1` |

The author notes that the token details are transcribed from a photo.

### 12-2. What the r2 tokens and lines mean (r2 source)

- **`EV`** = the last 6 events, **oldest first**, from all connections.
- **`E!0D`** = `BLE_GAP_EVENT_ENC_CHANGE` with a **non-zero status whose low byte is 0x0D**. r2 printed only `status & 0xFF`, so the full code is lost.
  - Candidates by definition: `0x00D` = `BLE_HS_ETIMEOUT`; `0x20D` = HCI 0x0D `BLE_ERR_CONN_REJ_RESOURCES`; `0x50D` = peer SM error 0x0D (`BLE_SM_ERR_ALREADY`); `0x40D` = local SM error 0x0D.
  - `0x10D` (ATT) is not a status `ENC_CHANGE` carries.
  - esp-nimble 5c1a43c, `ble_sm.c`: the status of `ENC_CHANGE` is `BLE_HS_ETIMEOUT` from `ble_sm_timer()` (SM procedure not completed within `BLE_SM_TIMEOUT_MS` = 30 000 ms), an HCI error, a peer SM error, the result of the start-encryption command, or `BLE_HS_ENOTCONN`.
  - **Which one occurred cannot be decided from r2's screen.**
- **`SkR1` / `SkR0`** = SUBSCRIBE event for the keyboard input value handle, reason RESTORE, `cur_notify` 1 / 0. NimBLE emits RESTORE only after a successful encryption that restored a stored bond (`ble_gap_enc_event`: status 0 and `security_restored`).
- **`ENC` line** = snapshot taken **only at a successful** `ENC_CHANGE` (r2 `onGapEvent`: `status == 0`). In the final photo it therefore belongs to the **last successful encryption, not to the connection that ended with `E!0D`**.
  - `id0..D0B0 ota0..D0B0`: peer identity and over-the-air addresses are both type 0 (`BLE_ADDR_PUBLIC`) with the same bytes.
  - `cc3/1/1`: at that moment NimBLE stored 3 CCCD records for this peer, 1 of them for the keyboard input report, 1 keyboard record for any peer.
  - `b1`: one bond.
- **`SUBSCRIBING`** = connected and the **current** handle is encrypted and `kbSubscribed` is false.
- **The library's `onConnect()` (BLE-1) clears `kbSubscribed`.** NimBLE calls listeners **before** the connection callback for SUBSCRIBE events (`ble_gap_subscribe_event`).

### 12-3. Findings

- **kb=0 despite `SkR1` in the history:** the `SkR1` tokens come **before** the last `C1`. That connect cleared `kbSubscribed` (BLE-1 design), and **no SUBSCRIBE event of any reason arrived after `C1`**. The only event after `C1` is the failed `ENC_CHANGE`. NimBLE does not run `ble_gatts_bonding_restored()` for a failed encryption, so no RESTORE could follow. This chain is consistent with kb=0 and with the source.
- **Address-key hypothesis H1 weakened:** the snapshot shows a public peer address with identical identity and over-the-air values. A public address does not change when the Mac restarts Bluetooth. (The snapshot is from the earlier successful encryption.)
- **Not explained by the visible tokens (needs r3):**
  1. The link is shown as encrypted although the last `ENC_CHANGE` after `C1` failed. NimBLE sets `encrypted` only on a successful HCI encryption change.
  2. The last `C1` follows earlier events with **no `D…` (disconnect) and no `C!…` in between**. The library restarts advertising only after a disconnect or a failed connect, and NimBLE reports both to listeners. So either the transcription differs, or the earlier events and `C1` belong to different, overlapping connections (the build allows up to 3).
  3. `SkR0`: a RESTORE with the notify bit clear is unexpected for a notify-only characteristic.
- **Handle reuse:** handle 1 was used at start and again for `C1`. r2 cannot separate connections by handle; r3 adds a connection generation.

**Status: root cause not established.** The most supported chain is: the security procedure on the reconnection failed (`E!0D`) → no CCCD restore → no keyboard subscription → no HID. Why the security procedure failed is open.

Relevant source fact for that question: `BLESecurity::m_forceSecurity` defaults to `true`, so `BLEServer` calls `ble_gap_security_initiate()` on **every** connection. As a peripheral, that sends an SMP Security Request and creates an SM procedure with the 30 s timeout. Whether this is involved, i.e. whether `E!0D` is `0x00D` about 30 s after the connect, is exactly what r3 shows.

### 12-4. r3 changes (`MediaJog_BLE_1_r3/`; r2 and BLE-1 unchanged)

| Change | Purpose |
|---|---|
| Records with **sequence number, time (0.1 s), connection generation (`g`), handle (`h`), full status/reason (hex, unpadded: `!D` = 0x00D, `!20D`, `!50D`), link flags at that moment (`E<encrypted>B<bonded>`)** | Separate connections despite handle reuse; full codes; check the "encrypted" contradiction |
| More NimBLE events: `LEST` (link established), `PAIR` (fresh pairing complete), `MTU`, `UPD` (connection update), `TFAI`, `PKEY` | `PAIR` distinguishes a fresh pairing from a restore; `MTU`/`UPD` show the Mac's activity |
| App-level records: `aCON`/`aDIS` (library connect/disconnect as BLE-1 handled them; `aDIS` shows the handle BLE-1 considered current), `aSUB` (`ok` / `FGN` = foreign handle), `RDY`/`LOST` | Shows whether the library delivered each event to BLE-1 and what BLE-1 did with it |
| 64-record ring; the **`t` key** cycles status → trace page 1 (newest 13) → 2 → … → status. The trace is **frozen** when page 1 opens | Photographable history covering a whole B7 sequence |
| `n` = number of open connections NimBLE reports (handles 0–15) on the status page; also in the trace header | Detects overlapping connections |
| `ENC` snapshot also on failed encryption: `ENC g<gen> <ok|!code> e<encrypted> i<type><2 bytes> o<type><2 bytes> c<peer>/<peer-kb>/<any-kb> b<bonds>` | Ties the snapshot to its connection |
| Top-right **`B1r3`**, right-aligned by measured width. `BUILD_TAG` stays `BLE-1r3` and is printed on Serial at boot. r2 drew `BLE-1r2` (42 px) from x 204, past the 240 px edge | Readable build id |

**Unchanged:** security settings, bonding, NVS use (only `nvs_get_stats`), HID reports, READY rules, queue, U135. All diagnostics are read-only. Records come from the host task (listener, library callbacks) and from `loop()`; a short `portMUX` critical section protects the ring copy.

Name rule for later diagnostic builds: BLE-1 rN → screen `B1rN`. **r2 is not changed** (hardware-tested binary `69962f82…`).

### 12-5. Build and hashes (2026-10-13)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, **0 sketch warnings** (two `-Wformat-truncation` warnings during development fixed by larger buffers). Flash 869,735 B (66%), RAM 68,692 B (20%). `#error` guards effective |
| Source `MediaJog_BLE_1_r3/MediaJog_BLE_1_r3.ino` | 61,717 B, SHA-256 `0c63dcc25cbb123c42b65a13145e80c9e44311cb6c3e8ffd7b01e59cc468662c` |
| App-only binary `MediaJog_BLE_1_r3/build/MediaJog_BLE_1_r3.ino.bin` | 869,888 B, SHA-256 `2c6a0cd718da062fc29a4a2f11d860d70595b7fd42a49fdbb3d9ed556354abab` |
| Binary check | Contains `B1r3` and `BLE-1r3` |
| Diff vs r2 | 427 insertions, 83 deletions |
| Host test | ✅ 53/53: the 36 BLE-1 tests; the 9 r2 state tests; 8 r3 tests (full-code tokens with `E!D`, `E!20D`, `E!50D` distinct; `aSUB` ok / FGN with the current handle; ring keeps the newest 64 and detects overwrites; drain after overflow; freeze of 64 chronological records; 5 pages; trace lines ≤ 40 columns). `onGapEvent()`, `traceSecOf()` and the drawing are compile-checked only |

### 12-6. One-shot hardware procedure for r3 (B7 is NOT claimed fixed)

1. Install `MediaJog_BLE_1_r3.ino.bin` and record its SHA-256 (`2c6a0cd7…54abab`). Keep the existing bond. Optional but best: Serial log over USB CDC at 115200 for the whole run; it does not affect BLE.
2. Start and wait for `BLE READY`.
3. Mac Bluetooth OFF, wait about 10 s; Mac Bluetooth ON. **Then wait at least 40 s without touching anything** (a 30 s SM timeout must be able to appear).
4. Photograph the status page (`EV`, `ENC`, `n`). Press **`t`** and photograph every trace page until the status page returns.

| Trace shows | Points to |
|---|---|
| `ENC !D` about 30 s after the `CONN` of the same `g` | SM procedure timeout (e.g. the peripheral Security Request not completed) |
| `ENC !50D` | The Mac sent Pairing Failed, reason 0x0D |
| `ENC !20D` | Controller-reported HCI error 0x0D |
| `ENC ok`, no `SUB … R1` afterwards, `PAIR ok` present | Fresh pairing, no restore (H5) |
| `SUB k W0` before `DISC` | The Mac unsubscribed (H2) |
| `CONN` of a new `g` without `DISC` of the previous one, or `n2` | Overlapping connections |
| `aSUB … FGN` | Subscribe for a handle BLE-1 did not consider current (H3) |
| `E1` in records after `ENC !…` | Encrypted although the security procedure reported failure |

### 12-7. Fix options, NOT implemented (each only if the r3 trace supports it)

| If the trace shows | Minimal change to consider | Note |
|---|---|---|
| `ENC !D` ~30 s after connect, together with the Security Request | `BLESecurity::setForceAuthentication(false)`: no Security Request from the peripheral at connect; the bonded Mac starts encryption itself. Bonding, SC and Just Works unchanged; READY still requires encryption | A security-setting change; needs the evidence and Edgar's agreement |
| Encrypted, bonded, CCCD stored for the keyboard, but no RESTORE and no WRITE within a few seconds | One controlled `ble_gap_terminate()` per connection generation, so that the Mac reconnects | B shows that a Mac-initiated reconnect alone did not help, so this needs evidence |
| Overlapping connections (`n` ≥ 2) | Close the stale connection when a new one is established | Needs evidence |

## 13. r3 hardware trace (2026-10-13), cause, and revision r4

### 13-1. r3 observation (author; transcribed from photos)

Right after start (before any Bluetooth OFF/ON): `BLE SUBSCRIBING`, `SKIP (no CCCD)`, h1, encrypted, bond=1, kb=0, ms=0; trace header n1, lost0. Trace, in order:

| ≈ time | Record |
|---|---|
| 1.7 s | `ENC ok` |
| 1.7 s | `SUB … R0` |
| 1.7 s | `SUB … R1` |
| 1.7 s | `aSUB k1 FGN` |
| 1.8 s | `aCON` (the photo read "aCON ok") |
| 1.8 s | read as `LOST` (see below) |
| 2.1 s | `MTU 256` |
| 2.7 s | `UPD ok` |
| 31.8 s | `ENC !D` |

Note: r3 writes `LOST` only from `onLinkLost()`, which runs only after the link was READY, and no `RDY` was seen. In r3, `LEST ok` is the NimBLE link-established event (`BLE_GAP_EVENT_LINK_ESTAB`), which is expected right after CONNECT. The 1.8 s record is therefore most likely `LEST`.

### 13-2. Cause (confirmed in source; reproduced in a host test)

1. **When `FGN` is produced (r3 `handleSubscribe()`):**
   - when the event's connection handle differs from `bleConnHandle`. `bleConnHandle` starts as `0xFFFF` and was set **only** in the library's `onConnect()` (and reset in `onDisconnect()`).
   - At the time of `aSUB k1 FGN`, no `onConnect()` had run yet, so it held `0xFFFF`.
2. **What `onConnect()` did next (r3):**
   - `kbSubscribed = false; mouseSubscribed = false;`, then it registered handle 1.
   - The restored subscription had already been discarded and could not come back: NimBLE emits RESTORE once per encryption.
3. **Why RESTORE comes before CONNECT (esp-nimble 5c1a43c, `ble_gap.c`):**
   - For a peripheral link, `ble_gap_rx_conn_complete()` installs the connection callback (`conn->bhc_cb`) but does not post CONNECT. It sends Read Remote Version.
   - `ble_gap_rx_rd_rem_ver_info_complete()` then sends Read Remote Supported Features (slave path).
   - Only `ble_gap_rx_rd_rem_sup_feat_complete()` posts `BLE_GAP_EVENT_CONNECT` and sets `slave_conn = 1`.
   - A bonded Mac that starts encryption at once gets `ENC_CHANGE` and `ble_gatts_bonding_restored()` (the RESTORE subscribe events) delivered through the same callback first.
4. **The chain is reproduced in a host test:**
   - With r3's `handleSubscribe()` (extracted from r3) and r3's `onConnect()` body, the order RESTORE → CONNECT gives `FGN`, kb=0, state `LINK_ENCRYPTED` (`BLE SUBSCRIBING`).
   - With r4 the same order gives READY.
5. **This also explains the r2 B7 photo** `E0 SkR0 SkR1 SkR1 C1 E!0D`:
   - The `E0` and RESTORE events belong to the **new** connection and precede its `C1`; there is no missing disconnect.
   - The link is encrypted because `E0` was the successful encryption.
   - `E!0D` is the timeout below.
   - Whether the order occurs depends on how fast the Mac encrypts relative to NimBLE's feature exchange. In r2-A (start) the order was `C1 E0 SkR1`; in r3 at start, and in B7 after Bluetooth ON, RESTORE came first.
6. **`SUB … R0`:** RESTORE with the notify bit clear. One of the 3 stored CCCDs (`c3`) has no notify bit, e.g. an indication-only CCCD such as Service Changed (shown as `x`). Not relevant to the keyboard. Not verified which characteristic it is.

### 13-3. `ENC !D` (separate from the cause)

- r3 prints the full code unpadded, so `!D` = **0x00D = `BLE_HS_ETIMEOUT`** (`ble_hs.h`: 13).
- 1.7 s → 31.8 s ≈ 30 s = `BLE_SM_TIMEOUT_MS` (30 000). In `ble_sm.c`, `ble_sm_timer()` reports an expired SM procedure as `ENC_CHANGE` with `BLE_HS_ETIMEOUT`.
- **Where the procedure comes from:**
  - The library's CONNECT handler calls `BLESecurity::startSecurity()` because `m_forceSecurity` defaults to true. That is `ble_gap_security_initiate()`, i.e. a peripheral Security Request with a 30 s SM procedure.
  - Because CONNECT is posted late, this happens **after the link is already encrypted** (`ENC ok` at 1.7 s). The Mac has no reason to answer, and the procedure expires.
- **This is not an encryption failure:** the link stayed encrypted (`ENC ok` earlier). NimBLE updates `sec_state` only on HCI encryption-change events, and the timeout does not terminate the link in the code checked.
- **Effect on BLE-1:** none on READY or subscriptions. BLE-1 polls `sec_state` and does not use `ENC_CHANGE` statuses. The library calls `onAuthenticationComplete()` (log only).
- **Not changed in r4:** `setForceAuthentication(false)` would remove this redundant request. For a first pairing, the Mac would then have to start pairing itself (not verified). It is a security-behavior change and stays a separate, evidence-based decision.

### 13-4. r4 fix (`MediaJog_BLE_1_r4/`; r3, r2, BLE-1 unchanged)

| Function | Change |
|---|---|
| `adoptConnHandle(h)` (new) | If no **live** connection is registered (`bleConnHandle` is `0xFFFF`, or no longer known to NimBLE) and NimBLE knows `h` as an open link (`ble_gap_conn_find`), register `h` and start its subscription flags cleared. Records `aADP` |
| `handleSubscribe()` | Calls `adoptConnHandle()` first; the handle check stays: an event for any handle other than the registered live connection is still rejected (`FGN`) |
| `appConnect(h)` (was `onConnect` body) | Keeps the subscription flags if `h` is the handle already adopted from an early event (trace `aCON keep`); otherwise clears them as before (`aCON new`) |
| `appDisconnect(h)` (was `onDisconnect` body) | Changes the state only if `h` is the current connection |
| `appConnectFailed(h)` (new, from the GAP listener) | A connect that fails before CONNECT is posted (NimBLE reports a CONNECT event with an error) drops an adoption of `h`, so a later link reusing the handle number never inherits its flags. This is the only state change made by the listener |
| Screen | Tag `B1r4` (right-aligned as in r3); status line adds `ad` (handles adopted) |

**Unchanged:** security settings (including the Security Request at CONNECT), bonding, NVS (`nvs_get_stats` only), HID reports, READY = connected + encrypted + keyboard CCCD, all-up handling, queue, U135, diagnostics. Diff vs r3: 130 insertions, 30 deletions.

**State transitions (single connection):**
- **Start or reconnect, RESTORE first:** `ENC ok` → `SUB k R1` → `aADP` → `aSUB k1 ok` (kb=1, not yet connected) → `CONN` → `aCON keep` → `RDY`. The first report is all-up.
- **Normal order:** `CONN` → `aCON new` → `ENC ok` → `SUB k R1` → `aSUB k1 ok` → `RDY`.
- **Disconnect:** `SUB k T0` (current, accepted) → `DISC` → `aDIS` (current) → state cleared, handle none → `LOST`.
- **Events of any other open or closed handle:** `FGN`.

### 13-5. Build and hashes (2026-10-13)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, **0 sketch warnings**. Flash 870,083 B (66%), RAM 68,692 B (20%). `#error` guards effective |
| Source `MediaJog_BLE_1_r4/MediaJog_BLE_1_r4.ino` | 66,484 B, SHA-256 `28c8c5ade351c56cda8ef98ba95588ae994c0e3baba7901a16b41a233760ae52` |
| App-only binary `MediaJog_BLE_1_r4/build/MediaJog_BLE_1_r4.ino.bin` | 870,224 B, SHA-256 `02be4af083defdec297bad2375d5d010bb2a1aa43963ffc5755696c87e4e13f6` |
| Binary check | Contains `B1r4`, `BLE-1r4` |

**Host tests ✅ 70/70** (logic only):
- The 53 earlier tests. The harness's connection table now treats only registered open handles as links, so tests that set "connected" also set the handle, as `onConnect()` does.
- 17 new r4 tests:
  - RESTORE before CONNECT is adopted and kept, READY with all-up first, then Right;
  - normal order;
  - another live handle rejected; a handle that is not open rejected, not adopted;
  - CONNECT for another handle clears;
  - TERM + DISCONNECT of the current link clears; a late event for the closed handle is rejected;
  - DISCONNECT of a non-current handle leaves the current link READY;
  - a failed connect drops the adoption, and a new link reusing the handle number does not inherit it;
  - a stale registered handle lets a new live handle be adopted;
  - a failed-connect event cannot clear an established link;
  - trace tokens.
- **Reproduction:** r3's own functions with the swapped order end in FGN, kb=0, `BLE SUBSCRIBING`.

`onGapEvent()`, `adoptConnHandle()`'s use of the real NimBLE, and macOS behavior are not host-testable.

### 13-6. Hardware acceptance (to run; B7 counts as fixed only after this)

Install `MediaJog_BLE_1_r4.ino.bin` and record its SHA-256 (`02be4af0…4e13f6`); keep the existing bond.

| # | Do | Pass |
|---|---|---|
| R4-H1 | Start several times (≥ 5) | `BLE READY` each time without touching the Mac; trace shows either `aADP … aCON keep` or `aCON new … aSUB k1 ok`; never `FGN` for the keyboard |
| R4-H2 | Rotate / press in TextEdit | Right / Left / one Space as before |
| R4-H3 | Mac Bluetooth OFF, ~10 s, ON (≥ 3 times) | Returns to `BLE READY` by itself; rotation works again; nothing typed by itself |
| R4-H4 | Mac "Disconnect", then let it reconnect | `BLE READY` again |
| R4-H5 | Cardputer restart (B6) | `BLE READY` |
| R4-H6 | Note `ENC !D` ~30 s after each connect | Expected, harmless (§13-3); record if anything else follows |

### 13-7. Residual risks

- macOS timing and behavior are not host-testable; B7 is fixed only after §13-6.
- The Security Request on every CONNECT and its 30 s timeout remain (§13-3). NimBLE's "tainted connection" note after an SM timeout is not implemented in the code checked.
- Adoption relies on `ble_gap_conn_find()` from the BLE library's callback (host task); this is the same call the library itself makes there.
- If two links were ever open at once, the first one registered stays current; events of the other are rejected (FGN). BLE-1 is single-connection by design.

## 14. r4 hardware result (2026-10-13, reported by the author)

| Check (§13-6) | Result |
|---|---|
| Start → `BLE READY` | **PASS** (first start) |
| Mac Bluetooth OFF → ON | **PASS: 3 times in a row**, automatic reconnection |
| Rotation after reconnection | **PASS** (left/right) |
| Cardputer restart → automatic connection | **PASS**; rotation works afterwards |
| Knob press | **PASS**: one Space per press |
| Not reported | number of start repetitions (the §13-6 target was ≥ 5), Mac "Disconnect" (R4-H4), uploaded SHA-256 |

**B7 is fixed on hardware with r4** for the reported cases. The main BLE-1 functions passed. Next step: USB/BLE integration (`docs/STAGE_2A_DESIGN.md`, design only).

## 15. Correction (2026-10-13): the mouse report characteristic was never registered

§1 and §3 state that BLE-1 has a mouse input report characteristic. **That is not the case.** With the 3.3.9 BLE library in its NimBLE build, `BLEService::addCharacteristic()` does not register a second characteristic with the same UUID in a service. `BLEHIDDevice::inputReport(2)` (and `outputReport(1)`) use the same UUID 0x2A4D as `inputReport(1)`, so only the keyboard input report exists in GATT; the mouse handle is 0xFFFF.

- BLE-1 never sent a mouse report on hardware, so its tests were not affected.
- Found in the Stage 2a hardware test (`STAGE_2A.md` §10).
- Test of a fix: `BLE_M1.md`.
