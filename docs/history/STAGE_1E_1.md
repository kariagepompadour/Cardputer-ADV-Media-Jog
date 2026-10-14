# Stage 1e-1 — Four profiles (implementation, build, test plan)

Created: 2026-10-13 · Implementation: Claude Opus 5.5
**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) review before hardware testing.**

> **Status update (2026-10-13, appended; the line above is the status at build time):** Installed through the Launcher and tested on a Mac by the author. **Ten checks passed, one was inconclusive and several were not recorded.** Not every test passed or was run. See **§10**.

Legend: ✅ checked in source or build · ❓ not verified on hardware

Stage 1e-1 is the first sub-stage of the Stage 1e plan (`DEVELOPMENT_PLAN.md` §6). It adds profiles 3 and 4 and nothing else. **There is no configuration UI and no NVS persistence in this stage**; the device still starts in Profile 1 with sensitivity 8 after every power-on.

## 1. Deliverables and hashes

| File | Size | SHA-256 |
|---|---|---|
| `MediaJog_1e_1/MediaJog_1e_1.ino` | 37,589 B | `38a695350d8ce169162db8a49e49f83cabe64049f13c7e6953dc03cb08b6ff3e` |
| `MediaJog_1e_1/build/MediaJog_1e_1.ino.bin` (app-only, for Launcher Wi-Fi upload) | 598,880 B | `31374dc0a292a5780718fc63c95cc360cb242a02e9077e11e3feb659933bbd8b` |

- Both are recorded in `MediaJog_1e_1/SHA256SUMS.txt` (paths relative to the repository root). `shasum -a 256 -c MediaJog_1e_1/SHA256SUMS.txt` → both OK.
- Other files in `build/` (merged/bootloader/partition images, ELF, map, cache) are not for use and are excluded by `.gitignore`.
- Starting point: `MediaJog_1d/MediaJog_1d.ino` (SHA-256 `3b8c5667…75ac30c6`), copied unchanged and then edited. Stage 1d and all earlier milestones are untouched (§7).

## 2. Behavior

| Input | Profile 1 YouTube | Profile 2 Spinner | Profile 3 Video Editing (new) | Profile 4 Custom (new) |
|---|---|---|---|---|
| Clockwise, per detent | `.` (0x37) | mouse +X × sensitivity | **Right Arrow (0x4F)** | nothing |
| Counterclockwise, per detent | `,` (0x36) | mouse −X × sensitivity | **Left Arrow (0x50)** | nothing |
| Knob press | `k` (0x0E), one tap | left button held until release | **Space (0x2C)**, one tap | nothing |
| `Fn+;` / `Fn+.` | — | sensitivity up/down (14 steps) | — | — |

- **Profile selection:** `Fn+1` YouTube, `Fn+2` Spinner, `Fn+3` Video Editing, `Fn+4` Custom. Each acts once per press. Selecting the current profile again does nothing.
- **Profile 3:** proposed defaults for Adobe Premiere Pro and Apple Final Cut Pro (arrow = one frame, Space = play/stop). **Not tested in either application; no compatibility is claimed.** All three keys are sent without modifiers, as single taps (press + all-keys-up), exactly like Profile 1.
- **Profile 4:** a safe placeholder. Rotation and knob press produce **no HID output**. The screen shows `NOT CONFIGURED` and `no output (n)`, where n counts ignored inputs.
- **Profiles 1 and 2:** unchanged from Stage 1d, including the 14 sensitivity steps, the `Fn+;`/`Fn+.` controls, mouse-button safety and all USB handling (§4).

### 2.1 Screens

- **Profiles 1, 3 and 4** use the Stage 1d status screen. Line 2 reads `PROFILE 1 YOUTUBE`, `PROFILE 3 EDITING` or `PROFILE 4 CUSTOM`. Line 4 shows:
  - Profile 1: the tap counters (unchanged from 1d).
  - Profile 3: the mapping `Right/Left/Space`. The 1d tap counters count since boot and are shared with Profile 1, so they are not shown here.
  - Profile 4: `NOT CONFIGURED`, with `no output (n)` on the next line.
- **Profile 2** uses the Stage 1d spinner screen; only its header text changed, from `MEDIA JOG 1d` to `MEDIA JOG 1e-1`.

