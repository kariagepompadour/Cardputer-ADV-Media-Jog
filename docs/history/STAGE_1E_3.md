# Stage 1e-3 — HOLD for the Custom knob press (Profile 4)

Created: 2026-10-13 · Implementation: Claude Opus 5.5
**Status: built successfully; NOT installed on hardware; awaiting GPT (Edgar) independent review before hardware testing.**

> **Status update (2026-10-13, appended; the line above is the status at build time):**
> - **Hardware test (author, Cardputer ADV + U135 on a Mac): the prioritized tests requested by Edgar passed** — TAP/HOLD selection in CUSTOM SETUP, HOLD Shift + CW Right selecting text, normal cursor movement after knob release, and release of Shift on a profile switch with no stuck modifier (§10).
> - This is **not** the full H1–H19 checklist (§7). The remaining items are not tested (§10-2).

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

Stage 1e-3 lets the Profile 4 (Custom) knob press (BTN) be **TAP** or **HOLD**. HOLD presses the assigned key and/or modifiers when the knob is pressed and releases them when the knob is released. CW and CCW stay **TAP only**. The setting is made on the Cardputer only (CUSTOM SETUP). **RAM only**: it is lost at power-off, like the 1e-2 mappings (NVS is Stage 1e-4).

Base: `MediaJog_1e_2_r3/` (unchanged). The r3 overview design (adaptive key-name sizing, confirmed by the author as appropriately sized) is preserved.

## 1. Deliverables and hashes

| File | Size | SHA-256 |
|---|---|---|
| `MediaJog_1e_3/MediaJog_1e_3.ino` | 71,848 B | `23054acf8b6721210d6ab65d96d59ab326de90819bf33f01cd5fbf3139241486` |
| `MediaJog_1e_3/build/MediaJog_1e_3.ino.bin` (app-only, for Launcher Wi-Fi upload) | 604,976 B | `0d982651cbe1d5994e06619e654380ce97e3d0586d50714d69ca8ed220927506` |

- Both are recorded in `MediaJog_1e_3/SHA256SUMS.txt` (paths relative to the repository root); `shasum -a 256 -c` → OK.
- Starting point: `MediaJog_1e_2_r3/MediaJog_1e_2_r3.ino` (SHA-256 `37f0698a…60fc06c`), copied and then edited. r3 and all earlier milestones are untouched (§9).
- Build tag on every screen: **`1e-3`** (the binary contains `1e-3` and no older tag).

## 2. HOLD architecture

### 2.1 Keyboard base state (the key-state model deferred from 1e-1)

Before 1e-3, every tap sent `[modifiers, key]` and then an **all-keys-up** report. With a key held, that second report would silently release it. 1e-3 adds a **base state**:

- `kbHoldMods`, `kbHoldUsage`: what the Custom HOLD currently holds. Both are 0 when nothing is held, which is **always** the case in Profiles 1–3.
- `sendKeyReport(mods, usage)` now builds `modifier = kbHoldMods | mods`, `keycode[0] = kbHoldUsage` (if any), then the tap key in the next slot.
- `sendRelease()` (code unchanged) now means "send the base state": all keys up when nothing is held, otherwise the held key/modifiers.
- With nothing held the bytes are **exactly the 1e-2 reports** (`modifier = mods, keycode[0] = usage`, then all zero). ✅ host test 1.

So a tap during HOLD is sent as **held + tap**, and the report after it returns to **held**, not to all-up.

| BTN (HOLD) | CW tap | Reports sent | Host sees |
|---|---|---|---|
| Shift (modifier only) | Right | `[Sft]` → `[Sft, Right]` → `[Sft]` → … knob up → `[]` | Shift+Right ✅ host test 2 |
| Command (modifier only) | Right | `[Cmd]` → `[Cmd, Right]` → `[Cmd]` → … → `[]` | Command+Right ✅ host test 3 |
| Cmd+A | Right | `[Cmd, A]` → `[Cmd, A, Right]` → `[Cmd, A]` → … → `[]` | A stays held during the tap ✅ host test 4 |
| Shift | Ctrl+Right | `[Sft]` → `[Ctl+Sft, Right]` → `[Sft]` → … → `[]` | Ctrl+Shift+Right ✅ host test 5 |

