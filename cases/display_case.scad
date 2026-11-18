// Display Unit Case for LDG Controller
//
// One continuous case on the same 80 x 105 footprint as the remote case, but
// oriented with the 105mm side across the front so the 3.5in TFT fits
// landscape.  The ESP32, PowerPole, and display share one cavity.
//
// Front (-X): tilted-back display face.  The TFT PCB mounts to the inside of
// the front wall; only the screen aperture is cut through, so there is no
// large hidden opening behind the bezel.
// Back (+X):  PowerPole at the bottom and the tuner cable entry/clamp.
//
// The bottom electronics reuse the remote case's proven floor/PP modules.
// A top lid covers the single cavity.

include <common.scad>
use <nodisplay_case.scad>

$fn = 50;

// ============================================================================
// CASE
// ============================================================================
case_w = 80;      // X: front-to-back depth
case_d = 105;     // Y: left-to-right width (display width)
case_h = 85;      // Z: total height (display sits ~15mm above the ESP)
front_h = 15;     // vertical front face below the display slope
display_floor_t = 4.0;   // flat floor; deep enough to trap the M3 nuts
tilt = 12;        // display face lean-back angle
corner_r = 3;

front_top_x = -case_w/2 + (case_h - front_h) * tan(tilt);   // -25.1
slope_len   = (case_h - front_h) / cos(tilt);               // 71.6
screen_cy   = slope_len / 2;                                // slope-frame center

// ============================================================================
// DISPLAY
// ============================================================================
screen_open_w = 75;    // active area is 73.4 x 49.0
screen_open_h = 51;
standoff_h    = 4;
standoff_od   = 6;
standoff_pilot_d = 2.5;

// Landscape: the 91.97mm hole spacing is across the case width.
tft_hole_x = tft_hole_spacing_x;   // 91.97 across
tft_hole_y = tft_hole_spacing_y;   // 49.99 up/down the slope

// ============================================================================
// TOP LID / CABLE CLAMP
// ============================================================================
top_lid_t = 3;
top_lid_x0 = front_top_x;
top_lid_len = case_w/2 - front_top_x;     // 65.1

lid_len   = case_w/2 - wall - top_lid_x0;   // 62.6, fits inside top opening
lid_w     = case_d - 2*wall;                // 100
lid_z0    = case_h - 6;                     // slides under the 3mm top rim
lid_t     = 3;
back_tab_w   = 20;
back_tab_len = 4.5;
back_tab_z   = lid_z0 + lid_t/2;

// ============================================================================
// HELPERS
// ============================================================================
// Extrude a 2D (X,Z) profile along Y.  World mapping: profile-x -> world X,
// profile-y -> world Z, extrusion -> world Y.
module extrude_profile_y(width) {
    multmatrix([
        [1, 0, 0, 0],
        [0, 0, 1, 0],
        [0, 1, 0, 0],
        [0, 0, 0, 1]
    ])
        linear_extrude(height = width, center = true, convexity = 10)
            children();
}

// Local frame on the front display face:
//   +X = across the case (world Y)
//   +Y = up the slope
//   +Z = inward (into the case); outside/front is -Z
module on_slope() {
    translate([-case_w/2, 0, front_h])
        multmatrix([
            [0, sin(tilt), cos(tilt), 0],
            [1, 0, 0, 0],
            [0, cos(tilt), -sin(tilt), 0],
            [0, 0, 0, 1]
        ])
            children();
}

// ============================================================================
// SHELL
// ============================================================================
outer_profile = [
    [-case_w/2, 0],
    [ case_w/2, 0],
    [ case_w/2, case_h],
    [ front_top_x, case_h],
    [-case_w/2, front_h]
];

module case_outer() {
    extrude_profile_y(case_d)
        polygon(outer_profile);
}

module case_inner() {
    // Start the cavity at the flat floor top so the floor is thick enough to
    // trap the M3 nuts without printed standoff bosses.
    intersection() {
        extrude_profile_y(case_d - 2*wall)
            offset(delta = -wall)
                polygon(outer_profile);
        translate([-case_w, -case_d, display_floor_t])
            cube([2*case_w, 2*case_d, case_h + 5]);
    }
}

module top_opening() {
    translate([front_top_x, -(case_d/2 - wall), case_h - wall - 1])
        cube([case_w/2 - wall - front_top_x,
              case_d - 2*wall,
              wall + 2]);
}

module back_lid_boss() {
    // Solid boss below the back-wall slot for the single retaining screw.
    // Self-tapping pilot; drill to 4mm for a heat-set insert.
    translate([case_w/2 - 4, -10, case_h - 12])
        cube([4, 20, 6]);
}

module back_lid_opening() {
    // Slot through the back wall only, for the lid's back tab.
    translate([case_w/2 - wall - 0.1, -back_tab_w/2 - 2, lid_z0 - 0.1])
        cube([wall + 0.2, back_tab_w + 4, lid_t + 0.2]);
}

module back_lid_pilot() {
    translate([case_w/2 - 0.5, 0, case_h - 9.5])
        rotate([0, 90, 0])
            cylinder(d = 2.5, h = 7, center = true, $fn = 20);
}

