# Installation

## What you need

- A **Cardputer ADV** with an **app launcher that supports Wi-Fi upload already installed**. This project was tested that way. Installing a launcher itself is a separate step that writes the device's flash; follow that launcher's own instructions if you do not have one yet.
- A **Unit Encoder (U135)** and its Grove cable.
- Before you start, keep copies of any app files you rely on (for example on the SD card).

## Install the firmware (tested path)

1. Download the **app-only** file `firmware/MediaJog_2a_5/MediaJog_2a_5.ino.bin`.
2. Check it. From the `firmware/MediaJog_2a_5` folder:

   ```bash
   shasum -a 256 -c SHA256SUMS.txt
   ```

   Both lines must end with `OK`. On Linux, use `sha256sum -c SHA256SUMS.txt`.
3. Upload the file with your launcher's **Wi-Fi upload**, then start it from the launcher.
4. Connect the U135 to the Cardputer's external Grove port **while the Cardputer is powered off**. Then power on and start the app.
5. Connect over USB or Bluetooth LE: [controls.md](controls.md#connecting).

**Keep your launcher:**
- Install only the app-only `.ino.bin`, and only through the launcher.
- A merged image (`*.merged.bin`, not provided here) contains a bootloader and a partition table. Writing it to the device with a flashing tool can replace the launcher and other apps.
- With this project's board package (`m5stack:esp32` 3.3.9), the Arduino IDE's **Upload** writes the bootloader, the partition table, `boot_app0` and the app, according to the board package's upload settings. Use **Export Compiled Binary** instead (see below).
- This firmware never erases the Cardputer's NVS storage. Nothing in these instructions erases it either.

## USB-only alternative

`firmware/MediaJog_1e_4/MediaJog_1e_4.ino.bin` is the USB-only firmware with the first four profiles (no C-SPINNER, no KEY STEP) and saved settings. It is installed the same way.

## Building from source

- **Board package:** `m5stack:esp32` **3.3.9**.
- **Board:** **M5Cardputer**.
- **Tools settings:**
  - **USB Mode: USB-OTG (TinyUSB)**
  - **USB CDC On Boot: Enabled**

  The sketches stop the build with an `#error` if either setting is different.
- **Libraries:** M5Cardputer 1.1.1, M5Unified 0.2.25, M5GFX 0.2.32, M5Unit-Encoder 0.0.2. Bluetooth LE uses the BLE library bundled with the board package (NimBLE).

With arduino-cli:

```bash
arduino-cli compile --fqbn "m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc" --output-dir build firmware/MediaJog_2a_5
```

The app-only file is `build/MediaJog_2a_5.ino.bin`. In the Arduino IDE, use **Sketch → Export Compiled Binary** and install the `.ino.bin` through the launcher.

A build on your machine is not guaranteed to be byte-identical to the provided file. The provided file is the one that was tested.

## Recovery

| Situation | What to do |
| --- | --- |
| The app does not start or the screen stays dark | Power-cycle the Cardputer and return to your launcher by that launcher's own method, then start a known-good app |
| The device no longer boots at all | Enter download mode: set the power switch OFF, hold **G0** while applying power, then release it (per M5Stack's Cardputer ADV documentation). Then reinstall your launcher following its own instructions |

This is an open hobby project, provided as-is. Use it only if you are comfortable with these recovery steps.