### 2.2 Modifier-only actions (addressed explicitly)

In 1e-2, `usage == 0` meant "not assigned" even if modifiers were set, so "HOLD Shift" would have been silently ignored. In 1e-3:

- An action is **assigned if it has a key or at least one modifier** (`customAssigned()`), used by `pushCustom()` and `customAnyAssigned()`.
- `formatKey()` shows a modifier-only action as e.g. `Sft` or `Ctl+Cmd`; `(none)` only when both are empty.
- The EDIT screen shows **`(no key)`** when modifiers are set without a key.
- Consequence: a **modifier-only TAP** on CW/CCW/BTN is now possible. It sends `[mods]` then the base state, a brief modifier press. It is harmless but usually does nothing on the host (§8).

### 2.3 Ordering: HOLD down/up go through the tap queue

- `EV_HOLD_DOWN` and `EV_HOLD_UP` are queued in the same ordered 32-entry queue as CW/CCW/BTN taps.
- `sendQueued()` changes the base state only when it reaches a HOLD event, then sends the base report and retries it (via `releasePending`) **before any later tap**.
- Therefore taps queued before the press are sent without the held modifiers, taps during the hold with them, and taps after the release without them. ✅ host test 7.
- A failed base report is retried before the next tap. ✅ host test 12.

### 2.4 Release rules

| Event | What happens | Where |
|---|---|---|
| Knob release (normal) | `EV_HOLD_UP` queued after the pending taps; base → all up | `onButton()` → `pushHoldUp()` |
| Queue full at knob release | The queue is **cleared** (taps dropped and counted as overflow), then `EV_HOLD_UP` queued. **A HOLD up is never dropped** | `pushHoldUp()` ✅ host test 8 |
| Queue full at knob press | The press is dropped (counted); nothing is held and the following release does nothing | `pushHoldDown()` |
| Profile change (`Fn+1..4`) | Immediate safety release | `setProfile()` |
| CUSTOM SETUP entry (`Fn+S`) | Immediate safety release | `enterSetup()` |
| U135 lost (no ACK) | Immediate safety release | `pollUnit()` |
| USB not ready | Immediate safety release | `updateUsbState()` |
| Knob press while USB not ready | Not held (counted as skipped) | `onButton()` |

**Safety release** (`releaseHoldNow()`): runs immediately, outside the queue. It clears the held state and the queue (including a HOLD down not yet processed) and requests the all-up report, which is retried until confirmed. ✅ host tests 9, 11, 13.

- **No auto-resume.** After any safety release, `holdArmed` is false. The later knob release does nothing, and a HOLD starts only on a **new physical press** (the knob logic acts on edges). Reconnecting USB or the U135 never re-presses anything.
- **No change when nothing is held.** `releaseHoldNow()` returns immediately if no HOLD is armed or held. In Profiles 1–3, and in Profile 4 with BTN = TAP, the callers behave exactly as in r3 (no extra queue clear, no extra report). ✅ host test 10.

## 3. UI (CUSTOM SETUP)

