# Architecture (Stage 2a-2)

An overview of how the recommended firmware ([`firmware/MediaJog_2a_2`](../../firmware/MediaJog_2a_2/)) works. The full design and test records are in [`docs/history/`](../history/); the main ones are `STAGE_2A_DESIGN.md`, `STAGE_2A.md`, `STAGE_2A_2_DESIGN.md` and `STAGE_2A_2.md`.

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
- **Profiles:** YOUTUBE, SPINNER, EDITING and CUSTOM map these events to keys or mouse actions.
  - Keyboard profiles queue events (a small queue, sent in order).
  - SPINNER sends relative X movement, scaled by one of 14 sensitivity steps.
- **Local keys:** the Cardputer's own keyboard only changes local settings (profile, sensitivity, CUSTOM setup, CONNECTION). Its keystrokes are never sent to the computer.

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

- **Where:** the Cardputer's NVS storage, under the app's own namespace `mediajog`. Two records, each with a magic, a version and a CRC-32:
  - **settings:** profile, CUSTOM keys, sensitivity;
  - **link:** the active transport.
- **When written:**
  - the profile and the transport at once;
  - CUSTOM only on Save;
  - the sensitivity 2 s after the last change;
  - nothing when the value is unchanged.
- **Safety:**
  - A damaged or newer record is never overwritten silently.
  - A held key or button is never restored.
  - The firmware never erases NVS.

## Status

- **Main functions:** tested on real hardware (see the README).
- **Not tested on hardware:** several paths, such as repeated switching, the owed-release `!` path, and HOLD over Bluetooth LE. They are listed in `STAGE_2A_2.md` §8.3.
