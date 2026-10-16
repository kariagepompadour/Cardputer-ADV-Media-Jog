# Stage 2a-3 — Step 1: KEY STEP and the `ext` record

Created: 2026-10-16 · Implementation: Claude Opus 5.5 · Design: `STAGE_2A_3_DESIGN.md`
**Status: step 1 built and host-tested. NOT installed on hardware.**
- Stage 2a-2 and all earlier builds are unchanged (their `SHA256SUMS.txt` verify).
- Steps 2 and 3 (Fn+5) are not started; they wait for the step-1 hardware check.

Legend: ✅ checked in source, build or host test · ❓ not verified on hardware

## 1. What step 1 changes (from 2a-2)

| Area | Change |
| --- | --- |
| KEY STEP | `keyStep[3]` for PROFILE 1 / 3 / 4, 1–10, default 1. `Fn+;` / `Fn+.` change the current key profile's value (`changeKeyStep()`), shown as `KEY STEP n` on the yellow line. PROFILE 2 keeps its spinner sensitivity on the same keys |
| Queue | `queueRep[]`: tap count per entry. `pushDetent()` is used for rotation in PROFILE 1 / 3 / 4: KEY STEP 1 calls exactly 2a-2's `pushEvent()` / `pushCustom()`; KEY STEP ≥ 2 queues one entry with KEY STEP taps, or drops the detent as a whole if more than `KEY_PENDING_MAX` (100, provisional) taps would wait. `sendQueued()` sends up to max(4, KEY STEP) taps per loop and keeps an entry at the head until all its taps were sent. `pushEvent()` and `queuePushRaw()` set the count 1 for every other entry |
| NVS | New `mediajog/ext` record (`MJEX` v1, 21 bytes, CRC-32), read at boot (`loadExt()`). Written by `serviceExtSave()` 2 s after the last KEY STEP change and only if different. `NV WAIT` is shown meanwhile. `cfg` code is unchanged |
| Display | PROFILE 1 / 3 status: `KEY STEP n` appended to the `KB keys…` line; `P<max pending> D<drops>` on the `BTN raw…` line (measurement). PROFILE 4 body: ` STEP n` on its small status line. Font sizes and the Overview are unchanged. Tag `USB 2a3` / `BLE 2a3`, build id `2a-3` |

**Function comparison with 2a-2:**
- 129 identical, including `tapKey`, `sendRelease`, `sendKeyReport`, `onButton`, `changeSensitivity`, `setProfile`, `releaseHoldNow`, `queueClear`, `pushHoldDown` / `Up`, `pushCustom`, all settings / NVS functions of `cfg`, all BLE / B7 functions and `bleBegin`.
- **Changed (10):** `queuePushRaw`, `pushEvent` (one line each: count 1), `sendQueued`, `onRotation`, `pollKeyboard`, `nvStateText`, `drawStatus`, `drawCustomStatusBody`, `setup` (calls `loadExt()`), `loop` (calls `serviceExtSave()`).
- **New (10):** `keyStepFor`, `pendingTaps`, `pushDetent`, `changeKeyStep`, `extDefaults`, `extValid`, `encodeExt`, `decodeExt`, `loadExt`, `serviceExtSave`.
- **GATT identity with BLE-B1:** PASS (unchanged).

## 2. Host tests ✅ (logic only, not hardware)

| Suite | Result |
| --- | --- |
| Stage 2a end to end (ported, tags `2a3`) | 85/85 |
| Stage 2a-2 scenarios N01–N08 | 32/32 |
| NVS (1e-4), 13 functions identical to 2a-2 | 47/47 |
| HOLD (1e-3), queue functions of 2a-3 with all KEY STEPs 1 | 16/16 |
| **KEY STEP / ext (new, K01–K14)** | **35/35** |
| **Differential, KEY STEP 1 vs 2a-2:** the same random input script (all four profiles, rotation bursts of up to 3 detents per loop, half detents, knob presses with CUSTOM HOLD, profile switches, injected USB send failures / BLE rejections) on both builds | **identical HID output**: 1,711 USB reports and 1,725 BLE reports, byte for byte |
| Rollback | NVS written by 2a-3 (`cfg` + `ext`): 2a-2 boots with `CFG RESTORED`, same profile, writable |

**New scenarios:**

