// Display Unit Case for LDG Controller
// Trapezoid side profile: tall back wall, short front wall.
// ESP32 + breakout on the floor. Tilted front panel is the removable lid
// with display screen cutout. PowerPole and signal cable at floor level on back.

include <common.scad>

$fn = 50;

// ============================================================================
// PARAMETERS
// ============================================================================
disp_w       = 130;
disp_d       = 120;
disp_h_back  = 60;
disp_h_front = 20;
panel_t      = 3;
corner_r     = 3;
foot_dia     = 6;
foot_depth   = 1;

cable_hole_d = 10;
clamp_z_center = floor_t + 2.5;

panel_length = sqrt(pow(disp_d, 2) + pow(disp_h_back - disp_h_front, 2));
tilt_angle   = atan((disp_h_back - disp_h_front) / disp_d);

esp32_x_off    = -20;
breakout_x_off = 20;
esp32_standoff_h = 5;

// ============================================================================
// BASE
// ============================================================================
module display_base() {
    difference() {
        union() {
            trapezoid_shell();

            translate([esp32_x_off, 0, floor_t])
                esp32_standoffs(esp32_standoff_h);

            translate([breakout_x_off, 0, floor_t])
                esp32_standoffs(esp32_standoff_h);
        }

        trapezoid_interior();

        translate([-20, -disp_d/2 + wall/2, floor_t + pp_h/2])
            rotate([90, 0, 0])
                powerpole_pocket();

        translate([20, -disp_d/2 + wall/2, floor_t + cable_hole_d/2])
            rotate([90, 0, 0])
                cylinder(d = cable_hole_d, h = wall + 0.2, $fn = 32);

        translate([-disp_w/2 + wall/2, 0, disp_h_back * 0.4])
            rotate([0, 90, 0])
                vent_slots(4, 5, 15, 3);

        translate([disp_w/2 - wall/2, 0, disp_h_back * 0.4])
            rotate([0, 90, 0])
                vent_slots(4, 5, 15, 3);

        for (x = [-1, 1], y = [-1, 1])
            translate([x * (disp_w/2 - 12), y * (disp_d/2 - 12), -0.1])
                rubber_foot(foot_dia, foot_depth);

        back_screw_holes();
        clamp_mount_holes();
    }
}

module trapezoid_shell() {
    linear_extrude(height = disp_w, center = true, convexity = 10)
        polygon([
            [-disp_d/2, 0],
            [ disp_d/2, 0],
            [ disp_d/2, disp_h_front],
            [-disp_d/2, disp_h_back]
        ]);
}

module trapezoid_interior() {
    translate([0, 0, floor_t])
        linear_extrude(height = disp_w - 2*wall, center = true, convexity = 10)
            polygon([
                [-(disp_d - 2*wall)/2, 0],
                [ (disp_d - 2*wall)/2, 0],
                [ (disp_d - 2*wall)/2, disp_h_front - wall],
                [-(disp_d - 2*wall)/2, disp_h_back  - wall]
            ]);
}

module back_screw_holes() {
    for (x = [-disp_w/4, disp_w/4])
        translate([x, -disp_d/2 + wall/2, disp_h_back - 8])
            rotate([90, 0, 0])
                cylinder(d = m3_dia, h = wall + 1, $fn = 24);
}

module clamp_mount_holes() {
    for (x = [20 - 8, 20 + 8])
        translate([x, -disp_d/2 - 0.1, clamp_z_center])
            rotate([90, 0, 0])
                cylinder(d = m3_dia, h = wall + 0.2, $fn = 24);
}

// ============================================================================
// STRAIN RELIEF CLAMP (same as remote case)
// ============================================================================
module strain_relief_clamp() {
    clamp_w = 30;
    clamp_d = 18;
    clamp_h = 5;
    groove_d = 6;

    difference() {
        cube([clamp_w, clamp_d, clamp_h], center = true);

        rotate([90, 0, 0])
            cylinder(d = groove_d, h = clamp_d + 2, $fn = 24, center = true);

        translate([0, 0, -0.1]) {
            for (x = [-8, 8])
                translate([x, 0, 0])
                    cylinder(d = m3_dia, h = clamp_h + 0.2, $fn = 24);
        }
    }
}

// ============================================================================
// TILTED PANEL (REMOVABLE LID)
// ============================================================================
module display_panel() {
    difference() {
        panel_body();

        translate([0, 0, -panel_t/2 - 0.1])
            display_screen_cutout();

        translate([0, 0, panel_t/2 - 1.5])
            display_pcb_recess(2);

        panel_screw_holes();
    }

    translate([0, 0, panel_t/2])
        display_standoffs(3);
}

module panel_body() {
    translate([0, -disp_d/2, disp_h_back])
        rotate([tilt_angle, 0, 0])
            translate([0, panel_length/2, 0])
                cube([disp_w, panel_length, panel_t], center = true);
}

module panel_screw_holes() {
    translate([0, -disp_d/2, disp_h_back])
        rotate([tilt_angle, 0, 0])
            for (x = [-disp_w/4, disp_w/4])
                translate([x, -panel_length/2 + 8, -panel_t/2 - 0.1])
                    cylinder(d = m3_dia, h = panel_t + 0.2, $fn = 24);
}

// ============================================================================
// ASSEMBLY PREVIEW
// ============================================================================
module display_assembly() {
    color("LightBlue", 0.7) display_base();
    color("LightGreen", 0.7) display_panel();

    color("Orange", 0.7)
        translate([20, -disp_d/2 - 9, clamp_z_center])
            strain_relief_clamp();
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

display_base();
// display_panel();
// strain_relief_clamp();
// display_assembly();
