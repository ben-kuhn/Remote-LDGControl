// Remote Unit Case for LDG Controller
// Project box: ESP32 breakout (rotated 90°) on metal standoffs with floor nut traps,
// PowerPole in back-right corner (rotated 90°, mating face on right wall),
// cable notch with strain relief on front.
// The PowerPole pair is retained by a vertical 3/32in roll pin dropped through
// the pocket ceiling into the slot between the red and black housings.

include <common.scad>

$fn = 50;

// ============================================================================
// PARAMETERS
// ============================================================================
case_w = 80;
case_d = 105;
case_h = 30;
lid_t  = 3;
floor_t = 4;

corner_r     = 3;
foot_dia     = 6;
foot_depth   = 1;
foot_inset_x = 8;   // keep feet clear of the front-row M3 nut traps
foot_inset_y = 15;
notch_w      = 12;
notch_depth  = 5;

screw_inset    = 8;

esp32_x_off       = 0;
esp32_y_off       = 8;  // Shift board forward to clear PP pocket

pp_conn_length  = 30;
pp_wall_t       = 2;
pp_ceiling_t    = 2;
// Anderson drawing 1-9597S1: the roll-pin slot is 0.39in (9.9mm) back from
// the mating face, on the seam between the two housings.  A 3/32in (2.38mm)
// Powerpole roll pin is the intended pin; 2.3mm gives it a slight press fit
// in the printed ceiling while the connector's own 2.6mm slot locates it.
pp_pin_from_face = 9.9;
pp_pin_d         = 2.3;

clamp_w = 30;
clamp_d = 20;
clamp_h = 10;
groove_d = 6;

// ============================================================================
// HELPERS
// ============================================================================
pp_pocket_x = case_w/2 - pp_conn_length/2;
pp_pocket_y = -(case_d/2 - wall - pp_w/2);

// Single source of truth for the four vertical screw stacks: board nut traps
// in the floor, case clearance holes, and lid holes must all use this pattern.
function screw_positions() = [
    [esp32_x_off - nodemcu_hole_y/2, esp32_y_off - nodemcu_hole_x/2],
    [esp32_x_off + nodemcu_hole_y/2, esp32_y_off - nodemcu_hole_x/2],
    [esp32_x_off - nodemcu_hole_y/2, esp32_y_off + nodemcu_hole_x/2],
    [esp32_x_off + nodemcu_hole_y/2, esp32_y_off + nodemcu_hole_x/2],
];

// ============================================================================
// BASE
// ============================================================================
module remote_base(cable_side = 1) {
    difference() {
        union() {
            box_shell();
            powerpole_enclosure();
        }

        esp32_nut_traps();
        powerpole_mating_hole();
        powerpole_retention_pin();
        powerpole_wire_passage();
        cable_notch(cable_side);

        translate([-case_w/2 + wall/2, esp32_y_off, case_h * 0.65])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        translate([case_w/2 - wall/2, esp32_y_off, case_h * 0.65])
            rotate([0, 90, 0])
                vent_slots(3, 5, 15, 3);

        for (x = [-1, 1], y = [-1, 1])
            translate([x * (case_w/2 - foot_inset_x),
                       y * (case_d/2 - foot_inset_y), -0.1])
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
    // Same pattern as the lid and case holes, so the whole screw stack is
    // guaranteed to line up from top to bottom.
    for (pos = screw_positions())
        translate([pos[0], pos[1], 0])
            embedded_nut_trap();
}

module embedded_nut_trap() {
    screw_clearance_h = floor_t - m3_nut_dep;
    translate([0, 0, -0.1])
        nut_trap(m3_nut_dep + 0.1);
    translate([0, 0, m3_nut_dep])
        cylinder(d = m3_dia, h = screw_clearance_h + 0.2, $fn = 24);
}

module cable_notch(side = 1) {
    y0 = side > 0 ? case_d/2 - wall - 0.1 : -case_d/2 - 0.1;
    translate([-notch_w/2, y0, case_h - notch_depth])
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

    // Left wall (-X side).  Start below the floor so the enclosure fuses
    // with the case floor instead of only touching it.
    translate([pp_left - pp_wall_t, pp_back, floor_t - 0.5])
        cube([pp_wall_t, pp_w, pp_h + pp_ceiling_t + 0.5]);

    // Front wall (+Y side)
    translate([pp_left - pp_wall_t, pp_front, floor_t - 0.5])
        cube([pp_conn_length + pp_wall_t, pp_wall_t, pp_h + pp_ceiling_t + 0.5]);

    // Ceiling
    translate([pp_left - pp_wall_t, pp_back, floor_t + pp_h])
        cube([pp_conn_length + pp_wall_t, pp_w, pp_ceiling_t]);
}

module powerpole_mating_hole() {
    translate([case_w/2 - wall/2, pp_pocket_y, floor_t + pp_h/2])
        cube([wall + 0.2, pp_w + 2*tol, pp_h + 2*tol], center = true);
}

module powerpole_retention_pin() {
    // Vertical 3/32in roll-pin hole through the pocket ceiling.  It lands in
    // the round slot formed where the + and - housings dovetail together,
    // 9.9mm behind the mating face, and locks the pair in the pocket.
    pin_x = case_w/2 - pp_pin_from_face;
    translate([pin_x, pp_pocket_y, floor_t + pp_h - 0.1])
        cylinder(d = pp_pin_d, h = pp_ceiling_t + 0.2, $fn = 24);
}

module powerpole_wire_passage() {
    pp_left = pp_pocket_x - pp_conn_length/2;
    wire_slot_w = 14;
    wire_slot_h = 8;
    translate([pp_left - pp_wall_t - 0.1, pp_pocket_y - wire_slot_w/2, floor_t])
        cube([pp_wall_t + 0.2, wire_slot_w, wire_slot_h]);
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
module strain_relief_clamp(side = 1) {
    clamp_y_center = case_d/2 - wall - notch_w/2;
    foot_w = 6;
    foot_d = 12;
    foot_t = 3;
    arch_span = 16;
    arch_height = 2.5;
    arch_thickness = 2;
    screw_pad_d = 6;

    translate([0, side * clamp_y_center, 0]) {
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
