# Stage 1e-4 — Settings kept across power cycles (NVS)

Created: 2026-10-13 · Implementation: Claude Opus 5.5
**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) independent review before hardware testing.** Before the first install, the review should explicitly accept or reject the pre-existing NVS erase path described in §6.3.

> **Status update (2026-10-13, appended; the line above is the status at build time):**
> - **Hardware results reported by the author** (in the BLE review request): NVS save and restore after restart work; the Launcher's Wi-Fi settings are kept; SOUND TERRARIUM works normally. Before installing 1e-4, the author backed up the whole 8 MB flash with esptool.
> - No per-item results for the §9 checklist (N1–N15) were reported; items are not individually marked as passed.
> - The author has fixed Stage 1e-4 as the **USB release candidate**; BLE work continues separately (`docs/BLE_1.md`).

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

Stage 1e-4 stores the last selected profile, the Custom mappings (including BTN TAP/HOLD) and the Spinner sensitivity in ESP32 NVS, and restores them at power-on. All HID behavior from Stage 1e-3 is unchanged. Base: `MediaJog_1e_3/` (unchanged).

## 1. Persistent record structure

One blob, schema version 1, **21 bytes**:

| Offset | Size | Field | Valid values |
|---|---|---|---|
| 0 | 4 | Magic | ASCII `MJCF` |
| 4 | 1 | Schema version | 1 |
| 5 | 1 | Record length | 21 (must also equal the stored blob size) |
| 6 | 1 | Profile | 1–4 |
| 7 | 1 | Spinner sensitivity index | 0–13 (`SENS_COUNT` = 14; 1…128 counts per detent) |
| 8–10 | 3 | CW: modifiers, usage, flags | modifiers ⊆ `0x0F` (L-Ctrl, L-Shift, L-Alt, L-GUI); usage valid (§3); flags **0** (TAP only) |
| 11–13 | 3 | CCW: modifiers, usage, flags | same; flags **0** |
| 14–16 | 3 | BTN: modifiers, usage, flags | same; flags ⊆ `CK_HOLD` (`0x01`) |
| 17–20 | 4 | CRC-32 of bytes 0–16 | IEEE 802.3 / zlib CRC-32, little-endian |

The record is serialized byte by byte (`encodeSettings()`), so struct padding and compiler layout do not matter.

**Never stored:** held keyboard keys or modifiers (`kbHoldMods`, `kbHoldUsage`, `holdArmed`), the mouse button, pending reports, event queues, USB state, U135 state, and the CUSTOM SETUP draft. The `Settings` struct has only `profile`, `sensIndex` and `custom[3]`. A source check confirms that none of the persistence functions references a HID, queue, USB, U135 or draft variable ✅.

**Defaults** (no valid record): Profile 1, sensitivity index 5 (8 counts per detent, as in Stage 1d), Custom unassigned, BTN = TAP.

## 2. Namespace and storage keys

| Item | Value |
|---|---|
| Partition | the default NVS partition (`"nvs"`), already initialized by the Arduino core before `setup()` |
| Namespace | `mediajog` (8 characters; the NVS limit is 15) |
| Key | `cfg` (one blob) |
| API | ESP-IDF `nvs.h` directly: `nvs_open`, `nvs_get_blob`, `nvs_set_blob`, `nvs_commit`, `nvs_get_stats`, `nvs_close` |

**Why not Arduino `Preferences`:** `Preferences` would also be safe here. Without a partition label, `Preferences::begin()` only calls `nvs_open()`; it calls `nvs_flash_init_partition()` only when a label is given (`Preferences.cpp:36-45`), and it never erases. But it reduces errors to `true`/`false`/`0`, and this stage must distinguish "no record" from "read error" and "out of space". The namespace concept is the same.

## 3. CRC and validation rules

`decodeSettings()` checks, in this order, and applies **nothing** unless all checks pass:

