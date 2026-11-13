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

lid_screw_positions = [
    [-10,  46.5],
    [-10, -46.5],
    [ 20,  46.5],
    [ 20, -46.5],
    [ 34.0,  20],
    [ 34.0, -20],
];

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

module top_lid_bosses() {
    for (p = lid_screw_positions)
        translate([p[0], p[1], case_h - 10])
            cylinder(d = 10, h = 10, $fn = 32);
}

module lid_nut_traps() {
    // Nuts slide in from the side through a slot, rest on a closed pocket
    // floor, and are captured under the top shoulder.  The screw passes
    // through the clearance hole above and pulls the lid down onto the boss.
    for (p = lid_screw_positions) {
        x = p[0];
        y = p[1];
        side = abs(y) > 40;   // side-wall bosses vs rear-wall bosses

        // screw clearance through the 2.5mm top shoulder
        translate([x, y, case_h - 2.5])
            cylinder(d = m3_dia, h = 2.6, $fn = 24);

        // closed-bottom hex pocket; nut rests on the floor at case_h-5
        translate([x, y, case_h - 5])
            nut_trap(m3_nut_dep + 0.1);

        // side-loading slot from the inside of the case
        if (side) {
            translate([x - 3, y - sign(y)*5, case_h - 5])
                cube([6, 5, 2.7]);
        } else {
            translate([x - 5, y - 3, case_h - 5])
                cube([5, 6, 2.7]);
        }
    }
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
    difference() {
        translate([top_lid_x0, 0, case_h])
            linear_extrude(height = top_lid_t)
                translate([top_lid_len/2, 0])
                    rounded_rect(top_lid_len, case_d, corner_r);

        for (p = lid_screw_positions)
            translate([p[0], p[1], case_h - 0.1])
                cylinder(d = m3_dia, h = top_lid_t + 0.2, $fn = 24);

        for (p = lid_screw_positions)
            translate([p[0], p[1], case_h + top_lid_t - 1.5])
                cylinder(d = m3_head_dia, h = 1.7, $fn = 24);
    }

    // locating lip
    translate([top_lid_x0 + 1, 0, case_h - 2])
        linear_extrude(height = 2)
            translate([(top_lid_len - 2)/2, 0])
                rounded_rect(top_lid_len - 6, case_d - 2*wall - 4, 1);
}

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
            top_lid_bosses();
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

        lid_nut_traps();
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
