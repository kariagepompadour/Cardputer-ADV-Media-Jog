# Stage 2a-2 — Stage 2a with the BLE-B1 GATT table

Created: 2026-10-14 · Implementation: Claude Opus 5.5 · Decision basis: Edgar (`STAGE_2A_2_DESIGN.md` approved with decisions 1–6, below)
**Status: built and host-tested. NOT installed on hardware.**
**Status update (2026-10-14): main functions hardware-verified** (USB and BLE keyboard, BLE mouse with drag, switching in both directions, reconnection). This is not exhaustive. Two known open issues remain. See §8; the line above is the status at build time.
- Stage 2a, BLE-B1, BLE-1 r4, BLE-M1, BLE-M1 r2 and 1e-4, with their binaries, are unchanged (their `SHA256SUMS.txt` verify).
- No NVS erase, no bond deletion, no Mac re-pairing.

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

## 1. Edgar's decisions (2026-10-14)

1. Folder `MediaJog_2a_2`, build id `2a-2`, screen tags `USB 2a2` / `BLE 2a2`.
2. **CCCD limit: option (a).** The BLE-B1 table is not changed. **Supported: at most 2 BLE hosts.**
   - **Known limitation:** NimBLE stores 3 CCCD records per host (keyboard, mouse, battery), with a maximum of 8 (`CONFIG_BT_NIMBLE_MAX_CCCDS`).
   - When a third bonded host writes its CCCDs, the library's store-full handler (`ble_store_util_status_rr` → `ble_gap_unpair_oldest_except`, source-checked) can **delete the oldest other host's bond**.
3. BLE-B1's permissions (no `READ_ENC`), PnP ID byte order and read-only descriptors are kept.
4. B7 handling, GATT registration order, Report Map and CCCD subscribe handling are identical to BLE-B1.
5. Existing builds and binaries are unchanged.
6. The Mac's existing bond, made with BLE-B1, is reused. Re-pairing is not part of the initial test.

## 2. What changed (Stage 2a → 2a-2)