| Screen | Change |
|---|---|
| EDIT BTN | `Fn+H` toggles **TAP ↔ HOLD**. A badge `TAP` (white) or `HOLD` (orange) is shown in the header (x 124, clear of the build tag). The bottom hint reads `Fn+H: TAP/HOLD   esc (`): undo` |
| EDIT CW / CCW | **Unchanged** workflow and hints (`Fn+H` is ignored there; CW/CCW are TAP only) |
| EDIT key area | `(no key)` when modifiers are set without a key; `(none)` when empty |
| `del` | Clears the action **and resets it to TAP** |
| `esc` (undo this action), Cancel, Save | Act on the whole action including TAP/HOLD (struct copies; `memcmp` detects a TAP/HOLD-only change as "Unsaved changes") |
| Overview | The three rows keep the **r3 adaptive key-name sizing unchanged**. A new line at y 102 shows `BTN: TAP` (white) or `BTN: HOLD` (orange); "Unsaved changes / No changes" moved right to x 100 on the same line |
| Profile 4 normal screen | BTN line reads `BTN: HOLD <key>` when HOLD is set (max. 35 characters, 210 px, fits). The last-event line shows `HOLD <key>` / `HOLD UP` |

Key names stay **uppercase** (`A`, not `a`): they are HID key names, not typed characters. To send an uppercase letter, add the Shift modifier (README note).

## 4. Source diff summary (vs `MediaJog_1e_2_r3.ino`)

192 insertions, 18 deletions. Per-function comparison (functions extracted and compared byte for byte):

- **New:** `isHoldEvent`, `queuePushRaw`, `pushHoldDown`, `pushHoldUp`, `releaseHoldNow`, `customAssigned`.
- **Changed, HOLD core:** `sendKeyReport` (base state), `sendQueued` (HOLD events), `onButton` (Profile 4 HOLD branch only).
- **Changed, one added `releaseHoldNow()` line each:** `setProfile`, `enterSetup`, `updateUsbState`, `pollUnit`.
- **Changed, modifier-only/UI:** `customAnyAssigned`, `pushCustom`, `formatKey`, `handleSetupKey` (`del` resets flags; `Fn+H`), `drawCustomStatusBody`, `drawSetupScreen`.
- **Comment only:** `actionFor`.
- **Unchanged (36 functions):** including `tapKey`, `sendRelease`, `pushEvent`, `queueClear`, all mouse functions (`setMouseButton`, mouse sending), the U135 reading, sensitivity, profile key tables, `setup()`, `loop()`.
- Data: `CustomKey` gains `flags` (`CK_HOLD`); `Event` gains `EV_HOLD_DOWN`, `EV_HOLD_UP`; new globals `kbHoldMods`, `kbHoldUsage`, `holdArmed`, `holdDowns`, `holdSafetyReleases`. `BUILD_TAG` = `1e-3`.

## 5. Build results (2026-10-13)

| Item | Result |
|---|---|
| Environment | arduino-cli 1.5.1, FQBN `m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc` (USB-OTG TinyUSB, CDC On Boot), same libraries as 1e-2 (M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2) |
| Result | ✅ exit 0. Flash 604,827 B (46%), RAM 60,196 B (18%) |
| Warnings (`--warnings all`) | **0 sketch warnings.** Known library warning only: `KEY_TAB` redefined. A `-Wformat-truncation` warning on the `HOLD …` display text was fixed during implementation (`%.18s`) |
| `#error` guards | ✅ still effective: hwcdc build and CDC-off build both stop with the intended error |
| USB settings | Unchanged from the established USB-OTG/TinyUSB configuration; HID layout unchanged (`hidOut`, then Mouse Report ID 2, Keyboard Report ID 1) |

### 5.1 Host test of the report logic ✅

The real `sendKeyReport`, `sendRelease`, `tapKey`, queue and HOLD functions were extracted from the source and run on the Mac against a mock HID. 16/16 checks passed: no-hold bytes identical to 1e-2; Shift+Right; Command+Right; held key kept during a tap; tap modifiers combined; tap of the held key; queue order; full-queue HOLD up; safety release while held, before the HOLD down is processed, and with nothing held; retry of a failed base report; nothing sent while the endpoint is not ready. This tests logic only, **not** hardware or the host OS.

## 6. Safety review