| Scenario | Checks |
| --- | --- |
| K01 | No `ext`: KEY STEP 1/1/1, nothing written; status shows `KEY STEP 1` and `USB 2a3` |
| K02 | `Fn+;` / `Fn+.` change only the current profile's value; ends 10 and 1; PROFILE 2 still changes the spinner sensitivity; PROFILE 3 independent |
| K03 | KEY STEP 5: one detent = exactly 5 `.` taps, press / release alternating; 3 detents in one loop = 15; the knob press stays one `k` |
| K04 | KEY STEP 10: the whole detent (10 taps) within the loop that read it |
| K05 | FIFO: 2 detents CW then 1 CCW at KEY STEP 3 = 6 `.` then 3 `,` |
| K06 | 15 detents at once, KEY STEP 10: 100 taps sent, 5 detents dropped as a whole; status `P100 D5` |
| K07 | CUSTOM: Ctrl+Right ×3 per detent; unassigned direction ignored once; HOLD Shift + rotation = 3 taps over the Shift base, then all-up |
| K08 | Profile switch with taps pending: queue cleared, nothing sent after it, all-up |
| K09 | BLE, KEY STEP 4, first press rejected: retried, exactly 4 taps |
| K10 | USB lost with taps pending: nothing replayed |
| K11 | `ext` written once, 2 s after the last change (`NV WAIT` before); content incl. C-SPINNER defaults (index 5, normal, mouse left HOLD); `cfg` unchanged; changed and changed back = no write |
| K12 | Stored KEY STEP 7 / 4 / 2 restored and used |
| K13 | Damaged `ext`: defaults, kept until a change replaces it |
| K14 | Newer-version `ext`: works in RAM, never overwritten |

**Mutation checks (each must fail):**

| Mutant | Failing checks |
| --- | --- |
| KEY STEP ignored | 11 |
| Loop budget fixed at 4 | 1 |
| No backlog limit | 3 |
| Entry leaves the queue after its first tap | 9 |
| Knob press multiplied | 1 |
| No 2 s save delay | 3 |
| No "unchanged" skip | 1 |
| `ext` not applied at boot | 2 |
| `Fn+;` changes KEY STEP in PROFILE 2 | 1 |
| Newer `ext` overwritten | 1 |

## 3. Build (2026-10-16)

| Item | Result |
| --- | --- |
| Clean build | ✅ exit 0, 0 sketch warnings; `#error` guards effective. Flash 891,599 B (68 %), RAM 69,524 B |
| Source `MediaJog_2a_3/MediaJog_2a_3.ino` | 182,262 B, SHA-256 `a9313f2772d6bc540c73e5c9c3f935483e0e596ad7bbbe6059ab4e7751d12327` |
| App-only binary `MediaJog_2a_3/build/MediaJog_2a_3.ino.bin` | 891,744 B, SHA-256 `9f222fc42248f3235eabdc013c8db2455696553b119d6dfe783ff4287e48c7e6` |
| Host-tested source | Equals the final preprocessed source |

## 4. Hardware check for step 1 (proposed; after review)

**Rules:**
- Install the app-only binary through the Launcher.
- No NVS erase, no bond deletion. The Mac's 2a-2 Bluetooth bond should be reused: the GATT table is identical.

| # | Test | Look for |
| --- | --- | --- |
| 1 | Start; restart once | Tag `2a3`; settings of 2a-2 restored (`CFG RESTORED`), KEY STEP 1 everywhere |
| 2 | KEY STEP 1 regression: YouTube, EDITING, CUSTOM (incl. HOLD), SPINNER, over USB and BLE | Same feel as 2a-2 |
| 3 | YouTube (paused video), KEY STEP 1 / 5 / 10, slow single detents | 1 / 5 / 10 frames per detent |
| 4 | **Measurement:** KEY STEP 10, spin fast, over USB and over BLE | `P` (most pending taps) and `D` (dropped detents) on the status screen; how long the output continues after stopping. These decide `KEY_PENDING_MAX` |
| 5 | `Fn+;` / `Fn+.` in each profile; PROFILE 2 still changes the spinner sensitivity | `KEY STEP n`; values independent per profile |
| 6 | Power off / on | KEY STEPs restored; `NV OK` |
| 7 | Web MAME Arkanoid (A): Fn+3 with KEY STEP 1 / 5 / 10 | Paddle movement per detent |
| 8 | Web MAME (B), information for step 2: does the paddle follow Fn+2 after enabling mouse input in the emulator (click into the game, mouse / pointer settings)? | Yes / no |

