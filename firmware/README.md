# Firmware

| Folder | Status | Use it for |
| --- | --- | --- |
| [`MediaJog_2a_2/`](MediaJog_2a_2/) | **Recommended.** USB **and** Bluetooth LE, all four profiles. Main functions tested on real hardware ([results](../README.md#tested-platforms-and-results)) | Normal use |
| [`MediaJog_1e_4/`](MediaJog_1e_4/) | USB only, all four profiles, saved settings. Tested on macOS over USB | If you want USB only, or as a fallback |
| [`MediaJog_BLE_B1/`](MediaJog_BLE_B1/) | Bluetooth LE test firmware (source only) | **Reference only. Do not install**, see its README |

Each installable folder has the Arduino sketch (`.ino`), the **app-only** firmware file (`.ino.bin`) and `SHA256SUMS.txt`.

## Check the files

The paths in `SHA256SUMS.txt` are relative to the folder itself:

```bash
cd firmware/MediaJog_2a_2
shasum -a 256 -c SHA256SUMS.txt
```

Both lines must end with `OK`. On Linux, use `sha256sum -c SHA256SUMS.txt`.

| File | SHA-256 |
| --- | --- |
| `MediaJog_2a_2.ino` | `9ec07fa4ad6bea1053d7bd4b2b15ce46d4e62e27e2755a62892bc19a112fbbd9` |
| `MediaJog_2a_2.ino.bin` | `4339eed644f01468548f6444501c287e74c14e5bd1d1b5de1448abdd42a8019b` |
| `MediaJog_1e_4.ino` | `04d0f590db626700b5c6578096d81169d52832a582874dda2f484e88c89018f5` |
| `MediaJog_1e_4.ino.bin` | `8db62fc43dfce3592b510ac232c3588a5a5fb6580c0a3b7d20105c465195d61c` |
| `MediaJog_BLE_B1.ino` | `7f5ab35dec573d51e9b373946b96654d1715fa60e14278248872ff60de32c95b` |

These are the same files, with the same checksums, as recorded in the development history ([`docs/history/`](../docs/history/)).

## Notes

- Use only the app-only `.ino.bin`, installed through a launcher. Merged images are not provided on purpose: they contain a bootloader and a partition table, and writing one with a flashing tool can replace your launcher.
- Installing and building: [`docs/guide/installation.md`](../docs/guide/installation.md).
- After installing Stage 2a-2, do not go back to older Bluetooth firmware. See [Known limitations](../README.md#known-limitations).