1. **Combined reports.** Every keyboard report is base + tap; the report after a tap returns to the base. A tap therefore never releases the held key or modifiers. If the press report times out (`TAP_UNKNOWN`), the following base report still restores the held state; if that fails, it is retried before the next tap. ✅
2. **No stuck key or modifier.** The base becomes all-up on knob release (queued, never dropped) and immediately on profile change, setup entry, U135 loss and USB not ready. The all-up report is retried every loop until confirmed (`releasePending`), as in 1a–1e-2. ✅ source/host test, ❓ hardware.
3. **No stale HOLD.** Each safety release clears the queue, including an unprocessed HOLD down, and disarms the hold. The pending release of that press is then ignored. ✅
4. **No auto-resume.** A HOLD begins only on a knob press edge in Profile 4 normal mode with USB ready. After USB/U135 recovery, a profile switch or setup exit, nothing is pressed until a new physical press. While CUSTOM SETUP is open, the knob drives only the UI. ✅
5. **Profiles 1–3 and Profile 4 TAP.** The base is always empty there, so the reports are byte-identical to 1e-2, and `releaseHoldNow()` is a no-op. Mouse functions (Profile 2) are unchanged. ✅ source/host test; ❓ hardware regression required (§7).
6. **No leaks during setup.** `enterSetup()` releases a HOLD first, then behaves as in 1e-2 (queue cleared, all-up). `Fn+H` is consumed by the setup UI like `Fn+C/S/A/G`. ✅ source.
7. **Saved vs draft.** HOLD reads `customSaved` (changed only by Save, which can only happen inside setup, after the release). Editing the draft cannot affect a held key. ✅
8. **Residual timing case.** HOLD down is processed at most one loop (~20 ms) after the press; a detent read in the same loop but **before** the press edge is already queued ahead of it and is sent without the modifier. A detent within that window **after** the press is sent with it. This follows the physical order as seen by the polling loop.

## 7. Hardware test checklist (proposed; not run)

Install `MediaJog_1e_3.ino.bin` via the Launcher Wi-Fi upload (app-only file; not merged.bin) and **record the SHA-256 of the uploaded file** (`0d982651…0927506`). Use TextEdit (or a keyboard viewer) on the Mac.

| # | Do | Pass condition |
|---|---|---|
| H1 | Start the app; look at every screen (Profiles 1–4, CUSTOM SETUP, EDIT) | Tag `1e-3` visible on each |
| H2 | Profile 1 regression | `.` `,` `k` as before |
| H3 | Profile 2 regression | Pointer movement, drag (knob held + turn), release on `Fn+1`, sensitivity `Fn+;`/`Fn+.` |
| H4 | Profile 3 regression | Video Editing keys as in 1e-1 |
| H5 | Profile 4, BTN = TAP (e.g. `Space`) | One tap per press, as in r2; CW/CCW taps unchanged |
| H6 | `Fn+S`, EDIT BTN, `Fn+H` twice | Badge toggles TAP ↔ HOLD; overview shows `BTN: TAP` / `BTN: HOLD`; "Unsaved changes" appears after a toggle; nothing reaches the Mac |
| H7 | EDIT CW: press `Fn+H` | Nothing changes; CW/CCW editing works exactly as in r2 |
| H8 | BTN = HOLD Shift (`Fn+S` toggle only → `(no key)`), CW = Right, Save. In text, hold the knob and turn CW | Text is selected to the right (**Shift+Right**); on knob release, turning CW moves the cursor without selecting |
| H9 | BTN = HOLD Command (`Fn+G` only), CW = Right; hold knob, turn CW | Cursor jumps to line end (**Command+Right**) |
| H10 | BTN = HOLD `A` (letter) | Holding the knob auto-repeats `a` (host auto-repeat, expected); stops on release |
| H11 | While holding the knob (HOLD Shift): `Fn+1`, then release | Shift released at `Fn+1` (no stuck Shift; type a letter → lowercase); nothing pressed on knob release; back in P4 nothing resumes |
| H12 | While holding (HOLD Shift): `Fn+S` | Setup opens; Shift released (keyboard viewer); no typing |
| H13 | While holding (HOLD Shift): unplug USB, plug back, keep the knob held | No stuck modifier on reconnect; nothing pressed until the knob is released and pressed again. Note the known USB replug issue |
| H14 | (Only if approved, §2.2 of the plan) while holding: unplug the U135 | Released; nothing resumes on reconnect without a new press |
| H15 | Cancel after a TAP→HOLD toggle; `esc` undo in EDIT BTN; `del` in EDIT BTN | Cancel keeps the saved mode; undo restores the mode; `del` clears the action and shows TAP |
| H16 | CW = Shift only (modifier-only TAP) | Harmless; no stuck Shift |
| H17 | Fast CW rotation while holding (HOLD Shift) | All taps include Shift; Shift released at knob release |
| H18 | Power cycle | Starts in Profile 1; Custom mappings and TAP/HOLD cleared (expected before 1e-4) |
| H19 | Overview with long names (r3 test R4/R5) | r3 sizing unchanged; `BTN: HOLD` line and change status do not overlap |