## 5. Hardware result 1: BLE at KEY STEP 10, fast rotation (2026-10-16; analysis only)

**Observed (author with Edgar):**
- **USB:** YouTube at KEY STEP 10 works; no missed input or delay noticeable.
- **BLE:** KEY STEP 1 is fine, and KEY STEP 10 works at normal speed. With fast rotation, YouTube sometimes shows its loading spinner, and frame stepping **continues after the knob stops**.

**Status screen after the BLE test (photo):**

| Field | Value | Meaning (source) |
| --- | --- | --- |
| `.4067 ,2452 k0` | 6,519 | Taps sent (`txCount`, counted per accepted tap) |
| `Q0/17` | 17 | Queue now empty; at most 17 entries at once (`queueMax`, may come from KEY STEP 1 use earlier in the session) |
| `P100 D428` | 100 / 428 | The unsent-tap limit was reached. 428 detents (4,280 taps) were dropped as whole detents, as specified: about 40 % of the generated taps |
| `FAIL0 SKIP33` | 0 / 33 | No keyboard tap with unknown outcome. 33 inputs while not READY (before the connection) |
| **`W298 T22`** | **298 ms / 22 ms** | **Longest `loop()` body 298 ms; longest single tap (press + release) 22 ms** (`maxWorkMs`, `maxTapMs`). Over USB a tap takes about 2 ms |
| `fail10` | 10 | `mouseFails`: mouse reports with **unknown** outcome. The BLE backend never returns "unknown", so these come from USB use in the same power-on session. **Not related to this problem** |
| `nv w0 f0` | — | No NVS write during the test |

**Analysis (source-based; the throughput figures are estimates):**
- **Sending blocks over BLE.** `T22` means one BLE tap (two notifications) took up to 22 ms, so one notification took about 11 ms, roughly one connection interval. `W298` is close to 10 taps × 22 ms plus drawing.
  - So `ble_gatts_notify_custom()` evidently **waits** when the controller has no free buffer, instead of returning at once. Where exactly it waits (NimBLE host or the ESP-IDF HCI transport) is a hypothesis to confirm.
  - Consequence: the actual BLE rate is about **40–50 taps/s**, that is 4–5 detents/s at KEY STEP 10. USB reaches about 250 taps/s.
- **The delay after stopping comes mainly from the firmware queue.** Up to 100 taps wait there; at about 45 taps/s that is about 2 s of frame stepping after the knob stops. The hidden backlog in the BLE stack is small because sending blocks.
  - The Mac is unlikely to be the cause: over USB, the same YouTube use has no delay.
  - The YouTube spinner is probably caused by the seeks that continue after stopping (hypothesis; compare with USB at the same speed).
- **Side effect:** while a loop sends 10 BLE taps, the loop is blocked for up to about 300 ms. Encoder counts are not lost (the U135 accumulates), but key handling and the display react late.

**Countermeasures compared (no code change yet):**

| Measure | Expected effect | Drawback | Compatibility |
| --- | --- | --- | --- |
| A. Diagnostics only: show `notifyFails`, the current pending taps, the measured BLE tap rate and the time from the last detent to the last tap | Measures the real rate and delay | No behavior change | None |
| **B. BLE-only backlog limit in time** (pending taps ≤ measured BLE rate × about 0.3 s; whole detents dropped beyond) | Delay after stopping bounded to about 0.3 s plus the tap in progress | More dropped detents at fast rotation (delay traded for loss). The number of taps per detent stays exact | USB limit (100) and KEY STEP 1 unchanged |
| **C. BLE-only taps per loop** (for example 2–4 instead of up to 10) | The loop is no longer blocked for 300 ms, so keys and the display stay responsive. Throughput is unchanged, because the radio is the limit | Needs B, or the backlog just waits in the queue | USB and KEY STEP 1 unchanged |
| D. Request a shorter connection interval | Possibly a higher rate | macOS may refuse; changes the verified BLE behavior | Risk to BLE / B7 |
| E. Larger queue limit | Fewer drops | **More delay** (excluded) | — |
| F. Merging or cancelling taps | — | Changes KEY STEP (excluded) | — |