1. Blob size ≥ 6, else `CFG_BAD_LENGTH`. A blob larger than 64 bytes is rejected without being read.
2. Magic `MJCF`, else `CFG_BAD_MAGIC`.
3. Version > 1 → `CFG_NEWER_VERSION`; version ≠ 1 → `CFG_BAD_VERSION`.
4. Length byte = 21 **and** blob size = 21, else `CFG_BAD_LENGTH`.
5. CRC-32 over bytes 0–16, else `CFG_BAD_CRC`.
6. Every field (`settingsValid()`), else `CFG_BAD_FIELD`:
   - profile 1–4;
   - sensitivity index < 14;
   - modifiers only `0x01|0x02|0x04|0x08`;
   - usage is 0 or a usage the setup UI can produce: `0x04–0x31`, `0x33–0x38`, `0x3A–0x45`, `0x49–0x52`;
   - CW/CCW flags = 0, BTN flags ⊆ `CK_HOLD`.

   The usage set is host-tested to be exactly the set produced by typed keys (`usageForChar()`), the Enter/Backspace/Tab flags and the picker list ✅.
7. Only then are the decoded values copied into a temporary, and from there into the runtime state in one step (`applySettings()`).

The same `settingsValid()` runs **before every write**, so an invalid record is never stored.

**Schema migration:**
- A future firmware with schema version N > 1 keeps its own magic, reads the version byte first, and either converts a version-1 record (using the version-1 length, 21) or rejects it, falling back to defaults.
- Older firmware (this one) treats a record of version > 1 as `CFG_NEWER_VERSION`. It uses the defaults and becomes **read-only for that session**, so a downgrade never overwrites a newer record.
- New fields must be appended, with the length byte and CRC covering them.

## 4. Read/write lifecycle

**Boot (`setup()`):**
1. HID runtime state is released by static initialization; `loadSettings()` does not touch it. Every USB-ready period still starts with an all-keys-up report and a released mouse button, exactly as in 1e-3.
2. `loadSettings()` runs right after `M5Cardputer.begin()`, before anything is drawn. It opens the namespace **read-only**, which never creates anything, reads `cfg`, and validates it (§3).
3. A fully valid record is applied: profile, sensitivity and Custom mappings, with `rebuildCustomActions()`. Otherwise **all** defaults are applied.
4. The device starts in the restored profile. The first screen draw shows it; in Profile 2 the Spinner screen shows the restored sensitivity.
5. **Nothing is written at boot.** A restored BTN = HOLD is only the mode: `readInitialState()` takes the knob state at boot (or at U135 reconnection) as the reference, so a HOLD starts only on a new physical press edge (unchanged 1e-3 code).

**Runtime:**
- A trigger only sets a pending flag: `setProfile()` → `profileSavePending`; `exitSetup(true)` → `customSavePending`; `changeSensitivity()` → `sensSavePending` + timestamp.
- `serviceSettings()` runs in `loop()` **after** `sendMouse()`/`sendQueued()`, so the HID release reports of that loop are submitted before any flash write. It also runs while USB is not ready: storing a profile does not depend on USB.
- `writeSettings()` steps:
  1. Validate.
  2. Compare with the last confirmed record; if identical, skip.
  3. Check that at least 32 NVS entries are available.
  4. `nvs_set_blob` + `nvs_commit`, then read the record back and compare.
  5. Only after all of that succeeds does `persisted` change and the status become `NV OK`.

**Status display:**

| Where | What |
|---|---|
| Profile screens and CUSTOM SETUP | `NV …` under the build tag, at x 192, y 10 (on the Spinner screen it is beside the sensitivity number instead) |
| Diagnostic counters | `nv w<writes> f<failures>`, plus the longest write time on the Profile 1/3 screen |
| Last-event line after boot | `CFG RESTORED`, `NO SAVED CFG`, `CFG BAD CRC`, … |
| After a Save | `SAVED`, `SAVE NOT STORED` or `SAVED (RAM ONLY)` |

| Status | Meaning |
|---|---|
| `NV OK` | Last load or write confirmed |
| `NV NEW` | No record yet (defaults) |
| `NV WAIT` | Sensitivity change waiting for the debounce |
| `NV BAD` | Invalid record ignored; defaults in use |
| `NV VER` | Newer record version; read-only session |
| `NV ERR` | NVS read error; read-only session |
| `NV FULL` | Not enough NVS space; not written |
| `NV WFAIL` | Last write failed |