**Function comparison:**
- 130 functions are identical to Stage 2a.
- **Changed (4):** `notifyReport`, `bleBegin` (both replaced by BLE-B1's, verbatim), `drawBuildTag` (`SCREEN_TAG "2a2"`), `drawConnScreen` (help on one line; the bottom line shows the live `ki / ko / mi` handles, or `GATT REG rc<n>` in red).
- **Removed:** `ReportCallbacks` (with `BLEHIDDevice` and the three `BLECharacteristic*`).
- **New (5, from BLE-B1 verbatim):** `gattConstAccess`, `gattValueAccess`, `setGattValue`, `registerGattTable`, `onSubscribeEvent`.
- **Also:** the `GattValue` objects with the same names `kbInput` / `kbOutput` / `mouseInput` (so `bleSendKeyboard` / `bleSendMouse` are unchanged); the notify status state; the two includes `host/ble_gatt.h` and `host/ble_hs_mbuf.h`; `BUILD_TAG "2a-2"`; the header comment.

**Unchanged:**
- USB backend, transport switching (state machine, 200 ms + 100 ms, 300 ms bound), `releaseOwed[]` and `noteReleaseSent`;
- profiles, CUSTOM, HOLD, SETUP, CONNECTION logic;
- NVS settings and the link record (`MJLK` v1);
- the 2a trace formatter;
- lazy BLE start (`bleBegin()` once per boot, from `bleStart()`).

**B7 and registration (decision 4) ✅:**
- `connAlive`, `adoptConnHandle`, `appConnect`, `appConnectFailed`, `appDisconnect`, `handleSubscribe`, `ServerCallbacks`, `SecurityCallbacks`, `onGapEvent` and `countCccd` are byte-identical to BLE-1 r4 **and** BLE-B1.
- **GATT identity script** (`gatt_identity.py`): it compares each of the following with BLE-B1, and all are **IDENTICAL**:
  - the `GattValue` objects, the notify state, the table with its access callbacks, `registerGattTable` and `onSubscribeEvent`;
  - `notifyReport`, `bleBegin` (registration order), `HID_REPORT_MAP`, `REPORT_ID_*`, `BLE_DEVICE_NAME`.

## 3. Host tests ✅ (logic only, not hardware)

**Model:** BLE-B1's NimBLE C API stubs, plus Stage 2a's USB, NVS and UI stubs.
- Each report is a GATT value handle, and notifications are recorded per handle.
- Per-handle rejections replace the old `rejectNext`.
- A registration error can be injected.

| Suite | Result |
|---|---|
| Stub handles: BLE-B1 and 2a-2 (boot in BLE) | **Identical**: protocol mode 13, control point 11, kb in 15 (CCCD 16), kb out 19, mouse in 22 (CCCD 23), battery 27 (stub numbering, not the device's) |
| Stage 2a end to end, ported to the new model | **85/85** |
| New 2a-2 scenarios N01–N08 | **32/32** |
| NVS suite (1e-4) on 2a-2's functions | **47/47** (all 13 functions identical to Stage 2a) |
| HOLD suite (1e-3) on 2a-2's send path | **16/16** (all 11 functions identical to Stage 2a) |
| Duplicate-UUID regression | BLE-1 r4 and Stage 2a still give mouse 0xFFFF |

The ported Stage 2a suite changed only mechanically:
- the reports' sent lists are now per handle;
- `simSubscribe` became `simSubscribeH(conn, handle)`;
- the stored-CCCD handles are the table's (15 / 22);
- the tag strings are `2a2`.

Every scenario and expectation is otherwise the same. With this model, its BLE mouse checks (S14, S23 …) run against a mouse report that really has a handle; the old 2a stub hid the duplicate drop.

**New scenarios:**

| Scenario | Checks |
|---|---|
| N01 | Boot in USB: nothing registered, CONNECTION shows `kiFFFF…`. First switch to BLE: one `count_cfg`, one `add_svcs`, one server start; handles = BLE-B1's; one HID service, no library service; Report Map = r4. BLE → USB → BLE ×3: still exactly one registration and the same handles. The CONNECTION screen shows `kiF ko13 mi16` |
| N02 | **B7 order** (ENC + RESTORE of both CCCDs before CONNECT): adopted, kb=1, **ms=1**, READY after CONNECT; all-up and released first; trace SUB → aSUB; no mbuf leak |
| N03 | **BLE → USB switch while dragging:** the BLE mouse gets the button-up (and the keyboard all-up) before the commit; no warning; within the bound; USB starts released; the old knob release presses nothing |
| N04 | The same with the BLE mouse release rejected: committed with BLE owed (`!`). Back on BLE (RESTORE first): button-up and all-up first, then owed cleared; no button-down |
| N05 | HOLD Shift over BLE: released on link loss; after reconnection all-up first, no Shift |
| N06 | SUBSCRIBE for an unknown connection ignored (library check); battery CCCD ignored; LED write accepted; mouse CCCD off leaves the keyboard READY |
| N07 | Injected registration error: `GATT REG rc6` on CONNECTION, handles FFFF, never READY on BLE; USB unaffected |
| N08 | BLE start, pairing and CCCD writes: the app writes only the link record (NimBLE's own bond store is separate) |

**Mutation checks (both suites; each must fail):**

| Mutant | Failing checks |
|---|---|
| m1: table registered again on every `bleStart()` | 1 |
| m2: subscribe listener after the trace listener | 1 |
| m3: listener without the `conn_find` check | 2 |
| m4: mouse CCCD sets the keyboard flag | 14 |
| m5: `switchMouseNeeded()` ignores the BLE mouse | 1 |
| m6: owed release ignores the mouse half | 5 |
| m7: table queued after `bleServer->start()` | 43 |
| m8: mouse report removed from the table | 17 |

## 4. Build (2026-10-14)

| Item | Result |
|---|---|
| Clean build | ✅ exit 0, **0 sketch warnings**; `#error` guards effective (hwcdc: USB Mode; CDC off: CDC On Boot). Flash 889,511 B (67 %), RAM 69,436 B (21 %); Stage 2a was 896,179 B |
| Source `MediaJog_2a_2/MediaJog_2a_2.ino` | 169,738 B, SHA-256 `9ec07fa4ad6bea1053d7bd4b2b15ce46d4e62e27e2755a62892bc19a112fbbd9` |
| App-only binary `MediaJog_2a_2/build/MediaJog_2a_2.ino.bin` | 889,664 B, SHA-256 `4339eed644f01468548f6444501c287e74c14e5bd1d1b5de1448abdd42a8019b`; contains `2a-2` and the CONNECTION handle line |
| Host-tested source | Equals the final preprocessed source |

## 5. Hardware procedure (after Edgar's review; not run)

**Rules:**
- No NVS erase, no bond deletion, **no Mac re-pairing in this test** (decision 6).
- Record the SHA-256 of the installed binary (`4339eed6…`).
- **After 2a-2 is installed, do not go back to Stage 2a, BLE-1 r4 or the BLE-M1 builds.** Their GATT tables differ from the one the Mac now caches.
- If the Mac does not subscribe (`kb=0` or `ms=0`), stop and report the trace (CONNECTION `t`).

| # | Test | Pass |
|---|---|---|
| H0 | Install. Boot (the stored transport from 2a is used; else `Fn+B` → 2). CONNECTION bottom line `ki… ko… mi…` | Tag `BLE 2a2`. **Handles equal to the values BLE-B1 showed** (please give B1's `ki / ko / mi` for comparison). The Mac reconnects with the existing bond: `bond=1 kb=1 ms=1`, READY |
| H1 | USB regression: Profiles 1–3, HOLD, CUSTOM, a settings change and restart, USB unplug / replug | As 1e-4 / 2a |
| H2 | BLE: keyboard profiles; Profile 2 (mouse) movement and drag | Pointer moves; drag works; keys work |
| H3 | Switching without restart, BLE ↔ USB ×5 each way, also **while dragging** (knob held) in both directions | No stuck key or button on either host; `!` only if a release could not be sent, and it clears later |
| H4 | B7 regression on BLE: Mac Bluetooth OFF → ON ×3, Cardputer restart ×2, boot in USB then switch to BLE | READY with `kb=1 ms=1` each time |
| H5 | Transport restore after restart (both ways); NVS statistics stable | As 2a |
| — | More than 2 BLE hosts | Not supported (§1, decision 2) |

## 6. Hardware H0 result 1: no BLE connection (2026-10-14; analysis only, no code change)

**Observed (author):**
- 2a-2 installed (app-only, Launcher Wi-Fi). CONNECTION shows `BLE 2a2`; BLE selected.
- The screen stays `BLE ADVERTISING`, with `ki10 / ko21 / mi24` (= BLE-B1). `kb=0 ms=0`.
- On the Mac, "MediaJog BLE" is under My Devices, "Not Connected". Mac Bluetooth OFF → ON and a Cardputer restart did not change this.
- The screen showed `CFG RESTORED` and `SKIP (no link)`.
- No unpairing, no NVS erase, no re-pairing.

### 6.1 Source comparison, BLE-B1 vs 2a-2 (connection path)

| Part | BLE-B1 | 2a-2 | Same? |
|---|---|---|---|
| `bleBegin()`: init, listeners (subscribe, then trace), security, `createServer`, `advertiseOnDisconnect(true)`, table, one server start, advertising data (appearance 0x03C1, UUID 0x1812, scan response, intervals 0x06 / 0x12), `startAdvertising()` | called once in `setup()` | called once, from `bleStart()` | **byte-identical** (function comparison and identity script) |
| When it runs | `setup()`, always | `setup()` if the stored transport is BLE (after `loadSettings`, `loadLink`, display and `USB.begin()`); otherwise at the first switch to BLE | Different caller only. `BLEDevice::init()` waits for host sync (`while (!m_synced)`, `BLEDevice.cpp:443`) in both, so advertising is not started before sync |
| Advertising stop / restart | Never stopped; the library restarts it after a disconnect | `bleStop()` (stop advertising, `advertiseOnDisconnect(false)`, terminate) is called **only** in `commitSwitch()` when leaving BLE. `bleStart()` re-enables both. Nothing else calls `stopAdvertising` or `ble_gap_terminate` | Same while BLE stays selected and no switch is made |
| GAP connect / disconnect, B7 (`appConnect`, `appConnectFailed`, `appDisconnect`, `adoptConnHandle`, `connAlive`, `handleSubscribe`), `ServerCallbacks`, `SecurityCallbacks`, trace listener, subscribe listener, `updateLinkState` | — | — | **byte-identical** |
| Encryption, bond restore, CCCD restore | NimBLE + library (same core, same bond store) | same | same |
| `onLinkUp` / `onLinkDown`, `onLinkReady` / `onLinkLost`, switch state machine | B1's minimal versions | Stage 2a's (transport hooks) | Different, but they act only **after** a connection exists (READY / link loss) or on a user switch. None of them touches advertising or rejects a connection |

**Conclusion from source:**
- While 2a-2 boots in BLE and nobody switches, the code from power-up to an incoming connection is the same as BLE-B1's.
- The transport layer cannot block an incoming CONNECT: it calls nothing on the GAP layer except `bleStop()` / `bleStart()` at a switch.
- **No firmware defect in the advertising or connection path is visible in the source.**

### 6.2 What the screen tells, and what it does not

| Shown | Source | Meaning |
|---|---|---|
| `CFG RESTORED` | `loadSettings()` in `setup()` sets `lastEvent` from the NVS result (`CFG_OK`) | **Boot**: the user settings were read correctly from NVS. Not related to BLE. `lastEvent` is a single slot and keeps it until something else overwrites it |
| `SKIP (no link)` | `pushEvent()`: knob input while not READY; `linkState` = ADVERTISING (no connection registered) | **The user turned or pressed the knob** while no BLE connection existed. It records the knob input, not a connection attempt |
| `BLE ADVERTISING` | `linkStatusText()`: `bleStarted` and `linkState` = ADVERTISING, i.e. `bleConnected == false` | No connection is registered. **It does not prove that advertising is on the air**: the result of `startAdvertising()` is not checked or shown (`BLEDevice::startAdvertising()` returns `void`), and `ble_gap_adv_active()` is not displayed |
| `kb=0 ms=0` | CCCD flags of the **current** connection | Zero because there is no connection; says nothing about the stored CCCDs |
| `ki10 / ko21 / mi24` | live `val_handle`s | The table is registered exactly as in B1 (`gattRegisterRc == 0`) |

**Not known from these screens:**
- whether any GAP event (CONNECT, ENC, DISCONNECT, SUBSCRIBE) happened since boot. The trace page (CONNECTION → `t`) has this, but it was not reported;
- whether advertising is really active;
- whether the Mac tries to connect at all (Mac side);
- the number of stored bonds (`b` field, filled only at an encryption event).

### 6.3 The six possibilities

| # | Possibility | Status |
|---|---|---|
| 1 | Mac sends no connection request | **Most consistent with all facts.** The same symptom appeared once before with an unchanged, previously working build (BLE-1 r4 after the BLE-M1 r2 failures, `BLE_M1.md` §10: advertising, Mac "Not Connected", no attempt). It ended with the B1 pairing and was never explained |
| 2 | Cardputer advertises but the Mac cannot connect | Not excluded: advertising state is not observable (6.2). Same code as B1, so no source reason |
| 3 | Request arrives, GAP connect fails | Would leave `CONN !<code>` (failed connect) or `CONN ok`/`DISC` in the trace. Unknown until the trace is read |
| 4 | Connects and drops at once | Would leave `CONN`/`DISC` pairs in the trace, and connection counters would rise. Unknown |
| 5 | Bond, encryption or CCCD restore problem | Would need a connection first (`ENC !…`, `SUB` lines). Nothing suggests a connection happened. The bond store was not touched by any code path (`BLE_M1.md` §10) |
| 6 | 2a-2 transport control blocks the connection | **Not supported by the source** (6.1). The only GAP calls of the transport layer are at a user switch |

**Most suspicious, not confirmed:** (1), the Mac does not start a connection to the advertising device. Next comes (2), advertising not actually on the air, which no screen shows. Both are visible to the test below.

### 6.4 Information needed

1. The **trace page** (CONNECTION → `t`, all pages): whether any `CONN`, `LEST`, `ENC`, `DISC`, `SUB` exists since boot, with times.
2. The CONNECTION line `BLE bond kb ms h ad fs`, the trace header `n…` (open connections) and `lost`.
3. The Mac's reaction to an explicit **Connect** request.
4. Optional, read-only: the Serial log (115200) with `[GAP] … rc`, `[BLE] advertising; gatt rc …` and every `[TRACE]` line. Also, on the Mac, the Bluetooth system log for the time of the test (`log show --last 5m --predicate 'subsystem == "com.apple.bluetooth"'`); this shows whether the Mac tried.
5. Whether the Cardputer's USB-C is connected to the same Mac during the test. 2a-2 enumerates USB HID; B1 did not. No source path links this to BLE, but it is a difference in the setup.

### 6.5 Proposed next hardware test (one test; bond kept, nothing erased)

**"Explicit Connect from the Mac, with the trace before and after."**

1. Run 2a-2 in BLE as now. **Do not switch transport** during the test. Note whether the USB-C cable is connected to the Mac.
2. Baseline: `Fn+B` (CONNECTION). Photograph the screen (line `BLE bond… kb… ms… h… ad… fs…`, the `sw/warn` line, `ki/ko/mi`). Press `t`: photograph every trace page, including the header (`n`, `h`, `@time`, `lost`). Close with `` ` `` (esc).
3. Mac: System Settings → Bluetooth → "MediaJog BLE" → **Connect** (from the (i) / context menu). **Not "Forget".** Note what the Mac shows (connecting spinner, "Connected", or an error) and the time.
4. Wait 30 s. Cardputer: `Fn+B` → `t` (a new frozen copy). Photograph all trace pages and the CONNECTION screen again.
5. Optional, read-only: Mac Terminal `log show --last 5m --predicate 'subsystem == "com.apple.bluetooth"' > ~/Desktop/bt.log` right after step 4. Serial log if a terminal was attached.

**Judgment:**

| Trace after Connect | Meaning | Next (to Edgar) |
|---|---|---|
| `CONN ok` → `ENC ok` → `SUB k R1`, `SUB m R1` → READY, `kb=1 ms=1` | Firmware, bond and CCCDs are fine; the Mac had only not started the connection by itself (1) | H0 passes with a manual Connect; then observe whether auto-reconnect works (Bluetooth OFF→ON, restart) |
| `CONN ok`, `ENC ok`, no `SUB` or `SUB … R0` | Encrypted with the existing bond, CCCDs not restored (5) | Check `ENC … c<peer>/<peer-kb>/<any-kb> b<n>` (stored CCCDs for handle 0x10) |
| `CONN ok` then `ENC !<code>` or `PAIR` | Key or bond problem (5): `!206` = key missing; `0x3xx` / `0x4xx` = pairing error | Report codes; no re-pairing without approval |
| `CONN ok` then `DISC 213` before `ENC` (repeated) | The Mac connects and drops (4), as BLE-M1 r2 | Report; compare with the Mac log |
| `CONN !<code>` | Connect failed in GAP (3) | Report code |
| **No new event at all**, Mac shows an error or keeps trying | Nothing reached the Cardputer: (1) or (2). The Mac log separates them (whether the Mac scanned and saw the device) | Report; the next step is a read-only check of advertising (a diagnostic that shows `ble_gap_adv_active()` and the `startAdvertising` result, decided by Edgar) |

## 7. Hardware test 1 (author, 2026-10-14): results

**Environment:**
- M5Stack Cardputer ADV + Unit Encoder U135; MacBook Pro (Apple Silicon).
- `MediaJog_2a_2`, build id `2a-2`, BLE name "MediaJog BLE", Profile 1 YOUTUBE.
- App-only install through the Launcher; no NVS erase.

**Passed (hardware ✅; one run each unless stated):**

| # | Test | Result |
|---|---|---|
| 1 | BLE new pairing (after the Mac-side unpairing, §7.1) | PASS |
| 2 | BLE connection: `BLE READY` / `LINK READY` | PASS |
| 3 | YouTube profile over BLE | PASS |
| 4 | Mac Bluetooth OFF → ON, automatic reconnection | PASS (1/1) |
| 5 | Cardputer restart, automatic BLE reconnection | PASS (1/1) |
| 6 | USB HID: `USB HID READY` | PASS (after §7.2) |
| 7 | macOS System Information lists the Cardputer | PASS (after §7.2) |
| 8 | YouTube profile over USB | PASS |
| 9 | USB → BLE switch (USB cable connected to the Mac), then `BLE READY` | PASS |
| 10 | BLE → USB switch, then `USB HID READY` | PASS |
| 11 | YouTube over USB after the BLE → USB switch | PASS |

**Not yet tested on 2a-2 (❓):**
- **the BLE mouse** (Profile 2 movement and drag; switching while dragging, `STAGE_2A_2.md` §5 H2 / H3);
- repeated switching (×5 each way);
- B7 repetition (Bluetooth OFF → ON ×3, restart ×2, boot in USB then switch to BLE);
- HOLD / CUSTOM on BLE;
- the `!` warning path;
- transport restore after restart in both directions;
- NVS statistics;
- long-time stability.

**Not passed / open:**

1. **H0 (reuse of the BLE-B1 bond) failed** (§7.1). The Mac had to unpair and pair again. The Cardputer side was not erased; in the new pairing the library replaces the old bond of this Mac only (REPEAT_PAIRING handler).
2. **USB was `NOT MOUNTED` at first** and was not listed in System Information, also after a Cardputer restart with the cable connected. It became `USB HID READY` after a power operation whose exact steps were not recorded (§7.2).

### 7.1 Evaluation: the BLE-B1 bond was not taken over

**Code comparison, BLE-B1 vs 2a-2 (✅ confirmed by source):**

| Aspect | Finding |
|---|---|
| Device identity | Same name (`BLE_DEVICE_NAME`, identity script), same own address: `BLEDevice::init()` → `onSync()` → `ble_hs_util_ensure_addr()`, public address of the same chip. The own IRK and keys come from the same NimBLE store (NVS `nimble_bond`); no code writes it |
| Advertising | Same `bleBegin()` (byte-identical): appearance 0x03C1, UUID 0x1812, scan response, intervals, `advertiseOnDisconnect(true)` |
| GATT table and handles | Identical block (identity script); hardware showed the same `ki10 / ko21 / mi24` |
| Report Map | Identical (r4 = B1 = 2a-2) |
| Security | Same `BLESecurity` settings and `SecurityCallbacks` (byte-identical) |
| Bond store and restore | NimBLE / library only; no sketch code reads, writes or deletes it. The deleting paths (REPEAT_PAIRING, store-full overflow on a new bond or CCCD) need a pairing or a CCCD write (`BLE_M1.md` §10). Before the unpairing, no connection happened, so none could run |
| CCCD store and restore | Same handles, so the stored CCCD records of the B1 pairing matched 2a-2 |
| BLE start / stop | 2a-2 calls the same `bleBegin()` from `bleStart()`, at boot if BLE is stored, or at the first switch. `bleStop()` only at a user switch |
| Initialization order | 2a-2 starts BLE after `loadSettings`, `loadLink`, display and `USB.begin()`; B1 had no USB HID. `BLEDevice::init()` waits for host sync in both cases. No order dependency of advertising or security on USB is visible |

**Assessment:**
- **Confirmed:** no difference in the code that defines what the Mac sees (address, name, advertising data, GATT, security). No code path changed the Cardputer's bond.
- **Not confirmed:** whether the Cardputer still held the B1 bond, and whether the Mac attempted any connection. The trace before the unpairing was not captured (`STAGE_2A_2.md` §6.5 was not run). The Mac's own records are unknown.
- **Hypothesis (Mac side):** macOS stopped reconnecting to a bonded LE HID device it had last seen working.
  - The same symptom appeared once before: BLE-1 r4 after the BLE-M1 r2 failures (`BLE_M1.md` §10). It was cleared only by a new pairing.
  - What both cases share is a period during which the device was not available (Launcher / Wi-Fi upload), followed by the device coming back. Earlier updates of the same kind (BLE-1 r1 → r4, r4 → M1, Stage 2a) did reconnect, so the update itself does not explain it.
- **"Unpair after every update?"** Not supported by the evidence: several updates kept the bond. Not excluded either: two of the last updates needed a new pairing, and the cause is unknown. A test with an unchanged firmware (§7.4 E1, E2) separates "update process / Mac behavior" from "firmware difference".

### 7.2 Evaluation: USB first `NOT MOUNTED`

**Source facts (✅):**
- **USB start:** with CDC On Boot, the core calls `USB.begin()` → `tinyusb_init()` before `setup()`. `setup()` calls `UsbMouse.begin()`, `UsbKeyboard.begin()` and `USB.begin()` (which returns early). **This happens on every boot, in USB and in BLE mode.** The HID devices are registered at static init.
- **No re-enumeration:** no code calls `USB.end()`, `tud_disconnect()` or `tud_connect()`. Switching transport never touches USB: `commitSwitch()` changes only `activeTransport`, `hidReady`, `bleStop()` / `bleStart()`. In BLE mode the USB HID stays enumerated and silent. So **"BLE → USB" does not need, and does not do, a USB re-enumeration**: the Mac sees the same USB device the whole time.
- **`NOT MOUNTED`:** `linkStatusText()` uses `!(bool)USB` = `!(_started && tinyusb_device_mounted)`. `tinyusb_device_mounted` is set only in `tud_mount_cb()`, after the **host has configured the device** (SET_CONFIGURATION), and cleared in `tud_umount_cb()`.
- **`USB HID READY`:** mounted and not suspended. Sending additionally checks the HID endpoint (`ready()`) and the IN completion (SENT / NOT_SENT / UNKNOWN, unchanged from 1e-4). The display state and the real ability to send are consistent: a report is never counted as sent without a completion.

**Assessment:**
- **Confirmed:**
  - `NOT MOUNTED` together with "not in System Information" means the Mac never configured the device at all: not even the CDC interface appeared.
  - This happens below the sketch's mode logic. The sketch's USB code path at boot is the same in the failing and the later working boot (same binary, same NVS).
- **Firmware candidates, not confirmed:**
  - (a) USB start after a **software restart**: the Launcher starts the app with a reset. If the restart with the cable connected was such a reset, the USB PHY and the host port may not see a clean disconnect / connect. A full power cycle resets them, which matches the recovery by a power operation. Neither the restart method nor the Launcher's USB mode is recorded.
  - (b) A TinyUSB start failure (`tinyusb_init` ≠ ESP_OK): it would also show `NOT MOUNTED` and is not logged on screen. It is unlikely to differ between two boots of the same binary.
- **External candidates, not excluded:**
  - cable (data lines);
  - port or hub;
  - **macOS accessory security on Apple Silicon laptops** ("Allow accessory to connect?"): a pending or declined prompt keeps a device off the bus list;
  - the power state when the cable was attached.

### 7.3 Decisions proposed (for Edgar)

| Issue | Proposal | Reason |
|---|---|---|
| BLE bond not taken over | **More verification needed** (no fix now) | No code difference explains it. A fix without a cause could change the verified GATT / advertising behavior. Whether it repeats on updates decides whether it matters for releases |
| USB first `NOT MOUNTED` | **More verification needed; firmware stays as it is** | Recovered without a firmware change; afterwards USB ⇄ BLE ⇄ USB worked. The cause may be outside the firmware. A firmware measure (for example a forced USB reconnect at boot) would touch the USB 1e-4 path, which is hardware-verified, and needs a reproducible case first |
| 2a-2 overall | **Keep; no change** | All tested functions passed. The BLE mouse and the H3–H5 items are still open |

### 7.4 Next verification (by information value; no unpairing, no erase)

| Priority | Test | Distinguishes |
|---|---|---|
| E1 | **BLE mouse and switching on 2a-2** (§5 H2, H3): Profile 2 movement and drag over BLE; BLE ↔ USB while dragging; 2× each way | The main function added by 2a-2, not yet tested on hardware |
| E2 | **Launcher round trip with the same binary:** with 2a-2 connected over BLE, start the Launcher, stay there about as long as an update takes (do not upload), start 2a-2 again. If it does not reconnect: open the trace (CONNECTION `t`), press **Connect** on the Mac (not Forget), trace again (§6.5 procedure) | Whether leaving the app for the Launcher alone (no firmware change) breaks automatic reconnection. If yes: Mac / process behavior; if no, continue with E3 |
| E3 | Only if E2 reconnects: **re-install the same 2a-2 binary** through the Launcher, then the same check | Whether the update process (write and first boot) matters; still no firmware difference |
| E4 | **USB cold start vs software restart** (one each, cable to the Mac directly, no hub): (a) power off → on with cable; (b) restart through the Launcher with cable. Record the screen, System Information, and any macOS accessory prompt | Whether `NOT MOUNTED` follows the restart method (candidate a) or is external |

Each test runs once unless it fails. On a failure, record the trace and screens before any recovery step.

## 8. Hardware verification — final record (author with Edgar, 2026-10-14)

**Status: main functions hardware-verified.**

This does **not** mean:
- all functions were verified;
- long-time stability;
- compatibility with every OS or application;
- that the known open issues (§8.4) are resolved.

**Environment:**
- M5Stack Cardputer ADV + Unit Encoder U135; MacBook Pro (Apple Silicon).
- `MediaJog_2a_2`, build id `2a-2`, app-only binary `4339eed6…`.
- Installed through the Launcher; no NVS erase.

### 8.1 Hardware results (consolidated; §7 and this round)

| # | Test | Result | Round |
|---|---|---|---|
| 1 | BLE new pairing (after the Mac-side unpairing, §8.4 #1) | PASS | §7 |
| 2 | BLE connection `BLE READY` / `LINK READY` | PASS | §7 |
| 3 | YouTube profile (keyboard) over BLE | PASS | §7 |
| 4 | Mac Bluetooth OFF → ON, automatic BLE reconnection | PASS (1/1) | §7 |
| 5 | Cardputer restart, automatic BLE reconnection | PASS (1/1) | §7 |
| 6 | USB HID `USB HID READY`; macOS System Information lists the Cardputer | PASS, after the first `NOT MOUNTED` (§8.4 #2) | §7 |
| 7 | YouTube profile over USB | PASS | §7 |
| 8 | USB → BLE switch (USB cable connected to the Mac), then `BLE READY` | PASS | §7 |
| 9 | BLE → USB switch, then `USB HID READY` and YouTube over USB | PASS | §7 |
| 10 | **BLE mouse movement:** SPINNER profile (`Fn+2`) over BLE, turning the U135 moves the Mac pointer | **PASS** | this round |
| 11 | **BLE mouse drag:** knob held drags a Finder window; releasing the knob ends the drag | **PASS** | this round |
| 12 | **BLE → USB switch while dragging:** knob held during a BLE drag, `Fn+B` to switch to USB. The drag ended **when `Fn+B` was operated**, not when the switch completed. Then `USB HID READY`, switch complete | **PASS** | this round |
| 13 | **Restart in USB mode:** restart from `USB HID READY`, USB HID connected again after the restart | **PASS** | this round |

**About #12 (the time of the drag release):**
- Observed: the drag ended at the `Fn+B` operation, before the switch completed.
- The direct cause (the key operation itself or the firmware's processing) **was not determined on hardware**.
- Source fact, for reference only: `Fn+B` opens the CONNECTION screen. `enterConn()` calls `releaseHoldNow()` and `setMouseButton(0)` and requests the release reports. This is Stage 2a behavior (design: nothing pressed is carried into CONNECTION), covered by host test S03.
- The hardware observation is consistent with this, but which step released the drag on the Mac was not observed separately.

### 8.2 Host-test and source evidence (unchanged, §2–§4)

- GATT identical to BLE-B1 (identity script).
- Stage 2a suite 85/85, new scenarios 32/32, NVS 47/47, HOLD 16/16, 8 mutants detected.
- Clean build with 0 sketch warnings.

These are logic tests on stubs, not hardware results.

### 8.3 Not verified on hardware (❓)

- BLE → USB switch while dragging **with the drag already inside the switch**: the switch state machine waiting for the BLE button-up (§3 N03). In #12 the release came earlier, at CONNECTION entry.
- USB → BLE switch while dragging.
- Repeated switching (×5 each way).
- B7 repetitions: Bluetooth OFF → ON ×3, restart ×2, boot in USB then switch to BLE. Only 1/1 each was done.
- HOLD and CUSTOM over BLE.
- Profiles other than YOUTUBE and SPINNER on 2a-2.
- The `!` warning path (owed release).
- Transport restore after restart in BLE mode (#13 covers USB mode only).
- NVS statistics over time.
- Long-time stability.
- Other hosts, OSs and applications.
- More than one BLE host. The supported maximum is 2 (§1 decision 2); the CCCD store limit is untested.

### 8.4 Known open issues (not resolved)

| # | Issue | Facts | Cause | Status |
|---|---|---|---|---|
| 1 | **Reuse of the BLE-B1 bond failed** | After updating from BLE-B1 to 2a-2, the Mac kept "MediaJog BLE" but did not connect (`BLE ADVERTISING`). Mac Bluetooth OFF → ON and Cardputer restarts, with and without USB, did not help. After the Mac-side removal and a new pairing, connection and reconnection work | **Not determined.** No code difference in identity, advertising, GATT, security or bond handling (§7.1). The trace before the removal was not captured | Open. **It is not established that every firmware update needs a new pairing**: earlier updates kept the bond. Verification proposal §7.4 E2 / E3 |
| 2 | **First USB recognition failed** | `NOT MOUNTED`, and the Cardputer was not in System Information (also after refresh and a restart with the cable connected). After a power operation (exact steps and USB power state not recorded), `USB HID READY` and listed. Later switches and the restart in USB mode (#13) worked | **Not determined**; also the exact recovery condition. Candidates (§7.2): a software restart without a clean USB reconnect; cable, port, macOS accessory approval or power state | Open. Firmware unchanged. Verification proposal §7.4 E4 |

## 9. Hardware verification on Windows and ChromeOS (author with Edgar, 2026-10-14)

- Firmware: Stage 2a-2 (`MediaJog_2a_2`, build id `2a-2`).
- On Windows and ChromeOS, **USB HID only**.

**Results by OS (hardware ✅, consolidated with §8):**

| OS | Connection | YOUTUBE | SPINNER |
|---|---|---|---|
| macOS (MacBook Pro, Apple Silicon) | USB | PASS | PASS (mouse) |
| macOS | BLE | PASS | PASS (movement, drag) |
| Windows | USB | PASS | PASS (pointer movement) |
| ChromeOS | USB | PASS | PASS (pointer movement) |

- **Windows / ChromeOS, USB:** YouTube video control with the YOUTUBE profile succeeded, and pointer movement with the SPINNER profile succeeded.

**Not verified (❓):**
- **BLE on Windows and ChromeOS.**
- Drag over USB on Windows and ChromeOS (only pointer movement was reported).
- OS versions and machine models for Windows and ChromeOS (not recorded).
- Profiles other than YOUTUBE and SPINNER on these OSs.
- **ChromeOS is not a general Linux distribution:** this result is **not** evidence for other Linux systems.
- Everything in §8.3 still applies.

The known open issues (§8.4) are unchanged. They were observed on macOS; whether they occur on the other OSs is unknown.
