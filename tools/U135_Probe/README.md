# U135_Probe

A small diagnostic sketch for the Unit Encoder (U135) on the Cardputer ADV. It only reads the encoder and shows the values on the screen. It does not act as a USB keyboard or mouse.

It was the first step of this project: it showed that the U135 answers on the external Grove port (I²C address 0x40) and how the display must be started. It is based on an M5Stack example (MIT, see [`LICENSES/M5Stack-MIT.txt`](../../LICENSES/M5Stack-MIT.txt)).

Only the source is provided. Build it with the M5Stack board package (`m5stack:esp32`, board **M5Cardputer**) and the M5Unit-Encoder library. `SHA256SUMS.txt` lists the source checksum.
