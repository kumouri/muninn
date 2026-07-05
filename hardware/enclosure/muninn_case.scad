// muninn_case.scad — parametric two-part enclosure for the Muninn line-tap device.
// Board: ESP32-S3-DevKitC-1 + PCM1808 breakout. Units: mm. Open in OpenSCAD.
//
// Cutouts: USB-C (back), 2x 3.5 mm panel jacks (front: program in, voice in),
// capture button + LED light-pipe (lid). Tune the [Board] values to your measured parts.
//
// NOTE: dimensions are a sensible starting point, not a verified fit — measure your boards and
// reprint. Nothing here has been printed yet (project is pre-hardware).

$fn = 48;

/* [Part to render] */
part = "both"; // [both, base, lid]

/* [Board] */
board_l = 69;         // ESP32-S3-DevKitC-1 length incl. USB shell
board_w = 26;
board_clear_h = 14;   // clearance above the PCB for headers + PCM1808

/* [Case shell] */
wall = 2.4;
floor_t = 2.0;
gap = 1.6;            // clearance around the board footprint
corner_r = 3;

/* [Standoffs] */
standoff_h = 4;
standoff_od = 6;
screw_hole_d = 2.6;   // self-tapping M2.5

/* [Lid] */
lid_t = 2.0;
lip_h = 3;
lip_gap = 0.35;

/* [Panel cutouts] */
usbc_w = 11; usbc_h = 5.5;
jack_d = 6.5;         // 3.5 mm panel-mount jack thread
jack_spacing = 15;    // between the two jack centers
button_d = 6.5;
led_d = 3.2;          // light pipe

// ── Derived ──────────────────────────────────────────────────────────────────
inner_l = board_l + 2 * gap;
inner_w = board_w + 2 * gap;
inner_h = standoff_h + board_clear_h;
outer_l = inner_l + 2 * wall;
outer_w = inner_w + 2 * wall;
outer_h = floor_t + inner_h;
z_pcb   = floor_t + standoff_h + 1.6;   // top of the PCB
z_jack  = outer_h / 2;

module rrect(l, w, r, h) {
  linear_extrude(h) offset(r) offset(-r) square([l, w], center = true);
}

// Rectangular cut through an X wall (w along Y, h along Z), centered on the wall plane.
module x_wall_cut(x_sign, y, z, w, h) {
  translate([x_sign * outer_l / 2, y, z]) cube([2 * wall + 2, w, h], center = true);
}

// Round cut through an X wall.
module x_wall_round(x_sign, y, z, d) {
  translate([x_sign * outer_l / 2, y, z]) rotate([0, 90, 0])
    cylinder(h = 2 * wall + 2, d = d, center = true);
}

module panel_cutouts() {
  x_wall_cut(-1, 0, z_pcb, usbc_w, usbc_h);                 // USB-C (back)
  x_wall_round(1, -jack_spacing / 2, z_jack, jack_d);       // program-in jack (front)
  x_wall_round(1,  jack_spacing / 2, z_jack, jack_d);       // voice-in jack (front)
}

module standoffs() {
  bx = board_l / 2 - 3;
  by = board_w / 2 - 3;
  for (sx = [-1, 1], sy = [-1, 1])
    translate([sx * bx, sy * by, floor_t])
      difference() {
        cylinder(h = standoff_h, d = standoff_od);
        translate([0, 0, -1]) cylinder(h = standoff_h + 2, d = screw_hole_d);
      }
}

module base() {
  difference() {
    rrect(outer_l, outer_w, corner_r, outer_h);
    translate([0, 0, floor_t]) rrect(inner_l, inner_w, max(0.1, corner_r - wall), outer_h);
    panel_cutouts();
  }
  standoffs();
}

module lid() {
  button_x = outer_l / 4;
  led_x = -outer_l / 4;
  difference() {
    union() {
      rrect(outer_l, outer_w, corner_r, lid_t);
      translate([0, 0, -lip_h])
        rrect(inner_l - 2 * lip_gap, inner_w - 2 * lip_gap, max(0.1, corner_r - wall), lip_h);
    }
    translate([button_x, 0, -lip_h - 1]) cylinder(h = lid_t + lip_h + 2, d = button_d);
    translate([led_x, 0, -lip_h - 1]) cylinder(h = lid_t + lip_h + 2, d = led_d);
  }
}

if (part == "base" || part == "both") base();
if (part == "lid" || part == "both")
  translate([0, 0, (part == "both") ? outer_h + 15 : 0]) lid();  // exploded preview