## 3. Implementation details (diff summary)

`diff -u MediaJog_1d/MediaJog_1d.ino MediaJog_1e_1/MediaJog_1e_1.ino`: 1 file, 97 insertions, 14 deletions.

| Change | Where |
|---|---|
| Header comment describes Stage 1e-1 | top of file |
| `EDIT_CW` / `EDIT_CCW` / `EDIT_PRESS` = Right Arrow / Left Arrow / Space, each checked with `static_assert` against TinyUSB's `HID_KEY_ARROW_RIGHT` (0x4F), `HID_KEY_ARROW_LEFT` (0x50), `HID_KEY_SPACE` (0x2C) (`class/hid/hid.h:433, 468-469`) | after the Profile 1 actions |
| `enum ProfileId` (1–4). The `profile` variable is moved above `actionFor()`, which now uses it | same place |
| `profileName()` for the status line. Placed **after** the `Event`/`TapResult` enums, because the Arduino builder inserts prototypes above the first function (the first build attempt failed for exactly this reason) | before `actionFor()` |
| `actionFor(ev)` returns the Profile 3 keys when `profile == PROFILE_EDIT`, and otherwise the 1d keys (unchanged code path for Profile 1) | `actionFor()` |
| `Chord` gains `CHORD_P3`/`CHORD_P4`; `static_assert` ties chord values to profile numbers (with casts, to avoid `-Wenum-compare`) | chord enum |
| `pollKeyboard()` recognizes `'3'`/`'4'` with Fn; the switch selects profiles 1–4 | `pollKeyboard()` |
| `onRotation()` / `onButton()`: Profile 4 returns without output (counts `customIgnored`). Profile 3 shares the Profile 1 path (one tap per detent, one tap per press) | input handlers |
| Status screen: profile name from `profileName()`, Profile 3/4 line 4 as described in §2.1 | `drawStatus()` |
| Spinner screen header text | `drawSpinnerFull()` |

**Why events are mapped when sent:** the keyboard queue stores `EV_CW`/`EV_CCW`/`EV_PRESS`, and `actionFor()` converts them to keys at send time using the current profile. This is safe because only Profiles 1 and 3 queue events, and `setProfile()` clears the queue on every switch. The profile switch and the sends happen in the same `loop()` pass, in the order `pollKeyboard()` → `pollUnit()` → `sendMouse()`/`sendQueued()`. So a queued event is always sent with the mapping of the profile that produced it.

## 4. Source review: what did NOT change

The following were compared function by function with Stage 1d. Their bodies are **byte-identical**:

`setup()`, `loop()`, `onUsbEvent()`, `updateUsbState()`, `sendKeyReport()`, `sendRelease()`, `tapKey()`, `sendQueued()`, `pushEvent()`, `sendMouse()`, `setMouseButton()`, `setProfile()`, `changeSensitivity()`, `readRegChecked()`.

No changed line in the diff touches `SendReport`, `hidOut`, `USB.*`, the release logic, the mouse sync logic or `usbReady`. Consequences:

- **USB HID initialization, report format (Report ID 1 keyboard, 2 mouse), error handling and reconnect behavior are unchanged.** The USB replug / `USB SUSPENDED` issue is therefore also unchanged.
- **Mouse button safety is unchanged:**
  - `setProfile()` still releases the mouse button, requests a state resend, and drops pending movement and queued key taps on every switch.
  - Leaving Spinner for Profile 3 or 4 therefore releases a held button exactly as leaving it for Profile 1 did in 1d.
- **No stale keypress can survive a profile change:**
  - Keyboard output is still tap-only (press, then all-keys-up). A failed all-keys-up report is retried until confirmed.
  - The queue is cleared on switch.
  - There is no HOLD behavior in this stage.
- **Cardputer keys remain local:** the only new keyboard handling is recognizing `'3'`/`'4'` in the Fn chord; nothing in `pollKeyboard()` produces HID output.
- **No NVS access was added.** The sketch does not call `Preferences` or any `nvs_*` function. The core's own NVS initialization at boot and M5GFX's existing board-detection entry are unchanged from 1d (`DEVELOPMENT_PLAN.md` §6.5, R-1/R-4). No partition settings were changed.

