// Remote Unit Case for LDG Controller
// Project box: ESP32 + breakout on floor nut traps, PowerPole side-wall pocket,
// cable notch with screw-down strain relief clamp on lid.

include <common.scad>

$fn = 50;

// ============================================================================
// PARAMETERS
// ============================================================================
case_w = 120;
case_d = 100;
case_h = 40;
lid_t  = 3;
floor_t = 4;

corner_r     = 3;
foot_dia     = 6;
foot_depth   = 1;
notch_w      = 12;
notch_depth  = 5;

esp32_x_off    = -20;

corner_post_d  = 8;
screw_inset    = 8;

pp_boss_depth  = 6;
pp_ear_w       = 4;
pp_pin_from_wall = 5;

// ============================================================================
// HELPERS
// ============================================================================
function screw_positions() = [
    for (x = [-1, 1], y = [-1, 1])
        [x * (case_w/2 - screw_inset), y * (case_d/2 - screw_inset)]
];

// ============================================================================
// BASE
// ============================================================================
module remote_base() {
    difference() {
        union() {
            box_shell();
            corner_posts();
            powerpole_pocket_boss();
        }

        esp32_nut_traps();
        powerpole_side_pocket();
        cable_notch();

        translate([-case_w/2 + wall/2, 0, case_h * 0.65])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        translate([case_w/2 - wall/2, 0, case_h * 0.65])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        for (x = [-1, 1], y = [-1, 1])
            translate([x * (case_w/2 - 10), y * (case_d/2 - 10), -0.1])
                rubber_foot(foot_dia, foot_depth);

        corner_screw_holes(case_h + 1);
        lid_nut_traps();
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

module corner_posts() {
    post_h = case_h - floor_t - 1;
    for (pos = screw_positions())
        translate([pos[0], pos[1], floor_t])
            cylinder(d = corner_post_d, h = post_h, $fn = 32);
}

module esp32_nut_traps() {
    for (p = [[-nodemcu_hole_x/2, -nodemcu_hole_y/2],
               [ nodemcu_hole_x/2, -nodemcu_hole_y/2],
               [-nodemcu_hole_x/2,  nodemcu_hole_y/2],
               [ nodemcu_hole_x/2,  nodemcu_hole_y/2]])
        translate([esp32_x_off + p[0], p[1], 0])
            embedded_nut_trap();
}

module embedded_nut_trap() {
    screw_clearance_h = floor_t - m3_nut_dep;
    translate([0, 0, -0.1])
        nut_trap(m3_nut_dep + 0.1);
    translate([0, 0, m3_nut_dep])
        cylinder(d = m3_dia, h = screw_clearance_h + 0.2, $fn = 24);
}

module cable_notch() {
    translate([0, case_d/2 - wall - 0.1, case_h - notch_depth])
        cube([notch_w, wall + 0.2, notch_depth + 0.1], center = false);
}

module corner_screw_holes(h) {
    for (pos = screw_positions())
        translate([pos[0], pos[1], -0.1])
            cylinder(d = m3_dia, h = h, $fn = 24);
}

module lid_nut_traps() {
    for (pos = screw_positions())
        translate([pos[0], pos[1], case_h - m3_nut_dep])
            nut_trap(m3_nut_dep + 0.1);
}

// ============================================================================
// POWERPOLE SIDE-WALL POCKET (right wall, connector rotated 90°)
// ============================================================================
module powerpole_pocket_boss() {
    conn_w = pp_h + 2*tol;
    conn_h = pp_w + 2*tol;
    z_center = floor_t + conn_h/2 + 2;

    translate([case_w/2 - wall - pp_boss_depth/2, 0, z_center])
        cube([pp_boss_depth, conn_w + pp_ear_w*2, conn_h], center = true);
}

module powerpole_side_pocket() {
    conn_w = pp_h + 2*tol;
    conn_h = pp_w + 2*tol;
    pin_d = 2;
    z_center = floor_t + conn_h/2 + 2;

    translate([case_w/2 - wall/2, 0, z_center])
        cube([wall + 0.2, conn_w, conn_h], center = true);

    translate([case_w/2 - pp_pin_from_wall, 0, z_center])
        rotate([90, 0, 0])
            cylinder(d = pin_d, h = conn_w + pp_ear_w*2 + 2, $fn = 16, center = true);
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

remote_assembly();
// remote_base();
// remote_lid();
// strain_relief_clamp();
