# 3D-print files

| File | What it is | Size (bounding box) | Printed? |
| --- | --- | --- | --- |
| `Cardputer-ADV-Media-Jog.3mf` | Bambu Studio project with all three parts, made for a Bambu Lab A1 mini (0.4 mm nozzle, "0.20mm Standard" profile) | — | **Yes**: the author printed and used these parts |
| `MediaJog_Base_A.stl` | **Flat base** | 54.0 × 133.0 × 19.0 mm | Same model: exported from the printed 3MF |
| `MediaJog_Base_B.stl` | **15° tilted base** | 57.2 × 133.0 × 26.7 mm | Same model: exported from the printed 3MF |
| `MediaJog_Dial.stl` | **Jog dial** (large knob for the U135) | 30.0 × 30.0 × 15.0 mm | Same model: exported from the printed 3MF |

- **The 3MF is the tested source.** The locating posts are Ø 4.8 mm: Ø 4.9 mm was too tight in the mounting holes and Ø 4.7 mm slightly loose, and the Ø 4.8 mm version printed from this 3MF fits well. The STL files are exported from this printed 3MF, so they are the same model. Print **one** base, flat or tilted, and the dial.
- **The STL files were checked against the 3MF:**
  - All three are closed (manifold) meshes when opened in Bambu Studio, and their volumes match the 3MF parts.
  - They are in the same orientation as on the 3MF's print plate; the 15° base is already tilted.
  - **Dial:** in the 3MF, the dial is a cylinder with a negative part that makes the inside hollow. The STL has the same result built into the mesh: a Ø 10 mm cavity, 13 mm deep, with a 2 mm closed end. It opens on the side facing up on the print plate.
- **Mounting:** the bases hold the Cardputer ADV and the U135 with locating posts that fit the devices' LEGO-compatible mounting holes. Both devices can be taken out again.
- **The published 3MF** is the author's project file with the author's account identifier removed from its metadata. Nothing else was changed; opened in Bambu Studio, it gives the same model information as the original.
- The enclosure is optional: see [No 3D printer? No problem!](../README.md#no-3d-printer-no-problem).