## 8. Known limitations

- **Tap of the held key:** if CW/CCW sends the same key that BTN holds, the tap has no effect (a held key cannot be pressed again in a report).
- **Host auto-repeat** acts on a held non-modifier key (expected; not a firmware repeat).
- **Modifier-only TAP** (e.g. CW = Shift) is allowed and harmless but usually does nothing on the host.
- `BTN: HOLD` can be set on an unassigned BTN; the overview then shows `BTN: HOLD` while the row shows `(none)`, and the press sends nothing.
- **RAM only:** TAP/HOLD and mappings are lost at power-off until 1e-4.
- The known **USB replug issue is unchanged**.
- macOS Japanese input (IME) may compose letter keys (host behavior; the firmware does not change the input method).

## 9. Unchanged

`MediaJog_1a` … `MediaJog_1e_2_r3`, `U135_Probe`, `archive`, SOUND TERRARIUM and the Launcher are untouched. No NVS or partition change. No Git, no flashing, no publishing.

## 10. Hardware test (2026-10-13, reported by the author)

Setup: Cardputer ADV + Unit Encoder U135, connected to a Mac. The test covered **the prioritized tests requested by Edgar**, not the full §7 checklist. SHA-256 of the uploaded binary: **not recorded** in the report.

### 10-1. Hardware-verified behavior (observed on the device and the Mac)

| # | Observation (author) | Result |
|---|---|---|
| 1 | The TAP/HOLD selector worked correctly in CUSTOM SETUP | PASS |
| 2 | BTN = HOLD Shift, CW = Right: holding the knob while turning clockwise selected text character by character | PASS — **combined HOLD + TAP report (Shift+Right) confirmed on hardware** |
| 3 | Releasing the knob restored normal cursor movement without selection | PASS — release on knob release |
| 4 | Switching profiles while HOLD Shift was active released Shift; no stuck modifier observed | PASS — release on profile switch |

The author reported that all requested tests worked well.

### 10-2. Status of the §7 checklist after this test

| Item | Status |
|---|---|
| H6 TAP/HOLD toggle | **PASS** (observation 1; the sub-points such as the overview `BTN:` line and "Unsaved changes" were not itemized) |
| H8 HOLD Shift + CW Right | **PASS** (observations 2, 3) |
| H11 release on profile switch | **Partial:** Shift released, no stuck modifier (observation 4). The profile key used, the "nothing pressed on knob release" and "nothing resumes back in Profile 4" parts were not reported |
| H1–H5, H7, H9, H10, H12–H19 | **Not tested** (e.g. Command+Right, release on `Fn+S`, USB unplug, U135 loss, no auto-resume after reconnect, Profiles 1–3 regression, Cancel/undo/`del`, fast rotation) |

### 10-3. Evidence classes

- **Hardware-confirmed:** observations 1–4 above only.
- **Source review and simulated host test only (§5.1, §6):** byte-identical reports with nothing held, Command+Right report bytes, queue ordering, full-queue release, safety release on setup entry / USB not ready / U135 loss, no auto-resume, failed-report retry.
