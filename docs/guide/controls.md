# Controls

All settings are made on the Cardputer. The Cardputer's own keys are **never** sent to the computer.

## Connecting

| Action | Keys |
| --- | --- |
| Open the **CONNECTION** screen | `Fn + B` (in any profile) |
| Choose USB / Bluetooth LE | `1` = USB, `2` = BLE; or turn the knob |
| Apply | `ok` (Enter), or press the knob |
| Close without changing | `` ` `` (esc) |
| Show the Bluetooth diagnostic trace | `t` (repeat to page through) |

- **USB:** connect a USB-C data cable. Status `USB HID READY`.
- **Bluetooth LE:** after choosing BLE, pair **"MediaJog BLE"** in the computer's Bluetooth settings (no PIN). Status: `BLE ADVERTISING` while waiting, then `BLE READY`.
- The chosen transport is saved and used again at the next start.
- **Releases on switching:**
  - Opening CONNECTION releases anything held: a HOLD key, the mouse button.
  - Before a switch completes, the old transport gets an "all released" report.
  - If that release could not be confirmed, a red `!` appears in the top-right tag (for example `!USB 2a5`). The release is sent again when that transport is ready.

## Profiles

| Keys | Profile | Turn CW | Turn CCW | Knob press |
| --- | --- | --- | --- | --- |
| `Fn + 1` | YOUTUBE | `.` | `,` | `k` |
| `Fn + 2` | SPINNER | pointer right | pointer left | hold = left button |
| `Fn + 3` | EDITING | Right Arrow | Left Arrow | Space |
| `Fn + 4` | CUSTOM | configurable | configurable | configurable, TAP or HOLD |
| `Fn + 5` | C-SPINNER | pointer right (left if REVERSE) | pointer left (right if REVERSE) | key, mouse left / right or none; TAP or HOLD |

- **YOUTUBE:** in YouTube in a browser, `.` / `,` step one frame forward / back on a **paused** video, and `k` plays / pauses.
- **SPINNER:**
  - Horizontal mouse movement; holding the knob holds the left button, for dragging.
  - Sensitivity: `Fn + ;` up, `Fn + .` down (14 steps: 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128; default 8; saved 2 s after the last change). The screen shows the value and the step, for example `64` and `12/14`.
- **KEY STEP (YOUTUBE, EDITING, CUSTOM):** `Fn + ;` / `Fn + .` set how many key presses one knob step sends (1–10, default 1, one value per profile, saved 2 s after the last change). Only rotation is multiplied, never the knob press. When you turn the other way, presses still waiting in the old direction are dropped.
- **EDITING:** common editing shortcuts. Not tested with any editor.
- **CUSTOM:**
  - `Fn + S` (in Profile 4) opens the setup: choose a key with Ctrl / Shift / Alt / Command (GUI) for CW, CCW and the press. The press can be **TAP** or **HOLD**.
  - The keys are saved only when you choose **Save**.
  - SETUP画面のアルファベットはキー名として大文字で表示されます。実際に大文字を入力するには、Shift修飾キーを追加してください。
  - What a letter or symbol key types depends on the computer's keyboard layout and input method.
- **C-SPINNER:**
  - Like SPINNER, with its own sensitivity (`Fn + ;` / `Fn + .` in Profile 5 change only C-SPINNER), a direction setting and a configurable knob press. Works over USB and Bluetooth LE.
  - `Fn + S` (in Profile 5) opens C-SPINNER SETUP. Nothing is sent to the computer while it is open, and anything held is released when it opens.

    | Row | Change it | Values |
    | --- | --- | --- |
    | `1 SENS` | `1` or knob press, then turn the knob or `Fn + ;` / `Fn + .`; `ok` or knob press = done, `` ` `` = undo | 1 … 128 (14 steps) |
    | `2 DIR` | `2` or knob press | NORMAL / REVERSE |
    | `3 BTN` | `3` or knob press, then: type a key, or turn the knob for a list (arrows, Space, Enter, F1–F12 …); `Fn + C` / `S` / `A` / `G` Ctrl / Shift / Alt / Command; `Fn + L` mouse left; `Fn + R` mouse right; `del` none; `Fn + H` TAP/HOLD; `ok` or knob press = done, `` ` `` = undo | key with modifiers, modifiers only, mouse left, mouse right, none |
    | `4 MODE` | `4` or knob press | TAP / HOLD |

  - On the rows, turning the knob moves the selection. `ok` (Enter) saves (and stores the settings), `` ` `` (esc) cancels.
  - **TAP:** one key press or one click per knob press, however long you hold it. **HOLD:** the key or button is held while you hold the knob (a mouse button on HOLD drags).
  - Defaults: sensitivity 8, NORMAL, mouse left, HOLD (the same as SPINNER's defaults).
  - Saved: SETUP when you save, the sensitivity 2 s after the last change, Profile 5 at once (it is restored at the next start).
  - Key names follow the CUSTOM convention (letters shown as uppercase key names; add Shift for a capital letter).

## On-screen help colours

In the help lines, the **operation** (a key such as `Fn+S`, `ok (Enter)`, `esc (`)`, `del`, or a knob action such as `knob press` or `turn knob`) is shown in **cyan**, and **what it does** in **white**: `Fn+S` (cyan) `: SETUP` (white). Values, the selected row, status texts and warnings keep their own colours.

## The status screen

- **First line:** the connection state (`USB HID READY`, `USB NOT MOUNTED`, `USB SUSPENDED`, `BLE ADVERTISING`, `BLE PAIRING`, `BLE SUBSCRIBING`, `BLE READY`).
- **Top-right tag:** the active transport and the firmware (`USB 2a5` / `BLE 2a5`).
- **Under the tag:** the state of the saved settings.
- `SKIP (no link)` means the knob was used while nothing was connected. Such inputs are dropped, never queued.
