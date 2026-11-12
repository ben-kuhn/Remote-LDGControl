// Display Unit Case for LDG Controller
//
// Same 80 x 105 footprint as the remote/no-display case.  The remote base is
// reused unchanged except for the cable notch/clamp, which moves to the back
// wall.  A taller front tower carries the TFT in portrait orientation on a
// front face tilted back 12 degrees.
//
// Back:  PowerPole (back-right) + tuner cable notch/clamp.
// Front: portrait 3.5in TFT, facing the user.
//
// The display head is a separate print.  It bolts to the remote base through
// the same four M3 screw positions, using screws inserted from underneath the
// base into nut traps in the head's adapter plate.

include <common.scad>
use <nodisplay_case.scad>

$fn = 50;

// ============================================================================
// REMOTE BASE (same footprint; cable exits the back)
// ============================================================================
base_w = 80;
base_d = 105;
base_h = 30;
cable_side = -1;        // -1 = back wall, +1 = front (remote case default)

notch_w        = 12;
clamp_y_center = base_d/2 - wall - notch_w/2;   // 44
clamp_h        = 10;

// ============================================================================
// DISPLAY HEAD (same footprint as the base)
// ============================================================================
head_w        = 80;
head_d        = 105;
head_bottom_t = 4;
head_h        = 156;
head_shell_z  = base_h + head_bottom_t;   // 34mm

tilt     = 12;          // degrees back from vertical
corner_r = 3;

// Portrait TFT35-SPI V2.1: PCB 56 wide x 98 tall, active area 49 x 73.4.
panel_w = 76;
panel_h = 120;
panel_t = 3;

// Opening through the sloped front, sized to clear the 56 x 98 PCB.
front_open_w = 58;
front_open_h = 100;

// Portrait screen aperture (49 x 73.4 active area + bezel clearance).
screen_open_w = 52;
screen_open_h = 76;

panel_screw_inset_x = 5;   // panel screws at x = +/-33
panel_screw_inset_y = 5;   // panel screws at y = panel_cy +/-55

standoff_h       = 4;
standoff_od      = 6;
standoff_pilot_d = 2.5;

// ============================================================================
// DERIVED
// ============================================================================
slope_len = (head_h - head_shell_z) / cos(tilt);
panel_y0  = (slope_len - panel_h) / 2;
panel_cy  = panel_y0 + panel_h/2;
panel_sx  = panel_w/2 - panel_screw_inset_x;   // 33
panel_sy  = panel_h/2 - panel_screw_inset_y;   // 55

head_shell_h = head_h - head_shell_z;          // 122
slope_top_y  = head_d/2 - head_shell_h * tan(tilt);

// TFT is portrait: the long hole spacing is vertical, short spacing across.
tft_hole_x = tft_hole_spacing_y;   // 49.99 across
tft_hole_y = tft_hole_spacing_x;   // 91.97 vertical

// ============================================================================
// HELPERS
// ============================================================================
// Extrude a 2D (Y,Z) profile along X.  World mapping: profile-x -> world Y,
// profile-y -> world Z, extrusion -> world X.
module extrude_profile_x(width) {
    multmatrix([
        [0, 0, 1, 0],
        [1, 0, 0, 0],
        [0, 1, 0, 0],
        [0, 0, 0, 1]
    ])
        linear_extrude(height = width, center = true, convexity = 10)
            children();
}

// Local frame on the sloped front panel face:
//   +X = across the panel
//   +Y = up the slope
//   +Z = inward (into the case); the outside/front is -Z
module on_slope() {
    translate([0, head_d/2, head_shell_z])
        rotate([90 + tilt, 0, 0])
            children();
}

// ============================================================================
// ADAPTER PLATE / HEAD BOTTOM
// ============================================================================
module head_bottom_plate() {
    difference() {
        translate([0, 0, base_h])
            linear_extrude(height = head_bottom_t + 0.5)
                rounded_rect(head_w, head_d, corner_r);

        // Screws come up from underneath the remote base into these nuts.
        for (pos = screw_positions())
            translate([pos[0], pos[1], base_h - 0.1])
                nut_trap(m3_nut_dep + 0.1);

        // Clearance above the base-mount nuts, through the plate and shell
        // floor, so a 35mm screw can protrude instead of bottoming out.
        for (pos = screw_positions())
            translate([pos[0], pos[1], base_h + m3_nut_dep - 0.1])
                cylinder(d = m3_dia, h = head_bottom_t + wall + 0.6, $fn = 24);

        // Cable notch, aligned with the remote base's back-wall notch.
        translate([-notch_w/2,
                   cable_side > 0 ? base_d/2 - wall - 0.1 : -base_d/2 - 0.1,
                   base_h - 0.1])
            cube([notch_w, wall + 0.2, head_bottom_t + 0.2]);

        // Strain-relief clamp holes.  The clamp hangs under this plate.
        for (x = [-8, 8]) {
            translate([x, cable_side * clamp_y_center, base_h - 0.1])
                cylinder(d = m3_dia, h = head_bottom_t + 0.2, $fn = 24);
            translate([x, cable_side * clamp_y_center,
                       base_h + head_bottom_t - m3_nut_dep])
                nut_trap(m3_nut_dep + 0.1);
        }

        // Display ribbon cable pass-through into the remote base.
        translate([0, 8, base_h - 0.1])
            cylinder(d = 12, h = head_bottom_t + wall + 0.2, $fn = 32);
    }
}