## 5. Exact write policy (DEVELOPMENT_PLAN.md §6.9)

| Setting | Trigger | Delay | Fields changed in the stored record |
|---|---|---|---|
| Profile | Confirmed `Fn+1…4` switch to a **different** profile (`setProfile()`) | Same loop, after the HID sends (≤ ~20 ms) | `profile` only |
| Custom mappings + BTN TAP/HOLD | **Save** in CUSTOM SETUP only (`exitSetup(true)`). Cancel, `esc` and editing never set the flag | Same loop | `custom[3]` only (copied from the saved mappings, never from the draft) |
| Spinner sensitivity | Each `Fn+;`/`Fn+.` step restarts the timer | **`SENS_SAVE_DELAY_MS` = 2000 ms** after the **last** step | `sensIndex` only |

- **Distinct triggers, not a global debounce:** each trigger copies only its own fields into a copy of the **last confirmed record**. For example, a profile write never stores an unsaved sensitivity that is still waiting, and never stores Custom values whose Save write failed. If two triggers are due in the same loop, they share one write. ✅ host test
- **Why 2 s:** long enough to cover a burst of key presses while tuning; short enough that power-off soon after tuning usually keeps the value. A power-off within 2 s of the last step may restore the previous value (accepted in the plan).
- **Identical records are not written** (`persistedInNvs` + byte comparison), e.g. Save without changes, or a sensitivity changed and changed back. ✅ host test
- **Never written** per detent, per mouse report or per key event. Rotation does not touch any trigger.
- **Failed writes:** the status becomes `NV WFAIL` (or `NV FULL`) and the failure counter increments. The confirmed record stays unchanged, and the setting is **not** reported as stored. There is no automatic retry, to avoid repeated flash operations against a failing partition; the next trigger writes again. A Save whose write failed shows `SAVE NOT STORED`. The mapping stays active in RAM until power-off.

## 6. NVS coexistence and initialization safety review

### 6.1 What MediaJog itself does

- The only NVS calls are in `nvBackendRead()` and `nvBackendWrite()` (source lines 1015–1044): `nvs_open` (READONLY at boot, READWRITE for a write), `nvs_get_blob`, `nvs_set_blob`, `nvs_commit`, `nvs_get_stats`, `nvs_close`. All of them use the namespace `mediajog`, key `cfg`.
- **No** `nvs_flash_erase()`, `nvs_flash_erase_partition()`, `nvs_erase_key()`, `nvs_erase_all()`, `esp_partition_erase_range()`, `nvs_flash_init*()` or `Preferences` call exists in the sketch (grep ✅).
- Boot reads only. Opening read-only does not create the namespace; the namespace entry is created by the first confirmed user-triggered write.
- **Free-space guard:** a write needs at least `NV_MIN_AVAILABLE_ENTRIES` = 32 available entries (`nvs_stats_t.available_entries`), about 1 KB. The record itself needs about 3 entries. MediaJog therefore never takes the last space of a partition shared with the Launcher and other apps; it shows `NV FULL` instead.
- A unique namespace isolates **keys** only. It does **not** isolate partition space, wear, or the core's erase path below. It is not claimed to guarantee the safety of other apps' NVS data.

### 6.2 Libraries in this build (source-checked)

- **M5GFX** writes namespace `M5GFX`, key `AUTODETECT`, when the detected board changes (R-4, unchanged since Stage 1a).
- **M5Unified** IMU calibration uses NVS only when calibration saving is used (`IMU_Class.inl:352-386`); the IMU is disabled in this sketch.
- The core's **BLE** library erases NVS on the same two init errors (`BLEDevice.cpp:312-316`); this sketch does not use BLE.

### 6.3 The Arduino core's automatic erase (pre-existing; not removable from the sketch)

`initArduino()` in `cores/esp32/esp32-hal-misc.c:331-344` (m5stack:esp32 3.3.9) runs **before `setup()` in every Arduino-core app**:

- If `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, it erases the **whole** first NVS partition (`esp_partition_erase_range(partition, 0, partition->size)`) and initializes it again.
- This would erase the Launcher's and other apps' NVS data.
- `initArduino()` is a strong symbol called from `main.cpp:112`, with no weak hook, so a sketch cannot disable or intercept it.

| Question | Finding |
|---|---|
| New with 1e-4? | **No.** The same code runs at every boot of MediaJog 1a–1e-3, and of any other Arduino-core app. 1e-4 adds no call to it and does not change when it runs |
| `NEW_VERSION_FOUND` | ESP-IDF: the partition "contains data in new format and cannot be recognized by this version of code". It depends on the NVS format written by the **newest** firmware that used the partition, not on MediaJog's writes. ❓ Whether the installed Launcher or SOUND TERRARIUM uses a newer NVS format than ESP-IDF 5.5.4 (the version in this build) is unknown |
| `NO_FREE_PAGES` | ESP-IDF: the partition "doesn't contain any empty pages. This may happen if NVS partition was truncated". ❓ Whether normal writes can lead to it is not stated on the ESP-IDF page checked. MediaJog's contribution is limited by the small record, rare writes, the skip of identical writes, and the 32-entry guard |
| Can 1e-4 trigger the erase itself? | Not through any call it makes. Only indirectly, if its writes contributed to an exhausted partition; the guard is designed to prevent that. Not verified on hardware |

**Mitigations considered:**

| Option | Decision |
|---|---|
| Keep usage small and rare, guard free space, never erase | **Implemented** |
| Patch `esp32-hal-misc.c` or build a custom core | **Rejected:** it changes the established toolchain for everyone, and the change would be lost on core updates |
| A private NVS partition for MediaJog | **Rejected:** needs a partition-table change, which conflicts with the Launcher (no partition changes) |
| Back up the flash before the first 1e-4 hardware test | **Proposed, for the author** (§9 step 0). Reading flash is non-destructive, but connecting a flashing tool resets the board. Not done by Claude; the tool, port and flash size are to be confirmed by the author |

**Conclusion:** Stage 1e-4 introduces no destructive NVS operation, and no new trigger of the core's erase path that I could identify. The core path itself remains a **pre-existing, unmitigable-from-the-sketch risk** shared by all Arduino apps on the device. It is recorded here as a residual risk to be accepted or rejected by Edgar and the author before the hardware test. It is not claimed to be eliminated.

### 6.4 Power loss

- **ESP-IDF documentation** (NVS, "Security, Tampering, and Robustness"): the device can be powered off at any time; only the key-value pair being written at that moment may be lost.
- **Expected after an interrupted write:** the old record or the new one. In the worst case, no record → defaults with `NV NEW`/`NV BAD`; never a partially applied record, because of the CRC and the field checks.
- ❓ Not tested on hardware (AT-16).

## 7. Failure handling

| Case | Result | Writes afterwards | Test |
|---|---|---|---|
| No saved record (namespace or key absent) | Defaults, `NV NEW`, `NO SAVED CFG` | Normal (first user trigger creates it) | ✅ host |
| Invalid magic | Defaults, `NV BAD`, `CFG BAD MAGIC` | Replaced only by the next user-triggered save (§7.1) | ✅ host |
| Unsupported newer version | Defaults, `NV VER`, `CFG NEWER VERSION` | **None this session** (never overwritten) | ✅ host |
| Version 0 / other unsupported older | Defaults, `NV BAD` | As invalid magic | ✅ host |
| Wrong length (byte or blob size, 3/20/22/100 bytes) | Defaults, `NV BAD` | As invalid magic | ✅ host |
| CRC mismatch | Defaults, `NV BAD` | As invalid magic | ✅ host (all 168 single-bit flips rejected) |
| Out-of-range profile (0, 5) | Defaults, `NV BAD`, `CFG BAD VALUE` | As invalid magic | ✅ host |
| Invalid sensitivity index (14) | Defaults, `NV BAD` | As invalid magic | ✅ host |
| Invalid Custom usage / modifier / flag (0x32, 0x39, 0xE0, modifier 0x10, HOLD on CW/CCW, unknown BTN flag) | Defaults, `NV BAD` | As invalid magic | ✅ host |
| NVS read failure (e.g. not initialized, no `nvs` partition under the Launcher) | Defaults, `NV ERR`, `NVS READ ERROR` | **None this session** | ✅ host |
| NVS write failure | `NV WFAIL`, `SAVE NOT STORED`; confirmed record unchanged; not retried automatically | Next trigger tries again | ✅ host |
| Not enough space (< 32 entries) | `NV FULL`; nothing written | Next trigger tries again | ✅ host |
| Interrupted write / power loss | Old or new record (ESP-IDF design), else defaults | — | ❓ hardware (AT-16) |
| Partial restoration | Impossible by construction: all fields from one fully valid record, or all defaults | — | ✅ host |

### 7.1 Why an invalid record may later be replaced

The namespace `mediajog` is used only by this firmware, and an invalid record in it cannot be used by anything. It is **never** rewritten at boot or automatically. It is replaced only when the user performs an action the policy says to persist: a profile switch, a Save, or a settled sensitivity change. The other fields then come from the defaults that are in use. Without this, a single corrupted record would disable persistence permanently. Exceptions, where nothing is written, are a newer version and a read error.

**Unrelated namespaces are never reset.** No failure path calls an erase function.

## 8. Build results and checksums (2026-10-13)

| Item | Result |
|---|---|
| Environment | arduino-cli 1.5.1, m5stack:esp32 3.3.9 (ESP-IDF 5.5.4), FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc` (USB-OTG TinyUSB, CDC On Boot); M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2 |
| Result | ✅ exit 0. Flash 608,179 B (46%), RAM 60,228 B (18%) |
| Warnings (`--warnings all`) | **0 in the sketch.** Core/library warnings are the same set as a fresh 1e-3 build: `NUM_DIGITAL_PINS` and 5 other pin macros redefined (32 each), `KEY_TAB` and `KEY_BACKSPACE` redefined (1 each), `cdc_line_control_state_t` missing initializers (4 + 4) |
| `#error` guards | ✅ hwcdc build stops ("USB Mode must be 'USB-OTG (TinyUSB)'"); CDC-off build stops ("USB CDC On Boot must be 'Enabled'") |
| Source `MediaJog_1e_4/MediaJog_1e_4.ino` | 87,671 B, SHA-256 `04d0f590db626700b5c6578096d81169d52832a582874dda2f484e88c89018f5` |
| App-only binary `MediaJog_1e_4/build/MediaJog_1e_4.ino.bin` | 608,320 B, SHA-256 `8db62fc43dfce3592b510ac232c3588a5a5fb6580c0a3b7d20105c465195d61c` |
| Binary check | Contains `1e-4` and `mediajog` |