### 4.1 Key availability (`Fn+3`, `Fn+4`)

- M5Cardputer 1.1.1 maps the number row as `'1'`, `'2'`, `'3'`, `'4'` (`Keyboard.h:19-22`). Fn does not change the character (`Keyboard.cpp:202-207`).
- So `Fn+3`/`Fn+4` reach the sketch as `fn == true` plus `'3'`/`'4'` in `keysState().word`. This is the same mechanism as `Fn+1`/`Fn+2`, which were verified on hardware in Stage 1b. ✅ source
- **Still ❓ on hardware:** whether the ADV's key matrix remapping (`TCA8418.cpp` `remap()`) delivers `3`/`4` correctly. The author will verify this.

## 5. Build results (2026-10-13)

| Item | Result |
|---|---|
| Environment | arduino-cli 1.5.1; board package `m5stack:esp32` 3.3.9; board M5Cardputer; M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2 |
| Settings | USB Mode = USB-OTG (TinyUSB), USB CDC On Boot = Enabled (FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc`) |
| Result | ✅ exit 0. Flash 598,735 B (45%), RAM 60,036 B (18%). App-only binary 598,880 B (Stage 1d: 598,592 B) |
| Warnings (`--warnings all`) | ✅ 0 from the sketch. The known library-level `KEY_TAB` redefinition warning remains (as in 1b–1d) |
| Guards | ✅ `#error` still stops builds with `USBMode=hwcdc` or `CDCOnBoot=default` |
| First attempt | Failed: `profileName()` was the first function, so the generated prototypes landed above the `Event` enum. Fixed by moving it (§3) |

**A successful build is not a hardware result.** Nothing in §6 has been run.

## 6. Proposed hardware test checklist (not run yet)

Install by **Launcher Wi-Fi upload** of `MediaJog_1e_1/build/MediaJog_1e_1.ino.bin` only (no merged image, no IDE upload). **Record the SHA-256 of the exact file uploaded.** Keep the Stage 1d binary available in the Launcher to roll back.

| # | Test | Pass condition | Result |
|---|---|---|---|
| 1 | Install and start through the Launcher | App starts; Launcher still present afterwards | ❓ |
| 2 | Boot state | `PROFILE 1 YOUTUBE`; `USB HID READY` after connecting | ❓ |
| 3 | Profile 1 regression | YouTube `.` / `,` / `k` as in Stage 1d | ❓ |
| 4 | `Fn+2`, Profile 2 regression | Spinner screen shows `MEDIA JOG 1e-1`; pointer movement; `Fn+;`/`Fn+.` change the value 1–128; drag with knob held | ❓ |
| 5 | **`Fn+3`** | `PROFILE 3 EDITING`, line 4 `Right/Left/Space` | ❓ |
| 6 | Profile 3 output in a text editor | Clockwise moves the cursor right one character per detent, counterclockwise left, press inserts one space | ❓ |
| 7 | **`Fn+4`** | `PROFILE 4 CUSTOM`, `NOT CONFIGURED`, `no output (0)` | ❓ |
| 8 | Profile 4 silence | Turning and pressing produce **nothing** on the host (text editor focused); the counter increases | ❓ |
| 9 | Leaving Spinner while dragging | Hold knob in Profile 2 (drag in Finder), press `Fn+3` → drag ends; repeat with `Fn+4` and `Fn+1` | ❓ |
| 10 | Switching during rotation | Turn quickly in Profile 1 or 3 and switch profiles meanwhile → no extra or stuck key on the host | ❓ |
| 11 | Profile keys stay local | With a text editor focused, `Fn+1`…`Fn+4`, `Fn+;`, `Fn+.` and other Cardputer keys type nothing | ❓ |
| 12 | Repeated selection | Pressing the current profile's key again changes nothing (`chg` counter unchanged) | ❓ |
| 13 | Optional, only if the author wants: Premiere Pro / Final Cut Pro | Record app, version and focused panel. Arrow = one frame and Space = play/stop are **expected**, not claimed | ❓ |
| 14 | Power cycle | Always restarts in Profile 1 with sensitivity 8 (no persistence in 1e-1; this is expected) | ❓ |

## 7. Preservation

- Not modified: `MediaJog_1a/`–`MediaJog_1d/`, `U135_Probe/`, `archive/`, `LICENSE`, `LICENSES/`, hardware and image files. Every file outside `MediaJog_1e_1/` and `docs/` was compared by SHA-256 before and after this work: unchanged.
- No device was flashed, no Git repository was initialized, nothing was published.

## 8. Unverified assumptions

- `Fn+3`/`Fn+4` are delivered correctly by the ADV keyboard hardware (source-checked only, §4.1).
- Right Arrow / Left Arrow / Space perform frame stepping and play/stop in Premiere Pro and Final Cut Pro. This depends on the application, its version and the focused panel; **not tested**.
- Arrow keys and Space behave as ordinary keys in other applications (in a browser, for example, they may scroll the page). This is expected host behavior, not a firmware issue.

## 9. Known limitations of Stage 1e-1

- No persistence: every power-on starts in Profile 1 with sensitivity 8.
- Custom has no configuration; it sends nothing by design.
- No HOLD behavior; all keyboard output is tap-only.
- Profile 3 shows its mapping instead of tap counters (the 1d counters are shared and count since boot).
- The USB replug / `USB SUSPENDED` issue and the intermittent first recognition are unchanged.
- Windows/Linux untested (as for Stage 1d).

## 10. Hardware test results (2026-10-13, reported by the author)

**Setup:** Stage 1e-1 installed through the Launcher (Wi-Fi upload of the app-only binary), tested on a Mac. The SHA-256 of the uploaded file was **not recorded**; the values in §1 identify the repository file only.

### 10-1. Results by checklist item (§6)

| # | Test | Result |
|---|---|---|
| 1 | Install through the Launcher; Launcher still present afterwards | Installed through the Launcher. "Still present afterwards" **not recorded** |
| 2 | Boot state | **PASS:** boots into Profile 1 |
| 3 | Profile 1 regression (YouTube `.` `,` `k`) | **Not recorded** in this session |
| 4 | Profile 2 regression | **PASS:** Spinner operation. Individual sub-checks (header text, `Fn+;`/`Fn+.` range, drag) not separately reported, except the drag release in item 9 |
| 5 | `Fn+3` | **PASS:** selects Profile 3 |
| 6 | Profile 3 output | **PASS:** sends Right Arrow, Left Arrow and Space |
| 7 | `Fn+4` | **PASS:** selects Profile 4 |
| 8 | Profile 4 silence | **PASS:** no HID output |
| 9 | Leaving Spinner while dragging | **PASS** for Spinner → Profile 3 and Spinner → Profile 4 (active drag released). Spinner → Profile 1 **not recorded** for this build (it passed in Stage 1c-1 with the 1c firmware) |
| 10 | Switching profiles during fast rotation | **INCONCLUSIVE.** The author could not clearly tell whether additional key events remained after switching. Neither PASS nor FAIL |
| 11 | Cardputer keys stay local | **PASS:** Cardputer keyboard input did not leak to the Mac |
| 12 | Re-selecting the current profile changes nothing | **Not recorded** |
| 13 | Premiere Pro / Final Cut Pro (optional) | **Not tested.** No application compatibility claimed |
| 14 | Power cycle | **PASS:** power-cycling from Profile 4 returned to Profile 1, as expected without persistence |

**Summary:** 10 checks passed (items 2, 4, 5, 6, 7, 8, 9 twice, 11, 14); item 10 is inconclusive; items 1 (Launcher afterwards), 3, 9 (Spinner → Profile 1), 12 and 13 were not recorded or not tested.

### 10-2. Notes on the inconclusive test (item 10)

- **What the source says:** `setProfile()` clears the keyboard queue, and the switch happens before any send in the same `loop()` pass (§3). So queued, unsent taps should be dropped at the switch.
- **What it does not cover:** a tap that is already being sent when the switch happens completes normally (press, then all-keys-up).
- **This is reasoning, not a test result.** A clearer test needs a way to observe events on the host, for example a key-event viewer or a text editor with Profile 1 (`.` and `,` are visible characters), and the screen's `Q` value (queue length) just before switching.
- The test remains open.
