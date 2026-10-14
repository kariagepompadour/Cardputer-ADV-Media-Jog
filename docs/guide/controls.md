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
  - If that release could not be confirmed, a red `!` appears in the top-right tag (for example `!USB 2a2`). The release is sent again when that transport is ready.

## Profiles

| Keys | Profile | Turn CW | Turn CCW | Knob press |
| --- | --- | --- | --- | --- |
| `Fn + 1` | YOUTUBE | `.` | `,` | `k` |
| `Fn + 2` | SPINNER | pointer right | pointer left | hold = left button |
| `Fn + 3` | EDITING | Right Arrow | Left Arrow | Space |
| `Fn + 4` | CUSTOM | configurable | configurable | configurable, TAP or HOLD |

- **YOUTUBE:** in YouTube in a browser, `.` / `,` step one frame forward / back on a **paused** video, and `k` plays / pauses.
- **SPINNER:**
  - Horizontal mouse movement; holding the knob holds the left button, for dragging.
  - Sensitivity: `Fn + ;` up, `Fn + .` down (14 steps, saved 2 s after the last change).
- **EDITING:** common editing shortcuts. Not tested with any editor.
- **CUSTOM:**
  - `Fn + S` (in Profile 4) opens the setup: choose a key with Ctrl / Shift / Alt / Command (GUI) for CW, CCW and the press. The press can be **TAP** or **HOLD**.
  - The keys are saved only when you choose **Save**.
  - SETUP画面のアルファベットはキー名として大文字で表示されます。実際に大文字を入力するには、Shift修飾キーを追加してください。
  - What a letter or symbol key types depends on the computer's keyboard layout and input method.

## The status screen

- **First line:** the connection state (`USB HID READY`, `USB NOT MOUNTED`, `USB SUSPENDED`, `BLE ADVERTISING`, `BLE PAIRING`, `BLE SUBSCRIBING`, `BLE READY`).
- **Top-right tag:** the active transport and the firmware (`USB 2a2` / `BLE 2a2`).
- **Under the tag:** the state of the saved settings.
- `SKIP (no link)` means the knob was used while nothing was connected. Such inputs are dropped, never queued.
