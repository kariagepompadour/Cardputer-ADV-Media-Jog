# Bluetooth LE HID: the fixed GATT table

Stage 2a-2 registers its Bluetooth LE HID services as a **fixed NimBLE GATT table**. This page explains why, and what the table contains. Full records: `BLE_M1.md`, `BLE_B1_DESIGN.md` and `BLE_B1.md` in [`docs/history/`](../history/).

## Why not the Arduino BLE HID class

The first Bluetooth firmware used the BLE library's `BLEHIDDevice` class (board package `m5stack:esp32` 3.3.9, NimBLE build). Two library behaviors made the mouse impossible.

1. **One characteristic per UUID and service.**
   - The library's C++ service keeps only the first characteristic of a UUID.
   - HID reports all share the UUID 0x2A4D, so the keyboard output report and the mouse input report were never registered.
   - The keyboard worked; the mouse could not.
2. **Handles depend on memory addresses.**
   - The library orders services and characteristics by object address.
   - The attribute handles can therefore change between builds. A bonded computer sees that as an unannounced change of the device.

A mouse in a **second** HID service did register. On hardware, however, macOS did not subscribe to it.

## The solution: one HID service, static table

The table is defined with the NimBLE C API (`ble_gatt_svc_def[]`) and registered before the GATT server starts. NimBLE's own table allows repeated UUIDs, so every report gets its own handle. The order, and with it every handle, is fixed by the source.

| Service | Characteristic | Properties | Descriptors |
| --- | --- | --- | --- |
| Device Information (0x180A) | Manufacturer, PnP ID | read | — |
| HID (0x1812) | HID Information | read | — |
| | Report Map (keyboard ID 1 + mouse ID 2) | read | — |
| | HID Control Point | write without response | — |
| | Protocol Mode (Report Protocol) | read, write without response | — |
| | Report: keyboard input | read, notify | CCCD, Report Reference {1, input} |
| | Report: keyboard output (LEDs) | read, write, write without response | Report Reference {1, output} |
| | Report: mouse input | read, notify | CCCD, Report Reference {2, input} |
| Battery (0x180F) | Battery Level (fixed 100) | read, notify | CCCD, Presentation Format |

- **Subscriptions:** a GAP event listener receives the subscription events of the two input reports and passes them to the same link logic as before. The reconnection fix ("B7") is unchanged.
- **Sending:** reports are sent with `ble_gatts_notify_custom()`, only to the current connection and only while that report is subscribed.

## Deliberate details

These keep the table the same as the Bluetooth test firmware that passed on hardware (`MediaJog_BLE_B1`):
- **No encryption flag on the reports.** The earlier, working firmware registered them without one, because the library ignored the setting. The firmware still sends only on an encrypted link.
- **PnP ID byte order** as the library wrote it (high byte first). The specification is little-endian; a change would alter the table.
- **Read-only descriptors.**

## Consequences

- **Limit of 2 hosts.** Each paired computer stores 3 subscription records (keyboard, mouse, battery). The stack keeps at most 8. Pairing a third computer can make the stack delete the oldest other pairing, so **at most 2 Bluetooth LE computers** are supported.
- **Firmware with a different table looks like a different device** to a bonded computer. Do not go back to older Bluetooth firmware after installing Stage 2a-2.
