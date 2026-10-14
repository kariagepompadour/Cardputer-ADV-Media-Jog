# Stage 1e-2 — On-device configuration UI for Profile 4 (Custom)

Created: 2026-10-13 · Implementation: Claude Opus 5.5
**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) review before hardware testing.**

> **Status update (2026-10-13, appended; the line above is the status at build time):**
> - **Hardware test 1: FAILED.** `Fn+S` did not open CUSTOM SETUP (§9).
> - **Root cause: not established.** Source review found no defect in the `Fn+S` path. It did find that this build mislabels itself `MEDIA JOG 1e-1` on the Spinner screen.
> - **Revision r2** (`MediaJog_1e_2_r2/`) adds build identification and key diagnostics without changing behavior. It is built, not hardware-tested; see §10.
>
> **Status update 2 (2026-10-13, appended):**
> - **r2 hardware test: 14 observations passed**, including `Fn+S` opening CUSTOM SETUP, Save, Cancel, Custom output and `Fn+G` (Command).
> - Several checklist items were **not** tested; the full 19-item checklist is **not** passed (§11).
> - **Revision r3** (`MediaJog_1e_2_r3/`) enlarges the key names on the overview screen only. It is built, not hardware-tested (§12).
>
> **Status update 3 (2026-10-13, appended):**
> - **r3:** the author confirmed that the enlarged key labels on the CUSTOM SETUP overview are appropriately sized. The other §12-4 items were not reported individually.
> - The r3 design is kept unchanged in **Stage 1e-3** (`MediaJog_1e_3/`, `docs/STAGE_1E_3.md`).

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

Stage 1e-2 adds a configuration UI for Profile 4 (Custom), run entirely on the Cardputer ADV keyboard and display. **TAP only** (HOLD is Stage 1e-3). **RAM only**: settings are lost at power-off (NVS persistence is Stage 1e-4). Profiles 1–3 are unchanged from Stage 1e-1.

## 1. Deliverables and hashes

| File | Size | SHA-256 |
|---|---|---|
| `MediaJog_1e_2/MediaJog_1e_2.ino` | 59,879 B | `cbde987a2325a001c6c5d468e6cf85da2c7a69f4c8c8ac7c9ff0695501e59a43` |
| `MediaJog_1e_2/build/MediaJog_1e_2.ino.bin` (app-only, for Launcher Wi-Fi upload) | 603,472 B | `65e95432a20dedf8202f587feb84d4d1ad389314f36133a29818128e4ee222ef` |

- Both are recorded in `MediaJog_1e_2/SHA256SUMS.txt` (paths relative to the repository root); `shasum -a 256 -c` → OK.
- Starting point: `MediaJog_1e_1/MediaJog_1e_1.ino` (SHA-256 `38a69535…6ff3e`), copied and then edited. Stage 1e-1 and all earlier milestones are untouched (§8).

## 2. Interaction design

### 2.1 Design constraints found in the keyboard library (M5Cardputer 1.1.1)

