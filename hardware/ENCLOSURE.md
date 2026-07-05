# Enclosure

A parametric, 3D-printable two-part case for the Muninn line-tap device:
[`enclosure/muninn_case.scad`](enclosure/muninn_case.scad).

> **Status:** design starting point, **not printed or fit-verified** (project is pre-hardware).
> Measure your actual boards and adjust the `[Board]` parameters before printing.

## Layout

```
        ┌───────────────────────────┐
 back → │ [USB-C]                   │ ← lid: (button)   (LED pipe)
        │      ESP32-S3 + PCM1808    │
front → │ (program in) (voice in)   │
        └───────────────────────────┘
```

- **Back wall:** USB-C cutout (data + power to the listener PC).
- **Front wall:** two 3.5 mm panel jacks — **program in** (mixer tap → L) and **voice in** (mic bus → R).
- **Lid:** capture **button** and an **LED light-pipe** hole (the one-pixel VU meter).
- **Floor:** four self-tapping **M2.5** standoffs for the devkit.

## Parameters

Edit the `/* [ ] */` groups at the top of the `.scad` (OpenSCAD Customizer exposes them):

- `[Board]` — `board_l`, `board_w`, `board_clear_h` (headers + PCM1808 clearance).
- `[Panel cutouts]` — `usbc_w/h`, `jack_d`, `jack_spacing`, `button_d`, `led_d`.
- `[Part to render]` — `both` (exploded preview), `base`, or `lid`.

## Print

```bash
# base
openscad -D 'part="base"' -o muninn_base.stl enclosure/muninn_case.scad
# lid
openscad -D 'part="lid"'  -o muninn_lid.stl  enclosure/muninn_case.scad
```

- PLA/PETG, 0.2 mm layers, 3 perimeters, 20% infill. No supports needed for either part.
- Print the lid flat (holes up); the base sits on its floor.

## Extra hardware

| Part | Qty | Notes |
|------|-----|-------|
| M2.5 × 5 mm self-tapping screws | 4 | board to standoffs |
| M2.5 × 6 mm screws + heat-set inserts *(optional)* | 4 | lid to base, if you add lid bosses |
| 3 mm clear filament / acrylic rod | 1 | LED light pipe |
| 3.5 mm panel-mount jacks | 2 | program + voice inputs |

See [`BOM.md`](BOM.md) for the electronics bill of materials.
