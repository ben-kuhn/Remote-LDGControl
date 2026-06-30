// Remote Unit Case for LDG Controller
// Simple project box: ESP32 + breakout board, PowerPole pocket,
// cable notch with screw-down strain relief clamp on lid.

include <common.scad>

$fn = 50;

// ============================================================================
// PARAMETERS
// ============================================================================
case_w = 100;
case_d = 80;
case_h = 40;
lid_t  = 3;

corner_r     = 3;
foot_dia     = 6;
foot_depth   = 1;
notch_w      = 12;
notch_depth  = 5;

esp32_x_off   = -15;
breakout_x_off = 20;
esp32_standoff_h = 5;

// ============================================================================
// BASE
// ============================================================================
module remote_base() {
    difference() {
        union() {
            box_shell();

            translate([esp32_x_off, 0, floor_t])
                esp32_standoffs(esp32_standoff_h);

            translate([breakout_x_off, 0, floor_t])
                esp32_standoffs(esp32_standoff_h);
        }

        translate([0, case_d/2 - wall/2, case_h/2 + 5])
            rotate([90, 0, 0])
                powerpole_pocket();

        cable_notch();

        translate([-case_w/2 + wall/2, 0, case_h * 0.55])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        translate([case_w/2 - wall/2, 0, case_h * 0.55])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        for (x = [-1, 1], y = [-1, 1])
            translate([x * (case_w/2 - 10), y * (case_d/2 - 10), -0.1])
                rubber_foot(foot_dia, foot_depth);

        corner_screw_holes(case_h + 1);
    }
}

module box_shell() {
    difference() {
        linear_extrude(case_h)
            rounded_rect(case_w, case_d, corner_r);

        translate([0, 0, floor_t])
            linear_extrude(case_h - floor_t + 0.1)
                rounded_rect(case_w - 2*wall, case_d - 2*wall, corner_r - wall);
    }
}

module cable_notch() {
    translate([0, case_d/2 - wall - 0.1, case_h - notch_depth])
        cube([notch_w, wall + 0.2, notch_depth + 0.1], center = false);
}

module corner_screw_holes(h) {
    for (x = [-1, 1], y = [-1, 1])
        translate([x * (case_w/2 - 6), y * (case_d/2 - 6), -0.1])
            cylinder(d = m3_dia, h = h, $fn = 24);
}

// ============================================================================
// LID
// ============================================================================
module remote_lid() {
    difference() {
        linear_extrude(lid_t)
            rounded_rect(case_w, case_d, corner_r);

        translate([0, case_d/2 - wall - 0.1, lid_t - notch_depth])
            cube([notch_w, wall + 0.2, notch_depth + 0.1], center = false);

        corner_screw_holes(lid_t + 1);

        translate([0, case_d/2 - 15, -0.1]) {
            for (x = [-8, 8])
                translate([x, 0, 0])
                    cylinder(d = m3_dia, h = lid_t + 0.2, $fn = 24);
        }
    }
}

// ============================================================================
// STRAIN RELIEF CLAMP
// ============================================================================
module strain_relief_clamp() {
    clamp_w = 30;
    clamp_d = 18;
    clamp_h = 5;
    groove_d = 6;

    difference() {
        translate([0, case_d/2 - 15, 0])
            cube([clamp_w, clamp_d, clamp_h], center = true);

        translate([0, case_d/2 - 15, 0])
            rotate([90, 0, 0])
                cylinder(d = groove_d, h = clamp_d + 2, $fn = 24, center = true);

        translate([0, case_d/2 - 15, -0.1]) {
            for (x = [-8, 8])
                translate([x, 0, 0])
                    cylinder(d = m3_dia, h = clamp_h + 0.2, $fn = 24);
        }
    }
}

// ============================================================================
// ASSEMBLY PREVIEW
// ============================================================================
module remote_assembly() {
    color("LightBlue", 0.7) remote_base();

    color("LightGreen", 0.7)
        translate([0, 0, case_h])
            remote_lid();

    color("Orange", 0.7)
        translate([0, 0, case_h + lid_t])
            strain_relief_clamp();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

remote_base();
// remote_lid();
// strain_relief_clamp();
// remote_assembly();