| Finding | Source | Consequence for the UI |
|---|---|---|
| `keysState()` exposes `fn`, `shift`, `ctrl`, `alt`, `opt`, `enter`, `del` (Backspace), `tab`, `space` and `word` (characters of the pressed keys). It is a **state** (which keys are down), not a stream of key events | `Keyboard.h:77-109`, `Keyboard.cpp:90-210` | The sketch derives key **presses** itself, by comparing `keysState()` with the previous loop (§3.4) |
| With Shift, Ctrl or CapsLock active, `word` holds the second legend (`!` for `1`, `A` for `a`); **Fn does not change the character** | `Keyboard.cpp:202-207` | `Fn+letter` can be used for commands. Typed keys are mapped back to the unshifted key (§3.3) |
| Tab, Backspace and Enter are **not** in `word`; they appear only as the `tab`, `del` and `enter` flags | `Keyboard.cpp:169-183` | Enter / Backspace / Tab are read from the flags |
| **`KEY_TAB` and `KEY_BACKSPACE` are defined differently** by M5Cardputer (`0x2b`, `0x2a`, `Keyboard_def.h:20-21`) and by the core's `USBHIDKeyboard.h` (`0xB3`, `0xB2`, lines 82-83), and both headers are included by the sketch | header sources | The sketch never compares key codes against these macros; it uses the library's flags, which are computed inside the library's own source file |
| Opt is defined as `0x00` (`Keyboard_def.h:18`) | — | Not used by the UI. Modifiers are toggled explicitly instead of being captured from the Cardputer's own modifier keys |
| The keyboard reader processes **one** key event per `update()` (`TCA8418.cpp:52-75`); `loop()` runs about every 20 ms | — | A two-key combination arrives over two loops; edge detection handles this |
| Keycap legends: orange **esc** on the `` ` `` key, **ok** on Enter, **del** on Backspace | author's keyboard image (2026-10-12) | Used as Cancel / Save / Clear |
| Screen 240×135; text size 1 = 6×8 px (38 characters per line from x=4), size 2 = 12×16, size 3 = 18×24 | M5GFX | Layouts below fit these limits |

### 2.2 The design (implemented)

**Entering:** in Profile 4, press **`Fn+S`** (S for setup). In Profiles 1–3, `Fn+S` does nothing. Opening the setup screen releases everything that could be held on the host and stops all HID output until the screen is closed (§5).

**Screen 1 — CUSTOM SETUP (overview of the three actions):**

```
CUSTOM SETUP                         ← size 2, cyan
1 CW    Ctl+Right                    ← selected row highlighted (navy background)
2 CCW   Ctl+Left
3 BTN   Space
1-3/turn: select  knob press: edit   ← size 1 help
ok (Enter): SAVE   esc (`): CANCEL
Unsaved changes                      ← or "No changes"
RAM only: lost at power-off
```

| Input | Effect |
|---|---|
| `1` / `2` / `3` | Edit CW / CCW / BTN |
| Turn the knob | Move the highlight |
| Press the knob | Edit the highlighted action |
| **ok (Enter)** | **SAVE**: the draft becomes the active Custom mapping; back to Profile 4 |
| **esc (`` ` ``)** | **CANCEL**: the draft is discarded; back to Profile 4 with the previous mapping |
| Anything else, including `Fn+1`…`Fn+4` | Ignored (close the setup screen first) |

**Screen 2 — EDIT (one action):**

```
EDIT CW                              ← size 2, cyan
Ctl  Sft  Alt  Cmd                   ← active modifiers green, inactive grey
Right                                ← key name, size 3 ("(none)" if unassigned)
                        5/28         ← position in the key list, when used
Type a key, or turn knob for list
Fn+C Ctl  Fn+S Sft  Fn+A Alt  Fn+G Cmd
del: clear  ok/knob press: done
esc (`): undo this action
```

| Input | Effect |
|---|---|
| A letter, digit, space or symbol key **without Fn** | Assigns that key (unshifted, §3.3). Modifiers are kept |
| Turn the knob | Steps through the **key list** (28 keys): Right, Left, Up, Down, Space, Enter, Esc, Tab, Backspace, Delete, Insert, Home, End, PageUp, PageDown, F1–F12, `` ` ``. Modifiers are kept |
| `Tab` key | Assigns Tab |
| **`Fn+C` / `Fn+S` / `Fn+A` / `Fn+G`** | Toggle **Ctrl / Shift / Alt (Option) / GUI (Command)** |
| **del (Backspace)** | Clears the action (no key, no modifiers) |
| **ok (Enter)** or knob press | Done with this action; back to screen 1 (still unsaved) |
| **esc (`` ` ``)** | Undo the changes to this action since entering EDIT; back to screen 1 |

**Why some keys can only come from the list:** Enter, Backspace and `` ` `` operate the UI itself (done / clear / undo). Arrows, function keys and navigation keys do not exist on the Cardputer.

**Seeing the assignments:**
- The Profile 4 status screen lists `CW :`, `CCW:` and `BTN:` with their saved keys, plus `Fn+S: SETUP` (or `NOT CONFIGURED` when nothing is assigned).
- Screen 1 shows the draft.
- Key names describe the **key**, not a character: what a letter or symbol key types depends on the host's keyboard layout (e.g. US vs JIS).

**Profile 4 in normal use:** one tap of the saved key (with its modifiers) per detent or per knob press, through the same send path as Profiles 1 and 3. Unassigned actions send nothing and are counted (`ign`).

## 3. Implementation details

`diff -u MediaJog_1e_1/MediaJog_1e_1.ino MediaJog_1e_2/MediaJog_1e_2.ino`: 1 file, 591 insertions, 17 deletions. Most of it is new functions; changes to existing functions are small guards.

### 3.1 Data (RAM only)

- `CustomKey {modifiers, usage}`; usage 0 means unassigned.
- `customSaved[3]` is what Profile 4 sends and changes only on Save. `customDraft[3]` is edited in the UI and is discarded on Cancel.
- `customAction[3]` / `customLabel[3]` hold the saved mapping in the `KeyAction` form the existing send path uses. They are rebuilt only on Save.
- Modifier bits are `static_assert`ed against TinyUSB's `KEYBOARD_MODIFIER_LEFT*` values. Every picker usage and character usage is `static_assert`ed against TinyUSB's `HID_KEY_*` constants.
- All new types are declared before the first function (Arduino prototype rule, as in 1e-1).

### 3.2 Changes to existing functions

| Function | Change |
|---|---|
| `actionFor()` | Profile 4 returns `customAction[ev]` (only assigned actions are ever queued) |
| `pollKeyboard()` | While the setup UI is open, all keys go to `processSetupKeys()` and the function returns before any chord handling (so no profile switching or sensitivity keys). `Fn+S` added as a chord; it acts only in Profile 4 |
| `onRotation()` | While the UI is open, rotation only drives the UI (`setupRotate()`) and returns first. Profile 4 normal use: per-detent taps via `pushCustom()` |
| `onButton()` | While the UI is open, the press only drives the UI. Profile 4 normal use: `pushCustom(EV_PRESS)` |
| `drawStatus()` | Profile 4 has its own body (`drawCustomStatusBody()`); Profiles 1–3 are drawn exactly as before |
| `loop()` | Display selection only: a `LAYOUT_SETUP` branch draws the setup screens. Polling and sending order unchanged |

Profiles 1–3 code paths are not modified. The new code is either a guard that returns before them (only while the setup UI is open, which can only happen in Profile 4) or a Profile 4 branch.

### 3.3 Typed-key capture

- `word` characters are mapped to the unshifted key by `baseChar()`, using the library's own key table `_key_value_map` (`Keyboard.h:18-75`). Then `usageForChar()` converts the result to an HID usage. Holding Shift while typing therefore does **not** add Shift; Shift is added only with `Fn+S`.
- **Host test (✅, not a hardware test):** `baseChar()`, `usageForChar()`, `keyName()` and `formatKey()` were extracted verbatim from the sketch and compiled on the Mac against the real `_key_value_map` and TinyUSB constants. Built once with signed and once with unsigned `char`.
  - All 48 printable keys of the table map to themselves and from their shifted legend back to themselves.
  - All have a non-zero usage.
  - Spot checks (`a`, `z`, `1`, `0`, `` ` ``, space, `/`, `'`, `!`→`1`, `+`→`=`, `*`→`8`, `(`→`9`, `Q`→`q`, `~`→`` ` ``, `?`→`/`) pass.
  - The longest label (`Ctl+Sft+Alt+Cmd+Backspace`, 25 characters) fits its 28-byte buffer.
  - Note: the library stores Backspace/Tab/Enter as `0x2a`/`0x2b`/`0x28`, which equal the characters `*`/`+`/`(`. These keys never appear in `word`, so this does not affect capture.

### 3.4 Key-press detection in the UI

- `processSetupKeys()` compares the current `keysState()` with the previous loop. A press is either a rising `enter`/`del`/`tab` flag or a base character that was not present before.
- `fn` is read at the moment of the press. Keys already held when a screen opens (e.g. `Fn+S`) are taken as the starting state, so they don't count as presses.
- **Consequence:** for `Fn+C/S/A/G`, **Fn must be pressed first**. Pressing `c` and then Fn registers a plain `C` (assigning the C key) before Fn is seen.

## 4. Build results (2026-10-13)

| Item | Result |
|---|---|
| Environment | arduino-cli 1.5.1; `m5stack:esp32` 3.3.9; board M5Cardputer; M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2 |
| Settings | USB-OTG (TinyUSB), CDC On Boot Enabled (`…:USBMode=default,CDCOnBoot=cdc`) |
| Result | ✅ exit 0 on the first attempt. Flash 603,323 B (46%), RAM 60,172 B (18%). App-only binary 603,472 B (1e-1: 598,880 B) |
| Warnings (`--warnings all`) | ✅ 0 from the sketch; the known library `KEY_TAB` redefinition warning remains |
| Guards | ✅ `#error` still stops `USBMode=hwcdc` and `CDCOnBoot=default` builds |

**A successful build is not a hardware result.** Nothing in §6 has been run.

## 5. Safety review

| Requirement | How it is met (source) |
|---|---|
| Configuration keystrokes never reach the host | While the UI is open, `pollKeyboard()` sends every key to `processSetupKeys()` and returns. No UI handler calls `pushEvent()`, `hidOut` or `UsbKeyboard`. The only HID-producing calls in the UI's lifetime are the release steps below |
| Encoder operations in the UI never reach the host | `onRotation()` and `onButton()` return to the UI handlers **before** any profile branch while the UI is open |
| Entering the UI releases any active key or mouse button | `enterSetup()` runs the same clean-up as a profile switch: clear the tap queue and pending movement, release the mouse button and request a resend (`mouseSyncNeeded`), and request an all-keys-up keyboard report (`releasePending`). These are **release** reports, not keystrokes; one all-keys-up report is sent even if nothing was held |
| Cancel discards uncommitted changes | `exitSetup(false)` leaves `customSaved` untouched; the draft is overwritten from `customSaved` the next time the UI opens |
| Save commits explicitly | Only `exitSetup(true)` copies the draft to `customSaved` and rebuilds the send-path form |
| No stale input after leaving the UI | `exitSetup()` clears the tap queue and both rotation accumulators |
| TAP only | Profile 4 uses the existing tap path (press, then all-keys-up, with retry of a failed release). No key is held |
| Profiles 1–3 unchanged | Byte-identical vs 1e-1: `setup`, `onUsbEvent`, `updateUsbState`, `sendKeyReport`, `sendRelease`, `tapKey`, `sendQueued`, `pushEvent`, `sendMouse`, `setMouseButton`, `setProfile`, `changeSensitivity`, `readRegChecked`, `drawSpinnerFull`, `drawSpinnerDiag`, `drawSpinnerSensitivity`. Other edits are guarded Profile 4 / UI branches (§3.2) |
| USB HID descriptors, reconnect handling, Launcher method | Unchanged (same functions as above; no descriptor or HID object change) |
| No NVS, no partition change | No `Preferences`/`nvs_*` calls added |
| U135 lost or USB lost while the UI is open | The existing handlers still release everything. The UI keeps working on the keyboard; the knob does nothing until the U135 is found again |

## 6. Proposed hardware test checklist (not run yet)

Install by **Launcher Wi-Fi upload** of `MediaJog_1e_2/build/MediaJog_1e_2.ino.bin` only. **Record the SHA-256 of the uploaded file.** Keep 1e-1 available in the Launcher to roll back. Keep a text editor focused on the Mac during every test, so that any leak would be visible.

| # | Test | Pass condition | Result |
|---|---|---|---|
| 1 | Install via the Launcher; Launcher still present | App starts; Launcher still there afterwards | ❓ |
| 2 | Profiles 1–3 regression | Profile 1 `.` `,` `k`; Profile 2 pointer, drag, `Fn+;`/`Fn+.`; Profile 3 Right/Left/Space | ❓ |
| 3 | `Fn+4` before any setup | `PROFILE 4 CUSTOM`, all three `(none)`, `NOT CONFIGURED`; turning and pressing send nothing | ❓ |
| 4 | `Fn+S` in Profiles 1, 2, 3 | Nothing happens | ❓ |
| 5 | `Fn+S` in Profile 4 | CUSTOM SETUP opens | ❓ |
| 6 | **Leak test, whole session** | During tests 5–14, **nothing** appears in the text editor and the pointer does not move | ❓ |
| 7 | Release on entry | In Profile 2 hold a drag, switch to Profile 4 with `Fn+4` (drag ends), then `Fn+S`. Also: press `Fn+S` while turning the knob in Profile 4 with an action assigned → no further taps after the screen opens | ❓ |
| 8 | Select actions | `1`/`2`/`3` open EDIT; turning moves the highlight; knob press opens EDIT for the highlighted row | ❓ |
| 9 | Typed key | In EDIT, type `j` → `J`; type `5` → `5`; hold Shift and type `1` → shows `1`, not `!` | ❓ |
| 10 | Key list | Turning in EDIT steps Right → Left → Up … `` ` `` and wraps; position `n/28` shown | ❓ |
| 11 | Modifiers | Fn first, then `C`/`S`/`A`/`G` toggles Ctl/Sft/Alt/Cmd (green/grey). Pressing `c` before Fn assigns C (documented behavior) | ❓ |
| 12 | Clear, undo, done | del clears to `(none)`; esc restores the value from when EDIT opened; ok or knob press returns to screen 1 | ❓ |
| 13 | **Cancel** | Change something, press esc on screen 1 → Profile 4 shows the old mapping, and the output uses the old mapping | ❓ |
| 14 | **Save** | Set CW = Cmd+Right, CCW = Cmd+Left, BTN = Space, press ok → status screen shows them | ❓ |
| 15 | Profile 4 output | In a text editor: CW moves to the end of the line (Cmd+Right on macOS), CCW to the start, press inserts a space. One tap per detent / press | ❓ |
| 16 | Other modifiers | e.g. Ctrl+A, Shift+Tab, Alt+Left: the host receives exactly the combination (check in a keyboard viewer if available) | ❓ |
| 17 | Profile switching after Save | `Fn+1`…`Fn+3` and back to `Fn+4` keep the mapping (RAM) | ❓ |
| 18 | Power cycle | Custom mapping is **lost** and the device starts in Profile 1 (expected: no persistence in 1e-2) | ❓ |
| 19 | Optional | Inconclusive 1e-1 item: fast rotation then switching (see `STAGE_1E_1.md` §10-2) | ❓ |

## 7. Unverified assumptions

- The edge-detection approach (§3.4) behaves as expected with the real keyboard timing (one TCA8418 event per loop).
- `Fn+S`, `Fn+C`, `Fn+A`, `Fn+G` are delivered like the hardware-verified `Fn+1`…`Fn+4` and `Fn+;` (same mechanism; not tested for these letters).
- The `` ` `` key (esc legend) and Enter (ok) produce `` ` `` in `word` and the `enter` flag on the ADV (source and keycap image; not tested).
- The host interprets GUI as Command and Alt as Option on macOS (standard HID behavior; not tested here).
- Readability of size-1 text on the ADV screen (layout calculated, not viewed).

## 8. Known limitations

- **RAM only:** the Custom mapping is lost at power-off and the device restarts in Profile 1. Persistence follows the agreed policy in `DEVELOPMENT_PLAN.md` §6.9 in Stage 1e-4: profile saved immediately on a confirmed switch, Custom saved on explicit Save, sensitivity after a debounce interval.
- **TAP only;** no HOLD (Stage 1e-3). Modifier-only actions (e.g. holding Shift alone) are not supported: an action needs a key.
- One key per action; no sequences or macros; no consumer/media keys.
- Enter, Backspace and `` ` `` can be assigned only from the key list (they operate the UI).
- Modifier toggles need Fn pressed first (§3.4).
- Opening the setup screen sends one all-keys-up keyboard report and a mouse button-up report. These are releases, not keystrokes.
- The USB replug / `USB SUSPENDED` issue and intermittent first recognition are unchanged.

Preservation: `MediaJog_1a`–`MediaJog_1e_1`, `U135_Probe/`, `archive/`, the licenses, and the hardware and image files were not modified (SHA-256 compared before and after). No device was flashed, no Git repository was initialized, nothing was published.

---

## 9. Hardware test 1 (2026-10-13): FAILED, and the investigation

### 9-1. Observed (author report)

- `Fn+4` selects `PROFILE 4 CUSTOM`.
- The screen shows `NOT CONFIGURED`.
- **Pressing `Fn+S` does not open CUSTOM SETUP; no configuration screen appears.**
- Not recorded: the SHA-256 of the uploaded binary; the other lines of the Profile 4 screen; the Spinner screen header.

§6 items 5 and later could not be run. Item 3 (`Fn+4` and `NOT CONFIGURED`) matches what was seen, but see §9-3 on which build was running.

### 9-2. Source investigation (installed M5Cardputer 1.1.1)

**Does `word` contain ordinary characters while Fn is held? Yes.**
- `Keyboard_Class::updateKeysState()` (`Keyboard.cpp:90-210`) has **no Fn layer**. Pass 1 only sets `fn = true` for the Fn key. Pass 2 skips the modifier keys and pushes every other printable key's **first legend** into `word`. The second legend is used only when Ctrl, Shift or CapsLock is active (`Keyboard.cpp:202-207`). Fn is not part of that condition.
- So `Fn+S` gives `fn = true`, `word = {'s'}`, by exactly the same code path as the hardware-verified `Fn+4` (`fn = true`, `word = {'4'}`) and `Fn+;`.
- The TCA8418 reader (`TCA8418.cpp`) and `updateKeyList()` do not filter or remap letter keys differently from digits. `'s'` is at `_key_value_map[2][3]` (`Keyboard.h:49`).
- **The library source does not explain the failure.**

**Does the 1e-2 sketch handle `Fn+S` correctly? As far as the source shows, yes:**
1. `pollKeyboard()` maps `'s'` with Fn to `CHORD_SETUP`. The "exactly one recognised character" rule is satisfied by `Fn+S` alone.
2. On the chord's rising edge, `case CHORD_SETUP` calls `enterSetup()` if `profile == PROFILE_CUSTOM`.
3. `enterSetup()` sets `uiMode = UI_SETUP`.
4. On the next loop, `loop()` selects `LAYOUT_SETUP` and draws CUSTOM SETUP within `DRAW_INTERVAL_MS`.
5. No code path leaves the setup UI without Enter or `` ` ``. The keys held while opening (`Fn`, `s`) are taken as the starting state, so they cannot trigger an exit.
6. Sequences such as `Fn` held → `4` released → `s` pressed also reach `CHORD_SETUP`, because releasing `4` resets the chord to NONE first.

**Conflicts with other handling:**
- In normal mode, `Fn+S` is used only for `CHORD_SETUP`.
- Inside the UI, `Fn+S` toggles Shift (EDIT screen) or is ignored (overview). It cannot re-open or close the UI. Inside the UI, `pollKeyboard()` returns before any chord handling.
- No conflict found.

**Behavior of `Fn+C/S/A/G` inside the UI:**
- **EDIT screen:** a new `'c'`/`'s'`/`'a'`/`'g'` with Fn held toggles Ctrl/Shift/Alt/GUI.
- **Overview:** they are ignored.
- As documented in §3.4, Fn must be pressed **before** the letter. If the letter comes first, it is assigned as a key.

### 9-3. Findings and hypotheses (not established)

- **Defect found (confirmed in the binary):**
  - The 1e-2 build still prints **`MEDIA JOG 1e-1`** on the Spinner screen, because `drawSpinnerFull()` was deliberately kept byte-identical to 1e-1 and its label was not updated.
  - The 1e-2 binary contains `MEDIA JOG 1e-1` and not `MEDIA JOG 1e-2` (checked with `grep -a` on the `.bin`).
  - So the running build could not be identified from the Spinner screen. This defect does **not** explain `Fn+S` failing.
- **Hypothesis A — the device was running 1e-1, not 1e-2.**
  - This would explain both observations: `Fn+4` exists in 1e-1, `Fn+S` does not.
  - The two builds' Profile 4 screens differ: 1e-1 shows `VAL …` and `no output (n)`; 1e-2 shows `CW : (none)` / `CCW: (none)` / `BTN: (none)` and **`Fn+S: SETUP`** under `NOT CONFIGURED`. `no output (` is present only in the 1e-1 binary; `Fn+S: SETUP` and `CUSTOM SETUP` only in the 1e-2 binary.
  - The report mentions only `NOT CONFIGURED`, so this cannot be decided from it.
- **Hypothesis B — on this ADV unit, the `S` key with Fn is not delivered as `'s'`** (hardware or matrix-mapping behavior not visible in the source). Not supported by the source, but it cannot be excluded without observing the key state.
- **No code change is made to the `Fn+S` logic until A or B is established.** Changing the entry key on a guess could hide the real cause.

## 10. Revision r2 (`MediaJog_1e_2_r2/`) — identification and diagnostics only

**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) review.**

### 10-1. Changes (37 insertions, 3 deletions vs `MediaJog_1e_2.ino`)

| Change | Purpose |
|---|---|
| `BUILD_TAG = "1e-2r2"` drawn in grey at the top-right (x 204–240) of the status screens (all profiles) and of both setup screens | Identify the running build on the device. The longest size-2 first line ends before x = 190, so nothing overlaps |
| Spinner header `MEDIA JOG 1e-1` → `MEDIA JOG 1e-2r2` | Fixes the stale label (§9-3) |
| Profile 4 status diagnostics line shows the live key state, e.g. **`key:Fn+s`** (was `T` and `fn` counters) | Shows whether `Fn+S` reaches the firmware as Fn + `'s'` (hypothesis B) |
| Serial log `[KB] Fn chord <n> in profile <p>` for every recognised chord (`CHORD_SETUP` = 7) | Diagnostic when a serial monitor is connected |

**Unchanged:** byte-identical to 1e-2 are `setup`, `loop`, all USB/HID functions (`onUsbEvent`, `updateUsbState`, `sendKeyReport`, `sendRelease`, `tapKey`, `sendQueued`, `pushEvent`, `sendMouse`, `setMouseButton`), `setProfile`, `changeSensitivity`, `readRegChecked`, `actionFor`, `onRotation`, `onButton`, `pushCustom`, and the entire setup logic (`enterSetup`, `exitSetup`, `startEdit`, `finishEdit`, `setupStep`, `setupRotate`, `handleSetupKey`, `processSetupKeys`, `snapshotSetupKeys`, `baseChar`, `usageForChar`). **HID behavior and the `Fn+S` logic are therefore exactly those of 1e-2.** Profiles 1–3 behave as in 1e-1; only their status screen gains the build tag.

### 10-2. Build and hashes (2026-10-13)

| Item | Result |
|---|---|
| Build | ✅ exit 0, same environment and settings as 1e-2. Flash 603,475 B (46%), RAM 60,172 B (18%). No sketch warnings (known library `KEY_TAB` warning only). `#error` guards still effective |
| `MediaJog_1e_2_r2/MediaJog_1e_2_r2.ino` | 61,618 B, SHA-256 `ae8d062d6c11945a1bbf8c4861514268623ed2d9657ae82a4038df270c1bd16f` |
| `MediaJog_1e_2_r2/build/MediaJog_1e_2_r2.ino.bin` | 603,616 B, SHA-256 `d657f465951f3eb4e2522443011c4e2486f77a6da3cda5fd79855fee026e30ae` |
| Binary check | Contains `1e-2r2`, `key:%s%c`, `[KB] Fn chord`, `Fn+S: SETUP`; no longer contains `MEDIA JOG 1e-1` |

Both hashes are recorded in `MediaJog_1e_2_r2/SHA256SUMS.txt`. `MediaJog_1e_2/` is kept unchanged as the build used in hardware test 1.

### 10-3. Hardware test for r2 (proposed; decides between hypotheses A and B)

Before testing:
- **Delete or rename the older MediaJog builds in the Launcher**, or make sure the selected app is `MediaJog_1e_2_r2`.
- **Record the SHA-256 of the uploaded file** (it should be `d657f465…26e30ae`).
- Keep a text editor focused on the Mac.

| Step | Do | Look for |
|---|---|---|
| 1 | Start the app | Grey **`1e-2r2`** at the top-right. If it is missing, a different build is running → stop and record what is shown |
| 2 | `Fn+4` | `PROFILE 4 CUSTOM`, three lines `CW : (none)` / `CCW: (none)` / `BTN: (none)`, `NOT CONFIGURED`, **`Fn+S: SETUP`** |
| 3 | Hold Fn, then press S (keep both held) | **Expected:** CUSTOM SETUP opens at once, while the keys are still held (within about 0.1 s) |
| 4 | Only if setup did **not** open: keep holding `Fn+S` and read the bottom line of the Profile 4 screen | It should read **`key:Fn+s`** (and `KB2`). Write down exactly what it shows. Then compare with `Fn+4` (should show `key:Fn+4`; already in Profile 4, so nothing else changes) and with plain `S` (should show `key:s`) |
| 5 | If the setup screen opened | Continue with the original checklist §6, items 6–19 |

**How to read the result:**

| Observation | Meaning | Next action |
|---|---|---|
| Tag `1e-2r2` shown and CUSTOM SETUP opens | Hypothesis A is likely: test 1 probably ran another build | Continue §6. Record the uploaded hash every time |
| Setup does not open, and step 4 shows `key:Fn+s` | A firmware logic defect that the source review missed | Stop; report the screen and, if possible, the serial log (a `[KB] Fn chord 7 in profile 4` line means the chord was recognised) |
| Setup does not open, and step 4 shows something other than `key:Fn+s` (no `Fn+`, another letter, or nothing) | Hypothesis B: the key is not delivered as Fn + `'s'` on this unit | Stop; report exactly what `key:` shows for `Fn+S`, for plain `S`, and for `Fn+4`. A different entry key can then be chosen from verified keys |
| Tag `1e-2r2` not shown | Wrong build running | Re-install and verify the uploaded hash |

## 11. r2 hardware test (2026-10-13, reported by the author)

**Build:** the `1e-2r2` build tag was visible, so the r2 firmware was running. The SHA-256 of the uploaded file was **not recorded**.

### 11-1. Hardware-verified behavior (observed on the device and the Mac)

| # | Observation |
|---|---|
| H1 | The `1e-2r2` build tag is visible |
| H2 | `Fn+4` selects Profile 4 |
| H3 | **`Fn+S` opens CUSTOM SETUP** |
| H4 | CW can be assigned the key A and saved |
| H5 | Turning clockwise then sends `a` to the Mac |
| H6 | CW, CCW and BTN can be independently assigned A, B and C |
| H7 | All three actions produce the expected HID output |
| H8 | CANCEL discards unsaved changes |
| H9 | `Fn+G` adds the GUI/Command modifier |
| H10 | Command+A (from the Custom profile) selects text on macOS |
| H11 | No unintended host typing was observed during setup |
| H12 | Turning the knob in EDIT selects special keys from the list |
| H13 | Custom assignments survive switching between Profiles 1–4 while powered |
| H14 | After power-off and reboot, the device starts in Profile 1 and the Custom assignments are cleared (expected: no persistence before Stage 1e-4) |

**Hardware test 1 (§9) in the light of this:**
- With r2, `Fn+S` opens setup. The `Fn+S` code is byte-identical in 1e-2 and r2.
- So the most consistent explanation of the test-1 failure is **hypothesis A** (§9-3): a different build was running.
- This remains an inference, not a proven cause, because the binary used in test 1 was never identified.
- Hypothesis B (key not delivered) is **not supported**: `Fn+S` worked on the same unit.

### 11-2. Status of the 19-item checklist (§6) after this test

Statuses: **PASS** (hardware-verified, ref. §11-1) · **partial** (only part of the item was observed) · **not tested** (no observation reported).

| # | Item | Status |
|---|---|---|
| 1 | Install via Launcher; Launcher still present | Correct build confirmed by the tag (H1). "Launcher still present afterwards" and the uploaded hash: **not recorded** |
| 2 | Profiles 1–3 regression | **Not tested** in this session (only switching between profiles is reported, H13) |
| 3 | Profile 4 before setup: `(none)`, `NOT CONFIGURED`, no output | **Partial:** `Fn+4` selects Profile 4 (H2); the screen contents and silence before setup were not reported this time |
| 4 | `Fn+S` in Profiles 1–3 does nothing | **Not tested** |
| 5 | `Fn+S` in Profile 4 opens setup | **PASS** (H3) |
| 6 | No leak during the whole setup session | **PASS** for typing (H11). Pointer movement not reported |
| 7 | Release on entering setup | **Not tested** |
| 8 | Selecting actions (`1`/`2`/`3`, knob) | **Partial:** all three actions could be assigned (H6); which selection method was used was not recorded |
| 9 | Typed key (incl. Shift held while typing) | **Partial:** letters A, B, C (H4, H6). Digits and the Shift-held case not tested |
| 10 | Key list with the knob (wrap, `n/28`) | **Partial:** special keys selectable (H12); wrap and position display not recorded |
| 11 | Modifier toggles (`Fn+C/S/A/G`, Fn first) | **Partial:** `Fn+G` (H9). `Fn+C`, `Fn+S` (Shift), `Fn+A` and the letter-before-Fn case not tested |
| 12 | Clear (del), undo (esc in EDIT), done | **Not tested** |
| 13 | Cancel | **PASS** (H8) |
| 14 | Save | **PASS** (H4, H6) |
| 15 | Profile 4 output, one tap per detent/press | **PASS** for letters (H5, H7) and Cmd+A (H10). The Cmd+Right / Cmd+Left / Space example was not tested; one-tap-per-detent was not explicitly counted |
| 16 | Other modifier combinations | **Partial:** Command only (H10). Ctrl, Shift, Alt not tested |
| 17 | Mapping survives profile switching | **PASS** (H13) |
| 18 | Power cycle clears the mapping, starts in Profile 1 | **PASS** (H14, expected behavior) |
| 19 | Optional fast-rotation retest | **Not tested** |

**Summary:** 6 items PASS (5, 13, 14, 15, 17, 18) and item 6 PASS for typing only; 7 items partial (1, 3, 8, 9, 10, 11, 16); 5 items not tested (2, 4, 7, 12, 19). **The checklist as a whole is not passed.**

### 11-3. Source/build verification (not hardware)

These were established before the test, and the hardware results don't change them:
- The `Fn+S` path, the release on entering setup, and the leak-proof design (§5) were checked in the source.
- `baseChar()`/`usageForChar()` were host-tested against the library table (§3.3).
- r2 builds without warnings, and its HID functions are byte-identical to 1e-2 (§10).

### 11-4. Host-side note: macOS Japanese input

- **Observation (author):** with macOS Japanese input active, ordinary letter keys sent by the Custom profile may start a partial Japanese composition (IME).
- **Assessment:** expected host behavior. The device sends HID usage IDs (physical keys), and the active input method decides what they produce, as it would for a physical keyboard. This is **not a confirmed firmware defect**.
- **Decision:** the firmware will **not** try to switch or force the host's input method. Users who want literal letters should switch the Mac to an English input source. Shortcuts with Command (e.g. Command+A) worked (H10).

## 12. Revision r3 (`MediaJog_1e_2_r3/`) — larger key names on the overview screen

**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) review.**

### 12-1. Change

Only the CUSTOM SETUP **overview** rows (CW / CCW / BTN) changed. Before, the assigned key was always drawn at text size 1 (6×8 px) at x = 84. r3 draws it at the **largest of sizes 2, 1.5 and 1 whose measured width (`textWidth()`) fits the row**:

| Item | Value |
|---|---|
| Key name area | x = 72 to 236 (164 px). The row labels end at x = 64 (`2 CCW` at size 2); 8 px gap |
| Size 2 (16 px high, 12 px per character) | up to 13 characters, e.g. `A`, `Cmd+A`, `Ctl+Right`, `Cmd+PageDown`, `Ctl+Sft+Right` |
| Size 1.5 (12 px high, about 9 px per character) | 14–18 characters, e.g. `Ctl+Sft+PageDown` |
| Size 1 (8 px high, 6 px per character) | 19–25 characters, e.g. `Ctl+Sft+Alt+Cmd+F12`, `Ctl+Sft+Alt+Cmd+Backspace` (150 px) |
| Vertical placement | Inside the existing 17 px row highlight (y−1 … y+15): size 2 at y, 1.5 at y+2, 1 at y+4. Rows are 18 px apart, so text never reaches the next row; the help text starts at y = 80, below the last row (ends y = 73) |
| Longest possible name | 25 characters (`formatKey()`: 16 for all four modifiers + up to 9 for the key name), 150 px at size 1: always fits, never clipped |

- The text uses the row's own colors (yellow on navy for the selected row, white on black otherwise), so the **selected-row highlight is preserved**.
- Size 1.5 uses M5GFX's scaling of the built-in font; glyph strokes may look slightly uneven at that size. Readability is to be judged on the device.

### 12-2. Unchanged

- **Source check:** of the 47 static functions plus `setup()` and `loop()`, only `drawSetupScreen()` differs from r2, and within it only the overview-row loop.
- **Therefore unchanged:** the EDIT screen, the Profile 4 normal screen, all key handling, the setup logic, all USB HID functions and safety logic, and Profiles 1–3.
- **Build tag:** `1e-2r3`, still drawn on every screen by the same `drawBuildTag()` and Spinner header code as r2.

### 12-3. Build and hashes (2026-10-13)

| Item | Result |
|---|---|
| Build | ✅ exit 0, same environment and settings. Flash 603,867 B (46%), RAM 60,172 B (18%). No sketch warnings (known library `KEY_TAB` warning only). `#error` guards still effective |
| Diff vs r2 | 29 insertions, 4 deletions (header note, build tag, overview-row drawing) |
| `MediaJog_1e_2_r3/MediaJog_1e_2_r3.ino` | 63,136 B, SHA-256 `37f0698a2469e21b34b561587f47aacc036bb8453a2148754f02d37b160fc06c` |
| `MediaJog_1e_2_r3/build/MediaJog_1e_2_r3.ino.bin` | 604,016 B, SHA-256 `a448c9574f9d2282bd15d6f1bdbd1b132678594786196528347a191ed35f447e` |
| Binary check | Contains `1e-2r3`; no longer contains `1e-2r2` |

Both hashes are recorded in `MediaJog_1e_2_r3/SHA256SUMS.txt`. r2 and all earlier builds are unchanged.

### 12-4. Hardware test for r3 (overview screen only; proposed, not run)

Install `MediaJog_1e_2_r3.ino.bin` via the Launcher and **record its SHA-256** (`a448c957…f447e`).

| # | Do | Pass condition |
|---|---|---|
| R1 | Start the app; `Fn+4`, `Fn+S` | `1e-2r3` tag visible top-right; CUSTOM SETUP opens |
| R2 | Look at the three rows with all actions unassigned | `(none)` shown large (size 2) in each row, not overlapping the labels |
| R3 | Assign CW = `A`, CCW = `Right` (list), BTN = `Space` (list) | All three shown at size 2 |
| R4 | Assign one action **Ctl+Sft+PageDown** (Fn+C, Fn+S, list) | Shown at the medium size (1.5), fully visible, legible |
| R5 | Assign one action **Ctl+Sft+Alt+Cmd+Backspace** (all four Fn toggles, Backspace from the list) | Shown at the small size, completely visible up to the final `e`, not clipped at the right edge |
| R6 | Move the highlight over each row (knob) | The navy highlight covers the whole key text of the selected row; no text from one row overlaps the row above or below; help lines below are intact |
| R7 | Open EDIT (`1`) and return; then Save, and check the Profile 4 normal screen | EDIT screen and Profile 4 screen look exactly as in r2 (only the tag reads `1e-2r3`) |
| R8 | Quick regression | Saved Custom keys still produce the expected HID output; no typing on the Mac during setup |