// ============================================================================
// HEAD SHELL
// ============================================================================
module head_shell_outer() {
    intersection() {
        translate([0, 0, head_shell_z])
            extrude_profile_x(head_w)
                polygon([
                    [-head_d/2, 0],
                    [ head_d/2, 0],
                    [ slope_top_y, head_shell_h],
                    [-head_d/2, head_shell_h]
                ]);
        translate([0, 0, head_shell_z])
            linear_extrude(height = head_shell_h)
                rounded_rect(head_w, head_d, corner_r);
    }
}

module head_shell_inner() {
    intersection() {
        translate([0, 0, head_shell_z])
            extrude_profile_x(head_w - 2*wall)
                offset(delta = -wall)
                    polygon([
                        [-head_d/2, 0],
                        [ head_d/2, 0],
                        [ slope_top_y, head_shell_h],
                        [-head_d/2, head_shell_h]
                    ]);
        translate([0, 0, head_shell_z])
            linear_extrude(height = head_shell_h)
                rounded_rect(head_w - 2*wall, head_d - 2*wall,
                             max(corner_r - wall, 0.1));
    }
}

module panel_frame() {
    // Internal reinforcing frame around the display opening.  It ties all
    // four panel-screw bosses together and fuses them to the slope wall.
    on_slope()
        translate([0, panel_cy, wall - 0.5 + 4.8/2])
            difference() {
                cube([panel_w, panel_h, 4.8], center = true);
                cube([front_open_w, front_open_h, 10], center = true);
            }
}

module panel_boss_holes() {
    on_slope()
        for (sx = [-1, 1], sy = [-1, 1]) {
            // clearance through the slope wall and frame front
            translate([sx * panel_sx, panel_cy + sy * panel_sy, -0.1])
                cylinder(d = m3_dia, h = 4.2, $fn = 24);
            // nut pocket, open to the inside of the head
            translate([sx * panel_sx, panel_cy + sy * panel_sy, wall + 1.5])
                nut_trap(m3_nut_dep + 0.5);
        }
}

module front_opening() {
    on_slope()
        translate([0, panel_cy, -0.1])
            cube([front_open_w, front_open_h, 20]);
}

module head_shell() {
    difference() {
        union() {
            // Hollow the tower first, then add the panel bosses so the
            // interior subtraction cannot eat them.
            difference() {
                head_shell_outer();
                head_shell_inner();
            }
            panel_frame();
        }
        front_opening();
        panel_boss_holes();

        // Ventilation through the side walls.
        for (side = [-1, 1], z = [52, 92, 130])
            translate([side * (head_w/2 - wall/2), 0, z])
                rotate([0, 90, 0])
                    vent_slots(3, 5, 14, 3);
    }
}

// ============================================================================
// DISPLAY PANEL (removable; TFT mounts to its back, portrait)
// ============================================================================
module display_panel() {
    difference() {
        // Panel body sits proud of the slope, on the outside (-Z).
        on_slope()
            translate([0, panel_cy, -panel_t/2])
                cube([panel_w, panel_h, panel_t], center = true);

        // Screen aperture.
        on_slope()
            translate([0, panel_cy, -panel_t/2])
                cube([screen_open_w, screen_open_h, panel_t + 0.2], center = true);

        // Mounting screws through the panel into the slope bosses.
        on_slope()
            for (sx = [-1, 1], sy = [-1, 1]) {
                translate([sx * panel_sx, panel_cy + sy * panel_sy, -panel_t/2])
                    cylinder(d = m3_dia, h = panel_t + 0.2, center = true, $fn = 24);
                // shallow counterbore in the panel front
                translate([sx * panel_sx, panel_cy + sy * panel_sy, -panel_t])
                    cylinder(d = m3_head_dia, h = 1.7, $fn = 24);
            }
    }

    // TFT PCB standoffs on the inside face (+Z).  Start 0.5mm inside the
    // panel so the standoffs are definitely fused to it.
    on_slope()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * tft_hole_x/2,
                       panel_cy + sy * tft_hole_y/2, (standoff_h - 0.5)/2])
                difference() {
                    cylinder(d = standoff_od, h = standoff_h + 0.5, center = true, $fn = 32);
                    translate([0, 0, -(standoff_h + 0.5)/2 - 0.1])
                        cylinder(d = standoff_pilot_d, h = standoff_h + 0.5 + 0.2, $fn = 20);
                }
}

// ============================================================================
// PRINTABLE HEAD (bottom plate + shell as one part)
// ============================================================================
module clamp_nut_access() {
    // The clamp nuts live in the adapter plate, but the shell floor sits on
    // top of it.  Open the hex pockets up through that floor so the nuts can
    // be inserted from inside the head.
    for (x = [-8, 8])
        translate([x, cable_side * clamp_y_center,
                   base_h + head_bottom_t - m3_nut_dep])
            nut_trap(m3_nut_dep + wall + 0.5);
}

module display_head() {
    difference() {
        union() {
            head_bottom_plate();
            head_shell();
        }
        clamp_nut_access();
    }
}

// ============================================================================
// ASSEMBLY PREVIEW
// ============================================================================
module display_assembly() {
    color("LightBlue", 0.75) remote_base(cable_side);
    color("LightGreen", 0.75) display_head();
    color("LightYellow", 0.9) display_panel();

    color("Orange", 0.8)
        translate([0, 0, base_h - clamp_h/2 - 0.1])
            strain_relief_clamp(cable_side);
}

// ============================================================================
// RENDER SELECTION
// ============================================================================

display_assembly();
// display_head();
// display_panel();
// remote_base(cable_side);