Both hashes are in `MediaJog_1e_4/SHA256SUMS.txt`; `shasum -a 256 -c` → OK.

**Diff vs 1e-3:** 366 insertions, 11 deletions.
- 15 new functions (record, NVS access, load/save, status).
- 11 changed: `setProfile`, `changeSensitivity`, `exitSetup` (one trigger line each); `loop` (one call); `setup` (one call); 6 drawing functions (status text and counters).
- **45 unchanged**, including every HID, HOLD, queue, USB, mouse and U135 function (`sendKeyReport`, `sendQueued`, `sendRelease`, `tapKey`, `releaseHoldNow`, `onButton`, `pollUnit`, `updateUsbState`, `sendMouse`, `setMouseButton`, `readInitialState` …) and the r3 overview-row sizing.

**Host tests ✅ (logic only, not hardware):**
- The real record and policy functions, extracted from the source and run against a mock NVS backend: **47/47**.
  - CRC test vector, the valid-usage set, and the restore of a full record.
  - 19 corruption cases plus all 168 single-bit flips.
  - Read error, newer version, write failure, low space, no retry.
  - Profile-only / Custom-only / sensitivity-only field updates, the 2 s debounce, skipping identical writes, a combined write, and replacement of a bad record by a user save.
- No device NVS was used or corrupted.

