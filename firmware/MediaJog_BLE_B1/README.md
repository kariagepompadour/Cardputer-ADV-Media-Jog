# MediaJog_BLE_B1 — reference source (do not install)

This is the **Bluetooth LE test firmware** that proved the final Bluetooth design: one HID service with keyboard input, keyboard output and mouse input reports, registered as a fixed NimBLE GATT table. Stage 2a-2 uses its Bluetooth part unchanged.

- **Not for normal use.** It is Bluetooth-only, has a fixed key mapping and test keys, and no USB HID. No firmware file is provided.
- Use [`../MediaJog_2a_2/`](../MediaJog_2a_2/) instead.
- Design and test record: [`docs/history/BLE_B1_DESIGN.md`](../../docs/history/BLE_B1_DESIGN.md) and [`docs/history/BLE_B1.md`](../../docs/history/BLE_B1.md). Overview: [`docs/technical/ble-gatt.md`](../../docs/technical/ble-gatt.md).
