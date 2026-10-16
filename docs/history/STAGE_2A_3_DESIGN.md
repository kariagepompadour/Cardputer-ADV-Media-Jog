# Stage 2a-3 — Design: KEY STEP and Fn+5 C-SPINNER

Created: 2026-10-16 · Design: Claude Opus 5.5 · Decision basis: Edgar and the author (revised specification, approved with the answers below)

- Base: Stage 2a-2 (`MediaJog_2a_2/`, hardware-verified; unchanged).
- New folder `MediaJog_2a_3/`, build id `2a-3`.
- Developed in three steps. Each step is approved, implemented, host-tested, built and hardware-checked before the next one.

## 1. Purpose

Make the controller adjustable per application:
- **KEY STEP:** the number of key inputs per knob detent, for the key profiles.
- **SPINNER SENS:** the mouse movement per detent, for the mouse profiles.

The trigger was Arkanoid in a web MAME. The goal is general, not a game-specific feature.

## 2. Profiles after all three steps

| Keys | Profile | Output | Adjustment |
| --- | --- | --- | --- |
| Fn+1 | YOUTUBE | keyboard | KEY STEP |
| Fn+2 | SPINNER | mouse | SPINNER SENS (the existing 14 steps, unchanged) |
| Fn+3 | EDITING | keyboard | KEY STEP |
| Fn+4 | CUSTOM (KEY) | keyboard | KEY STEP |
| Fn+5 | C-SPINNER (steps 2–3) | mouse + custom button | its own SPINNER SENS |

- Fn+1 to Fn+4 keep their key assignments, button actions and operation.
- `Fn+;` / `Fn+.` change the KEY STEP or the SENS of the current profile.

## 3. KEY STEP (step 1)

- **Range:** 1–10, step 1, default 1. One value per profile (1, 3, 4). Stored in NVS.
- **Meaning:** the number of key **taps** one detent sends.
  - Each tap is the existing tap: a press report, then a release report. **The press duration is not changed.** No artificial delay is added.
  - Each tap reaches the host as a separate press / release (USB: 1 ms polling, each report confirmed; BLE: one notification each), so event-driven applications count every tap.
- **Rotation only.** The knob press (`k`, Space, CUSTOM BTN) is never multiplied.
- **Queue:**
  - One queue entry per detent carries its tap count (`queueRep[]`), fixed when the detent is queued.
  - Entries are sent in order (**FIFO**, also when the direction reverses — Edgar).
  - KEY STEP 1 uses exactly the 2a-2 path: at most 4 taps per loop.
  - KEY STEP ≥ 2: up to KEY STEP taps per loop (at most 10, provisionally approved).
- **Backlog limit:** a detent that would make more than `KEY_PENDING_MAX` taps wait is dropped **as a whole** and counted. `KEY_PENDING_MAX` = 100 is **provisional**; the final value follows USB / BLE measurements (Edgar).
- **Count guarantee:**
  - Every detent that is processed normally sends exactly KEY STEP taps. A press that the host did not accept is retried.
  - **Not guaranteed in exceptions:** a link loss, a profile switch, CONNECTION / SETUP entry, a dropped detent (backlog limit, queue full), or a press whose outcome is unknown (not resent, as before) can lose taps (Edgar).
- **Limit:** above about 25 detents per second at KEY STEP 10 (USB estimate), detents are dropped rather than delayed further. The BLE throughput is still to be measured.
- **Apps that sample the key state per frame:**
  - Some apps (possibly the web MAME) read the key state once per video frame. They may merge or miss taps that are shorter than a frame.
  - KEY STEP does not change the press duration (specification), so it cannot guarantee proportional movement there. Whether it helps is measured on hardware.

## 4. C-SPINNER (steps 2 and 3)

- **Rotation:** the SPINNER path (half-count carry, pending limit), with its own SENS (14 steps, default as PROFILE 2) and a direction setting (normal / reversed).
- **Button:** a key (with modifiers), mouse left, mouse right, or none; TAP or HOLD.
  - Key TAP and HOLD work as CUSTOM's BTN.
  - Mouse TAP = click; mouse HOLD = held while the knob is held.
  - **Default: mouse left, HOLD** (Edgar).
- **Display name:** `C-SPINNER` (Edgar). The full name does not fit in the size-2 line.
- **SETUP (`Fn+S` in profile 5):**
  - a BTN row with CUSTOM's editing keys, plus `Fn+L` (mouse left) and `Fn+R` (mouse right);
  - a DIR row (`ok` or the knob press toggles the direction).
- **Releases:** the existing release points already release both a held key and the mouse button.
- **Keyboard and mouse together:** both are already present on USB (composite device) and on BLE (one HID service), and are independent reports.

## 5. NVS

- `mediajog/cfg` (version 1, 21 bytes) is **unchanged**. CUSTOM KEY and the PROFILE 2 sensitivity stay there.
- **New `mediajog/ext`:** magic `MJEX`, version 1, 21 bytes, CRC-32. It holds:
  - the active profile (1–5) and the `cfg` profile at write time;
  - the KEY STEPs (P1, P3, P4);
  - the C-SPINNER SENS, direction and button (type, modifiers, usage, flags).
