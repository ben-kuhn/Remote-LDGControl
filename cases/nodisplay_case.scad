// Remote Unit Case for LDG Controller
// Project box: ESP32 on floor nut traps, PowerPole floor pocket with screw retention,
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

screw_inset    = 8;

pp_conn_length  = 30;
pp_seat_depth   = 2;

// ============================================================================
// HELPERS
// ============================================================================
pp_pocket_x = case_w/2 - wall - pp_w/2 - 2;
pp_pocket_y = case_d/2 - pp_conn_length/2;

function screw_positions() = [
    [-(case_w/2 - screw_inset), -(case_d/2 - screw_inset)],
    [-(case_w/2 - screw_inset),  (case_d/2 - screw_inset)],
    [ (case_w/2 - screw_inset), -(case_d/2 - screw_inset)],
    [pp_pocket_x, pp_pocket_y]
];

// ============================================================================
// BASE
// ============================================================================
module remote_base() {
    difference() {
        box_shell();

        esp32_nut_traps();
        powerpole_floor_pocket();
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
        translate([pos[0], pos[1], -0.1])
            nut_trap(m3_nut_dep + 0.1);
}

// ============================================================================
// POWERPOLE FLOOR POCKET
// ============================================================================
module powerpole_floor_pocket() {
    // Connector flat on floor, long axis along Y, mating face at +Y (front)
    // Shallow recess in floor to locate it, hole through front wall for mating face
    
    // Shallow recess in floor (pp_seat_depth deep)
    translate([pp_pocket_x, pp_pocket_y, floor_t - pp_seat_depth/2])
        cube([pp_w + 2*tol, pp_conn_length + 2*tol, pp_seat_depth + 0.1], center = true);
    
    // Hole through front wall for mating face (connector sits on floor, so center at floor_t + pp_h/2)
    translate([pp_pocket_x, case_d/2 - wall/2, floor_t + pp_h/2])
        cube([pp_w + 2*tol, wall + 0.2, pp_h + 2*tol], center = true);
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