// ============================================================================
// DISPLAY MOUNT (front screws)
// ============================================================================
module screen_opening() {
    on_slope()
        translate([0, screen_cy, wall/2])
            cube([screen_open_w, screen_open_h, wall + 0.2], center = true);
}

// The TFT sits against the inside of the front wall.  M3 screws come from
// the front, through these holes and the PCB, into standoffs in the PCB
// mounting holes.  No printed standoffs or spacers.
module display_front_mounts() {
    on_slope()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * tft_hole_x/2,
                       screen_cy + sy * tft_hole_y/2, wall/2])
                cylinder(d = m3_dia, h = wall + 0.2, center = true, $fn = 24);
}

module display_front_counterbores() {
    on_slope()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * tft_hole_x/2,
                       screen_cy + sy * tft_hole_y/2, -1.5])
                cylinder(d = m3_head_dia, h = 1.7, $fn = 24);
}

// ============================================================================
// BACK CABLE ENTRY / FLOOR CLAMP
// ============================================================================
cable_notch_w = 12;
cable_notch_h = 10;

module cable_notch_floor() {
    translate([case_w/2 - wall - 0.1, -cable_notch_w/2, 0])
        cube([wall + 0.2, cable_notch_w, cable_notch_h]);
}

module board_nut_pockets() {
    for (pos = screw_positions()) {
        translate([pos[0], pos[1], -0.1])
            nut_trap(m3_nut_dep + 0.1);
        translate([pos[0], pos[1], m3_nut_dep])
            cylinder(d = m3_dia,
                     h = display_floor_t - m3_nut_dep + 0.2, $fn = 24);
    }
}

module floor_clamp_nut_traps() {
    for (y = [-8, 8]) {
        translate([case_w/2 - 10, y, -0.1])
            nut_trap(m3_nut_dep + 0.1);
        translate([case_w/2 - 10, y, m3_nut_dep])
            cylinder(d = m3_dia,
                     h = display_floor_t - m3_nut_dep + 0.2, $fn = 24);
    }
}

module floor_cable_clamp() {
    translate([case_w/2 - 10, 0, display_floor_t])
        rotate([0, 0, 90])
            rotate([180, 0, 0])
                strain_relief_clamp(side = 0);
}

// ============================================================================
// VENTS / FEET
// ============================================================================
module side_vents() {
    for (side = [-1, 1], z = [55, 70], i = [-2 : 2])
        translate([i * 8, side * (case_d/2 - wall/2), z])
            cube([3, wall + 0.1, 14], center = true);
}

module feet() {
    for (x = [-1, 1], y = [-1, 1])
        translate([x * (case_w/2 - 8),
                   y * (case_d/2 - 15), -0.1])
            rubber_foot(6, 1);
}

// ============================================================================
// TOP LID
// ============================================================================
module top_lid() {
    // Flat plate that slides under the case's top rim from the back.  The
    // front slope stops it forward; the back tab is screwed to the case.
    difference() {
        union() {
            translate([top_lid_x0, -lid_w/2, lid_z0])
                cube([lid_len, lid_w, lid_t]);

            // back tab through the back-wall slot (overlaps the plate)
            translate([top_lid_x0 + lid_len - 1, -back_tab_w/2, lid_z0])
                cube([case_w/2 - (top_lid_x0 + lid_len - 1) + 3, back_tab_w, lid_t]);

            // back flange outside the wall; this stops the lid and carries
            // the single retaining screw into a solid boss on the case.
            translate([case_w/2, -15, case_h - 12])
                cube([3, 30, 12]);
        }

        // retaining screw hole through the back flange
        translate([case_w/2 + 1.5, 0, case_h - 9.5])
            rotate([0, 90, 0])
                cylinder(d = m3_dia, h = 3.2, center = true, $fn = 24);
        translate([case_w/2 + 3, 0, case_h - 9.5])
            rotate([0, 90, 0])
                cylinder(d = m3_head_dia, h = 1.5, center = true, $fn = 24);
    }
}

// ============================================================================

// ============================================================================
// CASE
// ============================================================================
module display_case() {
    difference() {
        union() {
            // Hollow the shell first.
            difference() {
                case_outer();
                case_inner();
                top_opening();
            }

            // Bottom features and display mount live in the same cavity.
            powerpole_enclosure(display_floor_t);
            back_lid_boss();
        }

        board_nut_pockets();
        powerpole_mating_hole(display_floor_t);
        powerpole_retention_pin(display_floor_t);
        powerpole_wire_passage(display_floor_t);

        screen_opening();
        display_front_mounts();
        display_front_counterbores();

        cable_notch_floor();
        floor_clamp_nut_traps();

        side_vents();
        feet();

        back_lid_opening();
        back_lid_pilot();
    }
}

// ============================================================================
// ASSEMBLY
// ============================================================================
module display_assembly() {
    color("LightBlue", 0.8) display_case();
    color("LightGreen", 0.85)
        translate([0, 0, 0])
            top_lid();
    color("Orange", 0.9) floor_cable_clamp();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

display_assembly();
// display_case();
// top_lid();
// floor_cable_clamp();