- **Defined in full in step 1,** so steps 2 and 3 need no format change.
- **Rollback:**
  - `cfg` only ever holds profiles 1–4, so older firmware keeps reading it and ignores `ext`.
  - In step 2, Fn+5 is restored at boot only if `ext` is valid and its stored `cfg` profile equals the current `cfg` (otherwise older firmware changed the profile since).
- **Writes:**
  - KEY STEP and C-SPINNER SENS: 2 s after the last change, only when different from the stored record.
  - Profile switch: at once, and checked.
  - SETUP: on Save.
- **Damaged or newer records:**
  - A damaged `ext` means defaults until the next change replaces it.
  - A newer-version `ext` is never overwritten.

## 6. Web MAME (Arkanoid)

- **(A) KEY STEP with Fn+3:** measured on hardware (KEY STEP 1 / 5 / 10). Not guaranteed (§3).
- **(B) Mouse (Fn+2 / Fn+5):** Arkanoid's control in MAME is a dial, normally on mouse X when mouse input is enabled. Whether the web MAME accepts mouse input is an emulator setting (mouse option, pointer lock). The firmware cannot change it.
- **Button assignments for the game:** set in Fn+5 SETUP after the actual input is confirmed. Nothing game-specific is built in.

## 7. Design review: direction reversal during fast rotation (2026-10-16; no code change)

**Hardware observation (author):** BLE, YOUTUBE, KEY STEP 10. After fast right rotation, an abrupt left turn: frame stepping to the right continued for a short, clearly noticeable time.

**New principle (Edgar / author):** on a reversal, the current rotation has priority over past rotation.

### 7.1 Causes (source facts and estimates)

**The queue (fact):**
- One entry per detent carries an event (`EV_CW` / `EV_CCW` / `EV_PRESS` / `EV_HOLD_DOWN` / `EV_HOLD_UP`) and a tap count (`queueRep[]`).
- Entries are sent strictly FIFO. An entry stays at the head until all its taps were sent.
- On a reversal:
  1. every queued old-direction detent is still sent first: up to 100 taps ≈ 1.4 s at about 70 taps/s;
  2. while the queue is at its limit, **the new left detents are dropped** (`pushDetent()` checks the limit before queuing). This is exactly the "new direction discarded because the queue is full" case.

