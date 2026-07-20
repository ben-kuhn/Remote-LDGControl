// Remote Unit Case for LDG Controller
// Project box: ESP32 on standoffs (rotated 90°), PowerPole in back-right corner
// (rotated 90°, mating face on right wall), cable notch with strain relief on front.

include <common.scad>

$fn = 50;

// ============================================================================
// PARAMETERS
// ============================================================================
case_w = 80;
case_d = 85;
case_h = 30;
lid_t  = 3;
floor_t = 4;

corner_r     = 3;
foot_dia     = 6;
foot_depth   = 1;
notch_w      = 12;
notch_depth  = 5;

screw_inset    = 8;

esp32_x_off       = 0;
esp32_standoff_h  = 10;

pp_conn_length  = 30;
pp_wall_t       = 2;
pp_ceiling_t    = 2;

clamp_w = 30;
clamp_d = 20;
clamp_h = 10;
groove_d = 6;

// ============================================================================
// HELPERS
// ============================================================================
// PP rotated 90°: long axis (pp_conn_length=30) along X, short axis (pp_w=16.8) along Y
// Mating face on right wall (+X), back against back wall (-Y)
pp_pocket_x = case_w/2 - pp_conn_length/2;
pp_pocket_y = -(case_d/2 - wall - pp_w/2);

function screw_positions() = [
    [-(case_w/2 - screw_inset), -(case_d/2 - screw_inset)],
    [-(case_w/2 - screw_inset),  (case_d/2 - screw_inset)],
    [ (case_w/2 - screw_inset),  (case_d/2 - screw_inset)],
];

// ============================================================================
// BASE
// ============================================================================
module remote_base() {
    difference() {
        union() {
            box_shell();
            powerpole_enclosure();
            esp32_standoffs();
        }

        esp32_nut_traps();
        powerpole_mating_hole();
        powerpole_retention_screw();
        powerpole_wire_exit();
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

module esp32_standoffs() {
    for (p = [[-nodemcu_hole_y/2, -nodemcu_hole_x/2],
               [-nodemcu_hole_y/2,  nodemcu_hole_x/2],
               [ nodemcu_hole_y/2,  nodemcu_hole_x/2]])
        translate([esp32_x_off + p[0], p[1], floor_t])
            standoff(6, esp32_standoff_h);
}

module esp32_nut_traps() {
    for (p = [[-nodemcu_hole_y/2, -nodemcu_hole_x/2],
               [-nodemcu_hole_y/2,  nodemcu_hole_x/2],
               [ nodemcu_hole_y/2,  nodemcu_hole_x/2]])
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
    translate([-notch_w/2, case_d/2 - wall - 0.1, case_h - notch_depth])
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
// POWERPOLE ENCLOSURE (rotated 90°: long axis along X, mating on right wall)
// ============================================================================
module powerpole_enclosure() {
    pp_left  = pp_pocket_x - pp_conn_length/2;
    pp_back  = pp_pocket_y - pp_w/2;
    pp_front = pp_pocket_y + pp_w/2;

    // Left wall (-X side)
    translate([pp_left - pp_wall_t, pp_back, floor_t])
        cube([pp_wall_t, pp_w, pp_h + pp_ceiling_t]);

    // Front wall (+Y side)
    translate([pp_left - pp_wall_t, pp_front, floor_t])
        cube([pp_conn_length + pp_wall_t, pp_wall_t, pp_h + pp_ceiling_t]);

    // Ceiling
    translate([pp_left - pp_wall_t, pp_back, floor_t + pp_h])
        cube([pp_conn_length + pp_wall_t, pp_w, pp_ceiling_t]);
}

module powerpole_mating_hole() {
    translate([case_w/2 - wall/2, pp_pocket_y, floor_t + pp_h/2])
        cube([wall + 0.2, pp_w + 2*tol, pp_h + 2*tol], center = true);
}

module powerpole_retention_screw() {
    translate([pp_pocket_x, -case_d/2 + wall/2, floor_t + pp_h/2])
        rotate([90, 0, 0])
            cylinder(d = m3_dia, h = wall + 0.2, $fn = 24, center = true);
}

module powerpole_wire_exit() {
    wire_slot_w = 20;
    wire_slot_h = 8;
    translate([pp_pocket_x - wire_slot_w/2, -case_d/2 - 0.1, floor_t])
        cube([wire_slot_w, wall + 0.2, wire_slot_h]);
}

// ============================================================================
// LID
// ============================================================================
module remote_lid() {
    clamp_y_center = case_d/2 - wall - notch_w/2;

    difference() {
        linear_extrude(lid_t)
            rounded_rect(case_w, case_d, corner_r);

        translate([-notch_w/2, case_d/2 - wall - 0.1, -0.1])
            cube([notch_w, wall + 0.2, lid_t + 0.2], center = false);

        corner_screw_holes(lid_t + 1);

        translate([0, clamp_y_center, -0.1]) {
            for (x = [-8, 8])
                translate([x, 0, 0])
                    cylinder(d = m3_dia, h = lid_t + 0.2, $fn = 24);
        }

        translate([0, clamp_y_center, lid_t - m3_nut_dep]) {
            for (x = [-8, 8])
                translate([x, 0, 0])
                    nut_trap(m3_nut_dep + 0.1);
        }
    }
}

// ============================================================================
// STRAIN RELIEF CLAMP
// ============================================================================
module strain_relief_clamp() {
    clamp_y_center = case_d/2 - wall - notch_w/2;
    foot_w = 6;
    foot_d = 12;
    foot_t = 3;
    arch_span = 16;
    arch_height = 2.5;
    arch_thickness = 2;
    screw_pad_d = 6;

    translate([0, clamp_y_center, 0]) {
        difference() {
            union() {
                translate([-arch_span/2, 0, -foot_t/2]) {
                    cube([foot_w, foot_d, foot_t], center = true);
                    translate([0, 0, -foot_t/2])
                        cylinder(d = screw_pad_d, h = foot_t, $fn = 24);
                }

                translate([arch_span/2, 0, -foot_t/2]) {
                    cube([foot_w, foot_d, foot_t], center = true);
                    translate([0, 0, -foot_t/2])
                        cylinder(d = screw_pad_d, h = foot_t, $fn = 24);
                }

                for (angle = [0:10:180]) {
                    x = -arch_span/2 + (arch_span * angle / 180);
                    z = -foot_t - arch_height * sin(angle);
                    translate([x, 0, z])
                        rotate([90, 0, 0])
                            cylinder(d = arch_thickness, h = foot_d, center = true);
                }
            }

            for (x = [-arch_span/2, arch_span/2])
                translate([x, 0, -foot_t - arch_height - arch_thickness - 0.1])
                    cylinder(d = m3_dia, h = foot_t*2 + arch_height + arch_thickness + 0.2, $fn = 24);
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
        translate([0, 0, case_h - clamp_h/2 - 0.1])
            strain_relief_clamp();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

remote_assembly();
// remote_base();
// remote_lid();
// strain_relief_clamp();