## 9. Hardware test checklist (proposed; not run)

**Step 0 (author's decision, before installing):** optionally back up the whole flash with a read-only tool (§6.3). Note the Launcher name/version if visible, and check that the Launcher's own settings (e.g. Wi-Fi) are present before the test.

Install `MediaJog_1e_4.ino.bin` (app-only) via the Launcher Wi-Fi upload and **record the SHA-256 of the uploaded file** (`8db62fc4…95d61c`).

| # | Do | Pass condition | Plan ref. |
|---|---|---|---|
| N1 | First start | Tag `1e-4` on every screen; `NV NEW` and `NO SAVED CFG` if no record exists yet; starts in Profile 1, sensitivity 8 | AT-12 |
| N2 | `Fn+2`, power OFF/ON; repeat for Profiles 3, 4, 1 | `NV OK` after each switch; the same profile after each restart; `nv w` +1 per switch | AT-10, AT-17 |
| N3 | Switch profile, power OFF within about 1 s; ≥ 10 times across all profiles | Always a valid record (`CFG RESTORED`); normally the new profile. Record any case where the previous profile returns | AT-16 |
| N4 | Re-select the current profile (e.g. `Fn+1` in Profile 1); turn the knob for a while | No write (`nv w` unchanged); no write during rotation | AT-13 |
| N5 | CUSTOM SETUP: CW = Right, BTN = HOLD Shift, Save; power cycle | Restored: Profile 4 screen shows the mappings, `BTN: HOLD Sft`; the r3 overview shows them unchanged | AT-18 |
| N6 | Edit mappings, **Cancel**; power cycle | Previous saved mappings return; `nv w` unchanged by Cancel | AT-18 |
| N7 | Save without changes | `SAVED`; `nv w` unchanged | AT-13 |
| N8 | Profile 2: `Fn+;` ×3 quickly | `NV WAIT` during the steps; one write (`nv` +1) about 2 s after the last; power cycle → same sensitivity | AT-19 |
| N9 | Adjust sensitivity, power OFF within 2 s | Previous **or** new value; record which (accepted) | AT-19 |
| N10 | Restored BTN = HOLD: start with the knob **not** pressed; then start **while holding** the knob | Nothing held on the Mac (keyboard viewer / text field) in both cases; while held at boot, nothing happens until release and a new press | AT-11 |
| N11 | Restored Profile 2: start while holding the knob | No mouse button down until a new press | AT-11 |
| N12 | Watch `W` and the `nv … ms` value during writes | Record the longest write time; no noticeable stall of the spinner or of key output | §6.9 |
| N13 | 1e-3 regression: HOLD Shift + CW Right; release on profile switch; Profiles 1–3 quick check; `Fn+S`, `Fn+H` | As in 1e-3 | AT-1–AT-3, AT-8, AT-9 |
| N14 | After N2–N9: open the Launcher, check its settings (e.g. Wi-Fi); start SOUND TERRARIUM and check its settings; return to MediaJog | Launcher and other apps start and keep their settings; MediaJog restores its record | AT-14, AT-21 |
| N15 | Corrupt / old records | **Not on the device** (host-tested only; do not corrupt shared NVS) | AT-20 |

## 10. Known limitations

- The core's boot-time whole-partition erase (§6.3) is pre-existing and cannot be disabled from the sketch.
- A flash write blocks `loop()` for its duration. A page erase during NVS garbage collection may take longer than a normal write. ❓ Not measured; N12 records it.
- No automatic retry after a failed write. The setting stays active in RAM, and the status shows the failure.
- An invalid record is replaced, by defaults plus the triggering field, on the next user-triggered save (§7.1).
- A power-off within 2 s of the last sensitivity step may keep the previous value.
- Settings belong to the device, not to the connected host.
- The free-space guard uses `available_entries`; fragmentation could still make a write fail (`NV WFAIL`).
- The known USB replug issue is unchanged. Restoring a profile does not depend on USB.
- Stage 1e is **not** complete: 1e-5 (hardware regression and application tests) remains.