**The BLE stack (estimate):**
- Notifications already accepted (host mbufs: 12 + 24 msys blocks, roughly 18 taps ≈ 0.25 s; plus the controller's own transmit buffers, number unknown) **cannot be recalled** by the application.
- No API removes queued notifications short of ending the connection.

**Mac / application:** the time from the HID report to the visible frame is outside the firmware and cannot be measured from it.

**Answers to the questions:**

| # | Question | Answer |
| --- | --- | --- |
| A1 | Direction in the queue | Each rotation entry's event is `EV_CW` or `EV_CCW` |
| A2 | Unit of KEY STEP 10 | One entry per detent with `queueRep` = 10 |
| A3 | Old direction removable alone | Yes: rotation entries are recognisable by event, and other events can be kept in their order |
| A4 | Partly sent entry | Its remaining taps can be cancelled after a complete tap. Each tap is press + release inside `tapKey()`, and a release that the host refused is retried first in every loop (`releasePending`, `sendQueued()` line 1). Cancelling the rest leaves no key held |
| A5 | Order with other events | `EV_PRESS` and the HOLD events must stay, in their order. HOLD UP must never be removed (it ends a held modifier). Removing only rotation entries keeps every other event's relative order |
| B1 | What the stack may still hold | Up to about 18 taps in host mbufs (estimate) plus controller buffers |
| B2 | Cancel it | Not possible |
| B3 | Split the causes | Yes: the firmware part is the queued old-direction taps (d1 shows `q`). The stack part is bounded by the pool and can be measured as free blocks (`M`) |
| B4 | Less stack backlog helps | Yes, for the part after the firmware queue: at most about 0.25 s now, and a few tens of ms with pacing (estimate) |

### 7.2 Methods compared

| Method | Reversal response | Normal operation | Safety | Compatibility | Drops |
| --- | --- | --- | --- | --- | --- |
| A: on a reversal, remove the queued old-direction rotation (incl. the rest of a partly sent detent) | Firmware delay goes from up to 1.4 s to 0. The stack backlog (≤ about 0.25 s) remains | Unchanged: without a backlog there is nothing to remove | Keeps PRESS / HOLD and their order. No key left held | Queue layer only | The old-direction backlog (by design) |
| B: stack-aware pacing (BLE only) | Stack backlog from about 0.25 s to a few tens of ms. Does not remove the firmware backlog | The rate stays if the reserve keeps the radio fed | None | BLE only | None |
| **C: A + B** | **Both parts reduced. Remaining: about one or two connection events plus the controller buffers and the host** | Unchanged | As A | A in the queue layer, B BLE only | As A |
| D1: netting (each new detent cancels one old one) | Still plays the rest of the old direction first | — | — | — | Fewer |
| D2: short time-based look-ahead for all rotation | Bounds reversal and stop delay | **Drops during continuous fast rotation** | — | — | More |

D1 does not meet the goal. D2 also drops in the non-reversal case, so it is only a fallback.

### 7.3 Recommendation: C

**(1) Reversal purge** (queue layer, only when the current profile's KEY STEP ≥ 2):
- **Detecting a reversal:** in `pushDetent()`, before the limit check, if the newest queued **rotation** entry has the opposite direction:
  - remove every queued old-direction rotation entry, including a partly sent head entry (whose sent taps stay sent);
  - compact the ring buffer, keeping `EV_PRESS` and HOLD events in order;
  - count the removed taps (new diagnostic `R`).

  The new detent is then checked against the now emptier limit, so it is not dropped.
- **KEY STEP 1:** the `pushEvent()` / `pushCustom()` path is unchanged, so there is no purge.
- **No backlog:** nothing is queued, so nothing happens. Normal-speed operation is unchanged.
- **CUSTOM:** CW and CCW are user keys that are not necessarily opposites. The purge only removes rotation the user has turned away from; HOLD is not touched. Whether CUSTOM is included is a decision point.

**(2) Stack-aware pacing** (BLE only, KEY STEP ≥ 2):
- `sendQueued()` stops a loop's taps when the estimated in-flight notifications reach about 6, that is about 3 taps.
- The estimate is `os_msys_num_free()` at idle minus the current value, calibrated at link-up.
- The first value is to be set from d1's `M`. It is chosen so that the radio, about 2 notifications per 15 ms event, is never starved between two loops.

**USB:**
- The purge is a rule of the jog semantics in the shared queue. It acts only when a backlog exists, which on USB is rare and small.
- Making it BLE-only is possible, if Edgar prefers USB to stay byte-identical (decision point).
- Pacing is BLE-only.

### 7.4 Effects and risks

- **Unchanged:** KEY STEP 1, USB at KEY STEP 1, button handling, HOLD, profile switch, NVS, GATT, pairing.
- **New loss case (documented):** queued old-direction taps after a reversal. They are a new exception to the KEY STEP count guarantee, next to link loss, profile switch and drops.
- **Risk:** the ring-buffer compaction (ordering, head entry, `queueCount`). It is covered by host tests with mixed events.
- **Pacing risk:** a reserve set too small lowers the rate. Mitigated by calibration and the d1 rate as the reference.

### 7.5 Code change sites (estimate)

- `pushDetent()`: reversal check and call.
- New `purgeRotation(Event oldDir)`: compacting the queue.
- `sendQueued()`: a BLE-only pacing check before each tap, only with KEY STEP ≥ 2.
- Link-up: calibrate the idle mbuf count.
- Diagnostics: removed taps `R`, in-flight maximum.
- New folder (proposed `MediaJog_2a_3_r2`). 2a-3 and d1 stay unchanged.

### 7.6 Minimal hardware tests after implementation

1. BLE, YOUTUBE, KEY STEP 10: fast right then abrupt left, 5 times. Count the extra right frames (video) and the time until left starts.
2. The same at normal speed, and with KEY STEP 1 (no change expected).
3. USB, KEY STEP 10, the same reversal.
4. Stop after fast rotation: tail as before or shorter. d1's rate is not lower.
5. CUSTOM with HOLD Shift: reverse while holding, and release. No modifier left held.
6. Press `k` during a fast rotation with a reversal: the press is not lost and stays in order.

### 7.7 Limits

- Notifications already handed to the stack and the controller cannot be recalled. A reversal therefore always still shows the last few old-direction taps: a few tens of ms by estimate, after pacing.
- The time from the report to the visible frame (macOS, the browser, YouTube's seek) is outside the firmware. Firmware measurements alone cannot prove the felt delay is gone.

## 8. r2: reversal purge — decision and implementation (2026-10-16)

**Decision (author / Edgar):**
- **Principle:** method C is the goal. **This round implements method A only**; B (BLE pacing) is decided after the hardware evaluation.
- **Guiding principle:** "the machine follows the human". When the user turns the other way, the new direction starts as soon as possible. Replaying every queued old-direction input is not the goal.
- **Scope:**
  - KEY STEP 2–10, PROFILE 1 / 3 / 4 (CUSTOM included, no exception), USB and BLE alike.
  - KEY STEP 1 unchanged.
  - Not in this round: BLE pacing, connection interval, PHY, the 100-tap limit, KEY STEP range, time-based drops, thinning of normal rotation.
- **Pass criterion:** on hardware, "fast right, then left: it starts moving left". Queue numbers alone do not prove it.

**Implementation (`MediaJog_2a_3_r2/`, from 2a-3; build id `2a-3r2`, tag `2a3r`):**

| Item | Detail |
| --- | --- |
| Detecting a reversal | In `pushDetent()` (KEY STEP ≥ 2 path), `newestQueuedRotation()` finds the newest waiting rotation entry. If its direction differs from the new detent's, it is a reversal. The queue itself is the state, so nothing goes stale after a queue clear (profile switch, link loss, CONNECTION / SETUP, HOLD safety release). If no rotation is waiting there is nothing to remove, and nothing happens |
| Removal | `purgeRotation(oldDir)` removes every waiting rotation entry of the old direction, including the unsent rest of a partly sent head entry. It compacts the ring buffer in place, and every other event (`EV_PRESS`, `EV_HOLD_DOWN`, `EV_HOLD_UP`) keeps its order |
| Order | The removal happens **before** the CUSTOM assignment check and the backlog-limit check, so the new detent is not dropped because old-direction taps fill the queue. A reversal towards an unassigned CUSTOM direction also stops the old direction |
| HID safety | A tap is always complete: `tapKey()` sends the press and then the release, and a refused release sets `releasePending`, which `sendQueued()` retries first in every loop. The removal runs between taps (from `onRotation()`), never inside one, so no key can be left down. `sendQueued()`, `tapKey()`, `sendRelease()` and the HOLD functions are unchanged |
| Diagnostics | `V` (reversals that removed waiting rotation), `X` (taps removed), `q` (taps waiting now), next to `P` / `D`. Status screen: `BTN… P D q` and `FAIL… V X` lines; PROFILE 4: on its `W…` line. A reversal with nothing waiting is not counted, because nothing was removed |
| Unchanged | KEY STEP 1 path, `sendQueued`, `pushEvent`, `tapKey`, `sendRelease`, HOLD functions, `onButton`, `setProfile`, NVS (`cfg`, `ext`), `notifyReport`, BLE / B7, GATT (identity with BLE-B1: PASS), USB HID |

**New exception to the KEY STEP count guarantee:** waiting old-direction taps removed at a reversal (`X`). They join the existing exceptions: link loss, profile switch, CONNECTION / SETUP entry, dropped detents.

**Host tests ✅ (logic only):**

| Suite | Result |
| --- | --- |
| Stage 2a suite / 2a-2 scenarios on r2 | 85/85, 32/32 |
| KEY STEP K01–K14 on r2 | 34/35. **K05 fails as intended:** it asserted FIFO on reversal at KEY STEP 3 ("6 right then 3 left"), which r2 replaces by specification. The new behavior is covered by A1–A4 |
| KEY STEP 1 differential r2 vs 2a-3 (3 seeds × USB / BLE) | **identical**: 10,531 reports |
| NVS / HOLD | Their functions are byte-identical to 2a-3, where they passed 47/47 and 16/16 |
| **Reversal tests (new, 21 checks)** | **21/21** |

| Test | Checks |
| --- | --- |
| A1 / A2 | USB / BLE, KEY STEP 10: 10 right detents (90 taps still waiting), then 1 left: V1 X90; afterwards no right tap, exactly 10 left taps |
| A3 | Queue at its limit (5 detents dropped), then 3 left: none dropped, 30 left taps, one reversal |
| A4 | BLE refusing everything, exactly 100 taps waiting at the reversal: the left detent is admitted, not dropped; only left taps reach the Mac |
| B1 | Normal speed (the queue drains between detents): V0 X0, nothing dropped, 30 right + 30 left |
| C1 | KEY STEP 1: 8 right then 1 left, in order, nothing removed (as 2a-3) |
| D1 | YOUTUBE: `k` between right detents is kept (exactly one) and comes before the left taps; ends all-up |
| D2 | CUSTOM with HOLD Shift: HOLD down and up both processed, left taps without Shift, no right tap after the first left tap, nothing held, queue empty |
| E1 | BLE, the press of tap 4 refused: 3 right taps sent, the unsent 7 removed, 10 left taps; press / release alternate |
| E2 | BLE, the release of tap 4 refused: the release is sent before any left tap; 4 right taps (the started one complete), 6 removed, 10 left, ends all-up |
| F1 | Status shows `V1 X90` and `q0` |

**Mutation checks:**

| Mutant | Failing checks |
| --- | --- |
| No purge | 12 |
| Purge also removes `EV_PRESS` | 1 |
| Purge also removes HOLD up | 2 |
| Tap counts not moved when compacting | 1 |
| Purge also at KEY STEP 1 | 1 |
| Purge after the limit check | 2 (found by A4; A3 alone did not catch it, so A4 was added) |

**Build:**
- Clean build: exit 0, 0 sketch warnings, guards effective; flash 891,951 B.
- Source SHA-256 `579dd306bf3323acd9a12d421dcc50302d12cc8846be376ecd616a33b79b1eb0`.
- App-only binary (892,096 B) SHA-256 `a4ea24fd3f01243e62e34b1f30c50d101b06445d61d1fde5023a94bb4fe8e6af`.
- The host-tested source equals the final source.
- `MediaJog_2a_3` and `MediaJog_2a_3_d1` are unchanged (checksums verify).

**Hardware evaluation (proposed; not run):**
1. BLE, YOUTUBE, KEY STEP 10: fast right, then abruptly left, five times. Does it start moving left promptly? A video helps to count leftover right frames. Note `V` / `X`.
2. The same over USB.
3. Normal-speed use, both directions: no difference from 2a-3 (`V` stays 0 when the queue drains).
4. KEY STEP 1: unchanged.
5. CUSTOM with HOLD Shift: rotate, reverse while holding, release. No modifier left held.
6. Optional: `k` pressed during fast rotation with a reversal: not lost.

**Known limits and remaining risks:**
- **Stack backlog:** notifications already accepted by the BLE stack (an estimated ≤ 0.25 s of taps) and the controller cannot be recalled. A few right frames can still follow a reversal on BLE. Measuring this residue decides on method B.
- **Outside the firmware:** the time on the Mac and in the application (browser, YouTube seek) cannot be measured by the firmware.
- **Turning back and forth fast:** each reversal removes the other direction's waiting taps, so the total movement follows the latest direction rather than the sum of all detents. This is intended.
- **Possible regression:** a user who relies on CUSTOM CW and CCW being unrelated actions loses waiting taps of the old one at a reversal (decided: no exception).

## 9. r2 on hardware: no BLE input (2026-10-16; investigation only, no code change)

**Observed (author):**
- r2 over BLE: the Mac shows the device as connected, and the Cardputer shows `BLE READY`. The rotation value changes on the screen.
- But nothing reaches the Mac: no YouTube control, no `k` from the knob press, no `.` in TextEdit. Re-pairing did not change it.
- Over USB, r2 works.

**Code comparison, 2a-3 vs r2 (function comparison, source):**
- Changed: only `pushDetent()` (the reversal check, KEY STEP ≥ 2 path), `drawStatus()` and `drawCustomStatusBody()` (counters printed).
- New: `newestQueuedRotation()` and `purgeRotation()`.
- **Byte-identical to 2a-3:**
  - the BLE send path (`sendKeyReport`, `hidSendKeyboard`, `bleSendKeyboard`, `notifyReport`, `tapKey`, `sendRelease`, `sendQueued`);
  - the input path (`pollUnit`, `onRotation`, `onButton`, `pushEvent`);
  - the link logic (`updateLinkState`, `onLinkReady`, `onLinkUp`, `commitSwitch`, `startSwitch`, `serviceSwitch`, `linkStatusText`);
  - all BLE / B7 / GATT code.
- **The knob press does not touch the new code:** `onButton()` → `pushEvent(EV_PRESS)`. The purge runs only for a rotation detent with KEY STEP ≥ 2.
  - The purge only removes queue entries. It sets no flag that could block sending (`hidReady`, `releasePending`, the switch state).
  - So the purge alone cannot explain a lost knob press. Only a state that blocks every send could explain rotation and the press failing together.
- **Host tests:** the BLE scenarios pass on r2 (Stage 2a suite, A2, A4, E1, E2).

**What `BLE READY` means (source):**
- `linkStatusText()` shows it from `linkState`, which `updateLinkState()` sets when connected **and** encrypted **and** the keyboard CCCD is on. That is states 1 and 2 of the question (connection, and a host that has asked for reports).
- Sending is gated separately by `hidReady`, which `onLinkReady()` / `commitSwitch()` set only when BLE is the active transport and no switch is running.
- Whether reports actually leave the device shows in the `.` / `,` / `k` counters: `txCount` counts only taps the BLE host accepted.
- **`BLE READY` therefore does not prove state 3.**

**Judgment:** **not determined yet.** The source shows no path by which the r2 change stops all BLE input including the knob press. The candidates are states that would block every send:
1. **`hidReady` false while `linkState` is READY.** Inputs are then dropped: `SKIP` rises and the yellow line shows `SKIP …`.
2. **Every notification refused.** `releasePending` then stays set (`REL!`), and `sendQueued()` returns at its first line, so taps wait (`Q` / `q` grow, the counters stay).
3. **Reports accepted but not used by the Mac.** The counters rise, but nothing appears on the Mac.

Cases 1 and 2 would be firmware or stack state; case 3 is on the Mac or the link. No code path specific to r2 leads to any of the three.

**Smallest tests, in order:**
1. With r2 in BLE, PROFILE 1, KEY STEP 1 (`Fn+.` until `KEY STEP 1`), TextEdit in front: turn right 3 detents slowly, press the knob once. Then photograph the status screen: the `.` / `k` counters, `SKIP`, `Q` / `q`, `REL!`, the yellow line, and `V` / `X`.
   - KEY STEP 1 never runs the purge, so a failure here excludes method A.
2. `Fn+B` (CONNECTION): photograph the `BLE bond… kb… ms…` line and the `ki / ko / mi` handles.
3. Only if 1 shows the counters rising without input on the Mac: install `MediaJog_2a_3` (known good) and repeat 1 on the same Mac.
   - If 2a-3 also fails, the cause is the Mac / bond / link state, not r2.
   - If 2a-3 works, investigate r2 further.

| Test 1 result | Meaning |
| --- | --- |
| `SKIP` rises, yellow `SKIP …` | `hidReady` false (case 1): firmware state while BLE shows READY |
| `Q` / `q` grow, counters stay, `REL!` | Notifications refused (case 2): BLE stack / link |
| `.` / `k` counters rise, yellow `TX .` | Reports accepted on the device (case 3): Mac side or link; compare with 2a-3 |

**Fix policy:** none before the cause is known. Method A stays as designed: its code is not on the path that the knob press uses.

### 9.1 Test results and re-analysis (2026-10-16; no code change)

**The author's photos:**
- **Status screen** (r2, BLE, PROFILE 1, KEY STEP 1):
  - `BLE READY`;
  - counters `.760 ,811 k9`, yellow line **`TX k`**;
  - `BTN raw=1 Q0/10 OVF0 P100 D53 q0`, `FAIL0 SKIP0 ERR0 V8 X399`, `W112 T5`, `KB … KEY STEP 1`, `MS host=0 want=0 rep5 fail0`.
- **CONNECTION:**
  - `BLE: BLE READY`, `USB HID INACTIVE (mounted)`;
  - `BLE bond=1 kb=1 ms=1 h1 ad0 fs0`, `sw2 warn0 link rec BLE w2 f0`;
  - `ki10 ko21 mi24`.

**Meaning (source):**
- `TX k` and the counters are written only in `sendQueued()`, when `tapKey()` returns `TAP_SENT`. Over BLE, that means `notifyReport()` → `ble_gatts_notify_custom()` returned 0: the BLE host accepted the press report, and the release was attempted at once.
- `SKIP0`, `q0`, no `REL!`: no input was dropped for "not ready", nothing waits, and no release is pending.
- `V8 X399`, `P100 D53`: reversals and drops from earlier KEY STEP 10 use in the same power-on session. All counters are cumulative since boot.
- **Discrepancy with the written summary:** the text says "counters 0, V0 X0, last event PRESS". The photo shows `.760 ,811 k9`, `V8 X399`, `TX k`, and the firmware has no "PRESS" message. Probably the text summarised a different moment.

**Conclusion so far (stronger, still not final):**
- On the device, the inputs travel the whole path. The BLE host **accepted** the keyboard reports, including the knob press `k`, while the Mac had subscribed to the keyboard report (`kb=1`) on the current connection (`h1`, no foreign subscribe `fs0`).
- The loss is **after** the firmware hands a report to the BLE stack: the radio link or the Mac's HID handling. "Accepted" is not proof of delivery.
- This path is byte-identical to 2a-3 and d1, which delivered keys to the same Mac before. The r2 change (`pushDetent()`) is not on it.
- The most likely cause is therefore a state outside the r2 code (Mac Bluetooth / HID state, or the link). It is still unconfirmed.

**Next tests (smallest first):**
1. **Install `MediaJog_2a_3_d1`** (or `MediaJog_2a_3`; same BLE path, previously working). Restart, BLE, TextEdit: turn right 3 detents, press once.
   - d1 also shows `n` (BLE rejects), `M` and `P` / `R`.
   - **If d1 also fails:** the cause is not r2. Then, without unpairing: turn the Mac's Bluetooth off and on; if needed, restart the Mac.
   - **If d1 works:** investigate r2 further, with diagnostics below.
2. Repeat the r2 test right after a Cardputer restart. Photograph the status screen **before and after** the 3 detents + 1 press. The counters must rise by 3 and 1.

**Minimal diagnostics if needed later (proposal, not implemented):**
- In r2, count `BLE_GAP_EVENT_NOTIFY_TX` with its status (a host-side transmit result) in the existing trace listener's counters, and show `notifyFails`, as d1 does.
- Neither changes sending. Neither proves delivery to the Mac; that needs the Mac side.

### 9.2 d1 works, r2 does not, on the same Mac (2026-10-16; no code change)

**Observed (author):**
1. d1 installed: at first `BLE READY` but no input. After the Mac unpaired and paired again, rotation (both directions) and the knob press all work.
2. r2 installed, with a new pairing as well: `BLE READY`, but no input in TextEdit.

**Source and build facts:**
- `setup()`, `bleBegin()`, `bleStart()`, `registerGattTable()`, the access callbacks, `onGapEvent()` and the whole BLE send path are byte-identical in 2a-3, d1 and r2.
- The initialization order is therefore identical.
- r2 adds two `uint32_t` globals and longer tag strings ("2a-3r2", "2a3r"). The tags are only printed (`snprintf` with buffer size, `printf`), so there is no overflow.
- d1 changes more code than r2 (`notifyReport()`, `loop()`) and works.
- The r2 photo showed `TX k` and rising counters: the BLE host accepted the reports.

**Candidates:**

| # | Candidate | For | Against |
| --- | --- | --- | --- |
| A | A Mac / link state independent of the firmware: connected, subscribed, reports accepted, but not used | d1 showed **the same symptom** before its re-pairing; the same kind of state was seen before (§8.4 #1 of STAGE_2A_2). Only one try per build so far | r2 stayed broken after one re-pairing |
| B | An r2-specific runtime effect that the source comparison does not show (memory layout exposing undefined behavior in shared code, a different BLE stack state) | r2 is the only build failing after re-pairing | All BLE / init code is identical; d1 has larger layout changes and works; no undefined behavior known |
| C | Reports accepted by the host but not sent on the link | Would match "accepted, nothing arrives" | r2 does not show `n` / `M`; d1 does |
| D | Not the r2 binary that was meant | — | The screen tag shows `2a3r`; the SHA-256 of the uploaded file is not recorded |

**Smallest tests that separate them (in order):**
1. **No reinstall, r2 in the failing state:**
   - **Fresh link without unpairing:** `Fn+B` → `1` → ok (USB), then `Fn+B` → `2` → ok (BLE). `bleStop()` ends the link, and the Mac reconnects. Then the TextEdit test.
   - Before that, a photo of the trace page (`Fn+B`, `t`): it shows `CONN`, `ENC`, `SUB k / m` with `W` or `R`, and `UPD` for the current link.
   - **If input works after the fresh link:** candidate A.
2. **Mac Bluetooth off → on**, then the test; if still nothing, a **Mac restart**. Still no unpairing.
3. **Repeatability:**
   - install **2a-3** (not d1), with the same procedure as for r2 (one re-pairing), and test;
   - then install r2 again, with the same procedure.
   - Failures that follow the build (2a-3 works, r2 fails twice) point to B. Mixed results point to A.
4. **Only if B is likely:** a diagnostic r2 (with approval) that adds d1's counters `n` / `M` / `P` / `R` and counts `BLE_GAP_EVENT_NOTIFY_TX` with its status. The sending stays unchanged.

Record the SHA-256 of each uploaded file (rules out D).

## 10. r2 hardware evaluation: method A passes (2026-10-16)

**Results (author, hardware):**

| Connection | Test | Result |
| --- | --- | --- |
| BLE | YouTube: fast rotation, then an abrupt reversal | **Good feel**: follows the new direction |
| BLE | TextEdit: reversals | Follows the direction change correctly |
| USB | KEY STEP 10: fast rotation, then reversal | Output switches correctly from `。` to `、` (YOUTUBE keys `.` / `,` as typed with the Mac's Japanese input) |

**Decisions (author / Edgar):**
- **Method A meets the pass criterion** ("fast right, then left: it moves left") on hardware, over BLE and USB.
- **Method B (BLE stack-aware pacing) is not implemented.** The residual old-direction movement after a reversal was not noticeable in this test.
- **`MediaJog_2a_3_r2` is kept as the current build** (source `579dd306…`, app-only binary `a4ea24fd…`). `MediaJog_2a_3` and `MediaJog_2a_3_d1` stay unchanged for comparison and recovery.

**Not tested on hardware (as of this record):** KEY STEP 1 regression on r2, CUSTOM with HOLD during reversals, the knob press during a reversal, and normal-speed use over a longer time. The host tests cover them (§8).

**Open issue (not resolved, cause not determined): `BLE READY` without input.**

| Item | Record |
| --- | --- |
| Symptom | The Mac shows the device connected and the Cardputer shows `BLE READY` (`kb=1 ms=1`). The firmware counts the reports as accepted by the BLE host (`TX …`, counters rising), but no key reaches the Mac |
| Seen with | d1 after reinstalling, until the Mac re-paired. r2 after one re-pairing, in the run of §9 / §9.1–§9.2. Later r2 runs worked (this section) |
| Facts | The device-side path up to the BLE host is proven by the counters. All BLE and initialization code is identical in 2a-3, d1 and r2. A similar state was seen earlier (`STAGE_2A_2.md` §8.4 #1) |
| Not determined | Whether it is a Mac / link state (most likely, not proven), a link that accepts but does not transmit, or a firmware runtime effect. No reproduction recipe |
| If it happens again | Before any recovery step: the status screen and the trace page (`Fn+B`, `t`). Then, without unpairing, a fresh link via USB → BLE in CONNECTION, then Mac Bluetooth off / on; re-pair only as the last step. Record the SHA-256 of the installed file (`STAGE_2A_3_DESIGN.md` §9.2) |

## 11. r2 additional hardware tests (2026-10-16)

| Test | Setup | Result (author, hardware) |
| --- | --- | --- |
| BLE, KEY STEP 1 | YOUTUBE profile | **PASS**: right, left and the knob press behave as before (KEY STEP 1 unchanged) |
| BLE, CUSTOM, reversal while HOLD | CW Right Arrow, CCW Left Arrow, BTN Shift (`Sft`), HOLD | **PASS**: knob held, turned left and right, as expected |

**Still not verified on hardware:**
- a new knob press during a reversal;
- long-term continuous use;
- normal-speed use over a long time.

The "`BLE READY` without input" issue stays open (§10).

**Status:** `MediaJog_2a_3_r2` remains the current build. Method A is hardware-verified; method B is not implemented. 2a-3, d1 and r2 sources and binaries are preserved.

## 12. Web MAME Arkanoid with the existing SPINNER (2026-10-16; hardware result, no code change)

Result for question (B) of §6: the web MAME accepts mouse input once its mouse option is enabled, and the **existing SPINNER (Fn+2) controls Arkanoid**. KEY STEP route (A) was not part of this test.

| Item | Record |
| --- | --- |
| Hardware | M5Stack Cardputer ADV + Unit Encoder U135 |
| Firmware | `MediaJog_2a_3_r2` (unchanged) |
| Profile / link | SPINNER (Fn+2), BLE |
| Host | Mac, Google Chrome |
| Game | Internet Archive web MAME, Arkanoid Tournament (US), https://archive.org/details/arcade_arkatour |
| MAME input setting | Dial Analog = Mouse X; Dial Analog Inc = RIGHT; Dial Analog Dec = LEFT |

| Check (author, hardware) | Result |
| --- | --- |
| Mouse movement output from the Cardputer | OK |
| Paddle moves left / right | OK |
| Speed | No problem ("no problem at all", author) |
| Responsiveness | No practical problem |
| Smoothness | Slightly stepped ("a little jerky") |
| Firmware change needed | None |

The SPINNER SENS step used was not recorded; it was the setting already on the device.

### 12.1 Enabling mouse input in the Internet Archive web MAME

- **First attempt:** the Mac pointer moved, but the paddle did not. The cause is likely that mouse input was not enabled in the web MAME (not proven in detail).
- **Working method** (hardware-verified once, Chrome on the Mac):
  1. Reload the game page in Chrome.
  2. **Before starting the game,** run this in the address bar:
     ```
     javascript:MAMELoader.extraArgs=(z)=>({extra_mame_args:['-mouse']});AJS.emulate()
     ```
     Chrome strips `javascript:` from pasted text, so `javascript:` was typed by hand and the rest pasted.
  3. When the web MAME starts, the SPINNER moves the paddle.
- **Depends on the Internet Archive's page code** (`MAMELoader`, `AJS`). It may stop working at any time; not guaranteed.
- Reference: Alexander Pruss, "Two tweaks for Internet Arcade games on archive.org", https://alexanderpruss.blogspot.com/2023/02/two-tweaks-for-internet-arcade-games-on.html

### 12.2 Restoring the Mac pointer

- With mouse input enabled, the Mac pointer stayed hidden after leaving the game.
- **Pressing Esc once restored the pointer** (hardware-verified).
- Any published web MAME procedure **must** include this step.

### 12.3 Stepped movement (cause not determined)

- The paddle movement feels slightly stepped relative to the rotation.
- **Cause not determined.** Candidates, none confirmed: the encoder resolution, the firmware's detent-to-movement conversion (SENS, half-count carry, per-loop report), the web MAME's input handling (e.g. per-frame sampling of the dial).
- **Decision (author):** speed and responsiveness are good and are kept. No firmware change for smoothness now.

### 12.4 Consequences for the design

- §6 (B) is answered: the existing SPINNER is enough to play Arkanoid in the web MAME (with `-mouse`). The KEY STEP route (A) is no longer needed for this game; it was not tested here.
- **SPINNER SENS (14 steps) is kept:** fast movement for games like Arkanoid, fine movement for video editing, and user preference for general mouse use. The current setting was fast enough here.
- **Existing SPINNER (Fn+2): no change** to its settings or behavior.
- **C-SPINNER (Fn+5, steps 2–3)** stays a **general, customizable SPINNER, not an Arkanoid feature**, as designed in §4 / §5: Fn+5; relative mouse X; its own SENS; direction reversal; button as key + modifiers, mouse left or right; TAP / HOLD; settings saved (`ext`). Top priority when it is built: Fn+2 settings and behavior stay unchanged. Implementation has **not** started.

### 12.5 Addendum: SPINNER SENS used (2026-10-16, author)

- The SPINNER sensitivity during the successful Arkanoid test (§12) was **64**. This replaces "not recorded" in §12.
- At 64: paddle speed no problem, responsiveness sufficient for practical use, slightly stepped movement (as §12.3).
- **Recorded as the baseline for later comparison tests** (for example of the stepped movement). It is consistent with the 48–64 Breakout range in `DEVELOPMENT_PLAN.md` P3; the firmware default stays unchanged.

## 13. Steps 2–3 implemented as Stage 2a-4 (2026-10-16; host-tested only)

C-SPINNER (§4, §5) is implemented in `MediaJog_2a_4/` (build `2a-4`), on top of r2, in one stage instead of steps 2 and 3. Details, review findings and test results: `STAGE_2A_4.md`.

- As designed: Fn+5, `C-SPINNER`, SPINNER path with its own 14-step sensitivity (default index 5 = 8), direction, button (key + modifiers / mouse left / mouse right / none, TAP / HOLD, default mouse left HOLD), `ext` v1 unchanged, PROFILE 5 restore with the `cfg` check.
- Differences from §4: `ok` on the SETUP rows is SAVE (as CUSTOM), so DIR is toggled with `2` or the knob press; SENS and MODE are also SETUP rows (request of 2026-10-16). To be confirmed.
- **Not tested on hardware.** r2 remains the hardware-verified build.