**Recommendation:** A first (a diagnostic build, no behavior change). Then B + C for BLE only, with the limits taken from the measurement. USB, KEY STEP 1 (the 2a-2 path), the press / release timing, the HID reports, GATT and NVS stay unchanged. The improvement is not confirmed until measured.

## 6. Diagnostic build d1 (`MediaJog_2a_3_d1/`, 2026-10-16)

**Purpose:** measure the BLE send rate, where sending waits, the pending taps and the tail (output after the knob stops) before deciding on measures B + C. Edgar's goal: drop as little input as possible, reflect the rotation accurately, and keep the delay low.

- `MediaJog_2a_3` is unchanged. **Measurement only:** no change to sending, queueing, USB, BLE connection, GATT or NVS.
- **Build id** `2a-3d1`; tag `USB 2a3d` / `BLE 2a3d`.

### 6.1 What is measured

| Item | How (cost) |
| --- | --- |
| BLE notify time, press and release separately | `micros()` before / after `ble_hs_mbuf_from_flat()` and `ble_gatts_notify_custom()` in `notifyReport()`, keyboard reports only. A report equal to the base state (no key beyond a HOLD) counts as release. Average and maximum |
| Where the wait is | Allocation time (`A`) separately from the notify call; lowest free msys mbuf count before a notification (`M`, `os_msys_num_free()`); BLE rejects (`n`, `notifyFails`); connection interval (`ci`, read from `ble_gap_conn_find()` once per second) |
| Tap rate | Taps accepted per 1 s window (`r` last / peak) and per burst (`b`: first queued detent until the queue is empty), over USB and BLE |
| Pending taps now | `q` (sum of the queue entries' tap counts) |
| Tail | `t`: time from the last queued detent to the last tap sent; last / max |
| Display cost | `dr`: longest `drawStatus()` in ms |

- Each measurement is a few `micros()` / `millis()` reads and counter updates.
- The screen is redrawn no more often than before (100 ms minimum), plus once per second on BLE for the rate.
- Serial prints one `[DIAG]` line only at the end of a burst of 10 or more taps (the TX timeout is 0, so it never waits).

**Status screen**, PROFILE 1 / 3 with BLE active (USB and the other screens as 2a-3):
- `W… T… nv w… f… …ms dr<draw ms> n<BLE rejects>`
- `r<rate>/<peak> b<burst rate> q<pending> t<tail>/<max> KS<KEY STEP>` (replaces the "KB keys…" line)
- `P<press avg>/<max> R<release avg>/<max> A<alloc max> M<min free msys> ci<interval ms>` (replaces the "MS host…" line). Times in ms; the averages have one decimal

### 6.2 Host tests ✅

| Suite | Result |
| --- | --- |
| Stage 2a suite / 2a-2 scenarios / KEY STEP K01–K14 on d1 | 85/85, 32/32, 35/35 |
| **Differential d1 vs 2a-3:** 3 random scripts × USB and BLE, now including random KEY STEP changes | **identical HID output**, 31,086 reports in total |
| Diagnostics D01–D06 | 12/12: press / release classified (5 + 5 for one detent at KEY STEP 5), notify time and allocation separated, minimum free mbufs, tail ≈ 180 ms for 100 queued taps at 10 per loop, burst and window rates, BLE screen lines, USB lines unchanged, USB burst measured, no BLE timing on USB |

**Function comparison with 2a-3:**
- 145 identical (including `sendQueued`, `pushEvent`, `tapKey`, `sendRelease`, the `ext` functions, `bleBegin`, `handleSubscribe`, `onSubscribeEvent`).
- **Changed:** `notifyReport` (timing lines only), `pushDetent` (detent timestamp), `drawStatus`, `loop` (draw timing, `dgService()`).
- **New:** `dgTapTotal`, `dgDetentQueued`, `dgService`.
- GATT table and registration: identical to BLE-B1.

### 6.3 Build

| Item | Result |
| --- | --- |
| Clean build | ✅ exit 0, 0 sketch warnings, guards effective; flash 893,011 B |
| Source `MediaJog_2a_3_d1/MediaJog_2a_3_d1.ino` | SHA-256 `d2f205611b917ab156eacc2c62be76e6b22c153e349b9fd076e24c8c92dddf02` |
| App-only binary (893,152 B) | SHA-256 `714f034c22310f567b4ad9c3108338d6e113321162b8835e2647276636d231a9` |
| Host-tested source | Equals the final preprocessed source |

### 6.4 Measurement procedure (hardware)

1. Install d1 (app-only, Launcher). Use BLE (the bond should be reused: same GATT table). PROFILE 1, YouTube, paused video. Note `ci`.
2. **Uncongested:** KEY STEP 10, ten single detents slowly (one per second). Note `P`, `R`, `A`, `M`, `n`.
3. **Congested:** KEY STEP 10, spin fast for about 5 s, then stop. Note `r` (peak), `b`, `q` just before stopping if visible, `t` (tail), `D`, `P100`, `W`, `T`, `dr`, `n`. Use a stopwatch or video for the visible tail.
4. Repeat step 3 with KEY STEP 5 and KEY STEP 1.
5. The same on USB (`b`, `t`) for comparison.
6. Optional: Serial log (115200), the `[DIAG]` lines.

### 6.5 How the results will be read

| Observation | Meaning |
| --- | --- |
| `P` / `R` average close to `ci` or `ci`/2, while `M` stays well above 0 and `n` = 0 | The notify call waits below the host's mbuf pool, in the HCI transport or controller, for room for the next packet. This is about 1 or 2 packets per connection event. The send rate is set by the connection |
| `M` near 0, `A` large or `n` > 0 | The host's mbuf pool runs out (rejects or allocation waits) |
| `b` clearly below 1000 / (`P` + `R`) | Other loop work (for example `dr`) takes a significant share between the sends |
| `t` ≈ `q` / `b` | The tail is the firmware queue; the stack adds little |

### 6.6 Room to raise the BLE send rate (to be decided after the measurement)

- **The rate is roughly (packets per connection event) ÷ (connection interval) ÷ 2 notifications per tap.** At 15 ms: about 33 taps/s with 1 packet per event, 67 with 2, 133 with 4. The earlier estimate, about 45 taps/s, lies between.
- **Shorter connection interval:** request it with `ble_gap_update_params()`. The host decides whether to accept. This changes the hardware-verified connection behavior, so it needs a B7 regression run and Edgar's decision.
- **More packets per event:** depends on the Mac and on the controller. The controller settings are fixed in the precompiled core. d1's `P` / `R` against `ci` shows the current number.
- **Not possible within the specification:** merging a press and a release, or several taps, into one report. A tap needs a down state and an up state.
- **Pacing (C)** does not raise the rate. It keeps the loop responsive, so the backlog stays where B can bound it.

## 7. d1 hardware results and analysis (2026-10-16; no code change)

**Observed (author with Edgar):**
- **USB, KEY STEP 10:** fast rotation comfortable. The YouTube spinner shows almost only when entering unloaded parts. The picture sometimes stops slightly late.
- **BLE, KEY STEP 10:** normal speed good. With fast rotation the YouTube spinner shows often. The delay after stopping does **not** feel very different from USB, and does not grow with longer spinning.
- **d1 screen after fast rotation:**
  - `P100`, `D632`, `n1250`;
  - peak rate about 70 taps/s;
  - tail max 1,554 ms;
  - `ci` 15.00 ms;
  - press / release notify times about 0 ms.

### 7.1 What the values mean (source facts)

| Value | Counter and reset | Meaning |
| --- | --- | --- |
| `n1250` | `notifyFails`, cumulative since boot. +1 for every `notifyReport()` that is not accepted: no client, no subscriber, or a host error (rc ≠ 0, including `BLE_HS_ENOMEM` when no mbuf can be allocated). Keyboard sends reach `notifyReport()` only when READY, so here they are host errors | **Not lost input.** A rejected press returns `TX_NOT_SENT`: the queue entry stays and is retried next loop. A rejected release sets `releasePending`, and it is retried first next loop. Under a backlog, every loop ends with one rejection (send until the stack is full, then wait). About 1,250 rejections ≈ 1,250 loops of about 20–30 ms with a backlog, about 30 s of fast rotation in total. **Rejections are the stack's flow control, not an error rate** |
| `P` / `R` ≈ 0 ms | Time of the `ble_gatts_notify_custom()` call only | The call **does not block**: it queues the notification in the host, or fails at once. **This refutes the earlier hypothesis (§5) that sending waits one connection interval.** `T22` there was a single maximum of `tapKey()`. Possible causes are a printed rejection (`Serial.printf` in `notifyReport()`) or preemption by the BLE host task; not determined. The time on air cannot be measured from the app |
| ≈ 70 taps/s | `r` peak: taps accepted (`txCount`) in a 1 s window, maximum since boot | About 140 notifications/s at 15 ms: **about 2.1 notifications per connection event**. During fast rotation this is the radio's actual rate (the stack was full all the time) |
| tail 1,554 ms | `t` max since boot: last queued detent → last **accepted** tap | ≈ 100 queued taps ÷ 70 taps/s (1.43 s) plus loop time: **the firmware queue drains at the radio rate**. The notifications still waiting in the host pool (up to 12 + 24 msys blocks, about 18 taps ≈ 0.25 s) come on top; this is a source-based estimate, `M` shows the real minimum |
| `D632`, `P100` | cumulative / maximum since boot | Whole detents dropped at the 100-tap limit; the limit was reached |

**Why many rejections but little felt delay:**
- Rejected sends are retried and not lost.
- The delay is bounded by the 100-tap limit and the drops. The tail cannot grow beyond about 1.4 s + 0.25 s, however long you spin, which matches "does not get longer".
- That it feels similar to USB may come from YouTube's own seek / render delay (USB also stops slightly late). This is not proven.
- The YouTube spinner is not taken as evidence of BLE delay.

### 7.2 Bottleneck (facts and estimates)

- **Fact:** the notify call is non-blocking. The firmware sends until the host refuses, then retries each loop (about every 20 ms plus the loop's work). The host's mbuf pool holds 36 blocks, more than one loop of radio capacity. So the polling interval probably does not starve the radio.
- **Estimate:** the limit is the link — about 2 packets per 15 ms connection event, which the Mac and the controller decide. KEY STEP 1 is far below it (70 detents/s).

### 7.3 Candidates (in Edgar's priority order)

| # | Candidate | Expected effect | Basis | Side effects / risk |
| --- | --- | --- | --- | --- |
| 1a | Prefer LE 2M PHY (`ble_gap_set_prefered_le_phy`) | Possibly more packets per connection event (each packet is half as long on air), so a higher rate | Packets per event can be limited by the event length the Mac schedules; not confirmed | The Mac may stay on 1M. Changes the hardware-verified link; B7 regression needed. Low code risk, unknown benefit |
| 1b | Request a shorter connection interval (`ble_gap_update_params`, e.g. 11.25 ms) | Up to +33 % connection events | The rate scales with events per second | The host may refuse. Power. Changes the verified connection behavior; B7 regression needed |
| 2a | Stack-aware pacing, BLE and KEY STEP ≥ 2 only: send while `os_msys_num_free()` is above a small reserve | The hidden host backlog (≈ 0.25 s) shrinks, and rejections nearly disappear. **The rate stays** as long as the reserve keeps the radio fed. No input dropped | The pool currently fills to the limit every loop | Needs a threshold (from `M`). USB and KEY STEP 1 untouched. Low risk |
| 2b | Count rejections without printing each one (`Serial.printf` per rejection) | Less loop time per rejection (relevant with a CDC host attached) | One print per loop under a backlog | None in function; Serial log less detailed |
| 3 | BLE-only time-based backlog limit (§5 B) | Shorter tail | — | Drops more detents; last resort only |

### 7.4 Recommendation

- **The current behavior is usable as it is.** Rejections are harmless, the delay is bounded, and KEY STEP 1 is unaffected. Keeping 2a-3 as it is is a valid decision.
- **If an improvement is wanted:** first 2a + 2b. They are low risk and drop nothing, and they remove the hidden host backlog and the rejection churn. 1a / 1b are a separate experiment, because they change the verified BLE link. Option 3 only if a shorter tail is still needed.
- None of these effects is confirmed before a hardware test.

### 7.5 Minimal extra information (no new firmware)

- From the same d1 screen after a fresh restart and one fast-rotation run: `M` (lowest free msys) and `A`.
  - `M` near 0 confirms that the host pool limits the sending, and sets the reserve for 2a.
- Optional: one line of the Serial log `[BLE] notify not accepted: status 3 code <n>` (code 6 = `BLE_HS_ENOMEM`).
