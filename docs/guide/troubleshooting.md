# Troubleshooting

These are the problems seen during testing, and what helped. **Causes marked "not determined" are not known.** The fixes are what worked in the observed cases, not guarantees.

| Symptom | What helped | Cause |
| --- | --- | --- |
| A key or the mouse button seems stuck on the computer | Unplug the USB cable, or over Bluetooth LE turn the computer's Bluetooth off and on. A red `!` in the tag means a release could not be confirmed; it is sent again when that transport is ready | — |
| `USB NOT MOUNTED`, and the computer does not list the device at all | Powering the Cardputer off and on. A restart with the cable connected did not help in the observed case | Not determined (firmware restart behavior and external causes such as cable, port, or the computer's accessory approval are not excluded) |
| `USB SUSPENDED` after unplugging and replugging USB | Power the Cardputer off and on. The saved profile and settings are restored | Not determined (seen with earlier USB builds; USB code unchanged) |
| Bluetooth: the computer lists "MediaJog BLE" but does not connect (`BLE ADVERTISING`) after a firmware update | Remove "MediaJog BLE" in the computer's Bluetooth settings and pair again | Not determined. Seen once after updating from an earlier Bluetooth test firmware; other updates kept the pairing |
| Bluetooth: a third computer was paired, and an earlier computer no longer connects | Pair that computer again. Use at most 2 computers | The Bluetooth stack's storage limit can delete the oldest other pairing |
| The app does not start | See [installation.md → Recovery](installation.md#recovery) | — |

If a problem is not listed, the CONNECTION screen (`Fn + B`) and its trace (`t`) show the connection details used during development.
