# Architecture (Stage 2a-5)

An overview of how the recommended firmware ([`firmware/MediaJog_2a_5`](../../firmware/MediaJog_2a_5/)) works. The transport and Bluetooth parts are unchanged since Stage 2a-2. The full design and test records are in [`docs/history/`](../history/); the main ones are `STAGE_2A_DESIGN.md`, `STAGE_2A.md`, `STAGE_2A_2_DESIGN.md`, `STAGE_2A_2.md`, `STAGE_2A_3_DESIGN.md`, `STAGE_2A_3.md`, `STAGE_2A_4.md` and `STAGE_2A_5.md`.

## One firmware, two transports

- **USB HID** (TinyUSB) is a composite keyboard + mouse. It is enumerated on every boot, in both modes, and only *sends* while USB is the active transport.
- **Bluetooth LE HID** (HID over GATT, NimBLE) starts only when Bluetooth LE is selected, at boot or at the first switch. It is started once per boot.
- **Exactly one transport is active** (`activeTransport`). Reports are built once and handed to the active backend only; nothing is ever sent to both.
- **Reports:**
  - an 8-byte keyboard report (modifiers, reserved, 6 keys);
  - a 5-byte mouse report (buttons, X, Y, wheel, pan).
  - Both layouts are the same on USB and Bluetooth LE.
- **Send results:** every send reports **SENT**, **NOT SENT** or **UNKNOWN**.
  - USB: transfer completed / endpoint not ready / completion not seen.
  - Bluetooth LE: accepted by the stack / not accepted.
  - A key press that was not sent is retried. Movement that may have been sent is never sent twice.

## Profiles and input

- **Input:** the U135 is read over I²C. Rotation is counted in detents. A press is a TAP, or with CUSTOM HOLD a held key.
- **Profiles:** YOUTUBE, SPINNER, EDITING, CUSTOM and C-SPINNER map these events to keys or mouse actions.
  - Keyboard profiles queue events (a small queue, sent in order). With **KEY STEP** 2–10, one queue entry carries that many taps. When the knob is turned the other way, waiting taps of the old direction are removed (other events keep their order).
  - SPINNER and C-SPINNER send relative X movement, scaled by one of 14 sensitivity steps; each has its own step. C-SPINNER can reverse the direction, and its knob press is a key (TAP / HOLD, through the keyboard queue) or a mouse button (HOLD, or TAP = one click released after the press report was confirmed).
- **Local keys:** the Cardputer's own keyboard only changes local settings (profile, sensitivity / KEY STEP, CUSTOM and C-SPINNER setup, CONNECTION). Its keystrokes are never sent to the computer.
- **Screen:** operation guides draw the operation in cyan and the description in white (Stage 2a-5, display only).

## Release rules (nothing stays pressed)

- **Every new connection** starts with an "all keys up" keyboard report and a released mouse report.
- **These release everything held at once:** a profile switch, opening CONNECTION or SETUP, link loss.
- **Switching transport:**
  1. The old transport gets the release first: up to 200 ms; Bluetooth LE adds 100 ms for transmission; the whole switch is bounded at 300 ms.
  2. Then the switch commits.
  3. If the release could not be confirmed, it is recorded as owed (red `!`) and sent first when that transport is ready again.
- **While a transport is not ready,** inputs are counted and dropped, never queued and replayed.

## Bluetooth LE link rules

- **READY** = connected **and** encrypted **and** the keyboard input report subscribed. Nothing is sent otherwise.
- **Mouse reports** are sent only while the mouse input report is subscribed.
- **Pairing:** "Just Works" with bonding and LE Secure Connections. Bonds are stored by the Bluetooth stack itself.
- **Event order:** for a bonded computer, the stack can deliver the restored subscriptions **before** it reports the connection. The firmware adopts the connection from such an event, so a reconnection becomes READY without a new subscription. This was found and fixed during development (BLE-1 r4, "B7"); see `BLE_1.md` in the history.
- **GATT:** the GATT layout is a fixed table: [ble-gatt.md](ble-gatt.md).

## Saved settings

- **Where:** the Cardputer's NVS storage, under the app's own namespace `mediajog`. Three records, each with a magic, a version and a CRC-32:
  - **settings** (`cfg`, unchanged since Stage 1e-4): profile 1–4, CUSTOM keys, SPINNER sensitivity;
  - **link:** the active transport;
  - **extension** (`ext`, since Stage 2a-3): the KEY STEPs, the C-SPINNER settings, and whether Profile 5 is active. Profile 5 is restored only if `cfg` still holds the profile it held when `ext` was written, so older firmware, which ignores `ext`, keeps working.
- **When written:**
  - the profile and the transport at once;
  - CUSTOM and C-SPINNER SETUP only on Save;
  - the sensitivities and KEY STEPs 2 s after the last change;
  - nothing when the value is unchanged.
- **Safety:**
  - A damaged or newer record is never overwritten silently.
  - A held key or button is never restored.
  - The firmware never erases NVS.

## Status

- **Main functions:** tested on real hardware (see the README). Stage 2a-5 differs from the hardware-tested Stage 2a-4 only in the screen colours.
- **Not tested on hardware:** several paths, such as repeated switching, the owed-release `!` path, and HOLD over Bluetooth LE. They are listed in `STAGE_2A_2.md` §8.3.
