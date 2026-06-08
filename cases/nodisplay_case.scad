// Non-Display Unit Case for LDG Controller
// Simple box with removable lid
// Use OpenSCAD to render and export STL for 3D printing

include <common.scad>

// ============================================================================
// NODISPLAY CASE PARAMETERS
// ============================================================================
$fn = 50;

// ============================================================================
// BASE (BOTTOM HALF)
// ============================================================================
module nodisplay_base() {
    difference() {
        union() {
            // Box shell
            box_shell();
            
            // ESP32 standoffs inside
            translate([0, 0, floor_t]) {
                esp32_standoffs(5);
            }
        }
        
        // Hollow interior
        translate([0, 0, floor_t]) {
            cube([nodisp_case_w - 2*wall, nodisp_case_d - 2*wall, nodisp_case_h - floor_t], 
                 center = true);
        }
        
        // Nut traps at top rim (for lid screws)
        translate([-nodisp_case_w/3, 0, nodisp_case_h - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        translate([nodisp_case_w/3, 0, nodisp_case_h - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        translate([0, -nodisp_case_d/3, nodisp_case_h - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        translate([0, nodisp_case_d/3, nodisp_case_h - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        
        // PowerPole pocket on back wall
        translate([0, nodisp_case_d/2 - wall/2, nodisp_case_h/2]) {
            rotate([90, 0, 0]) {
                powerpole_pocket();
            }
        }
        
        // Cable hole on back wall
        translate([nodisp_case_w/4, nodisp_case_d/2 - wall/2, nodisp_case_h/3]) {
            rotate([90, 0, 0]) {
                cable_grommet_hole(10);
            }
        }
        
        // Vent slots on left side
        translate([-nodisp_case_w/2 + wall/2, 0, nodisp_case_h/2]) {
            rotate([0, 90, 0]) {
                vent_slots(4, vent_spacing, vent_slot_l, vent_slot_w);
            }
        }
        
        // Vent slots on right side
        translate([nodisp_case_w/2 - wall/2, 0, nodisp_case_h/2]) {
            rotate([0, 90, 0]) {
                vent_slots(4, vent_spacing, vent_slot_l, vent_slot_w);
            }
        }
        
        // Rubber feet recesses on bottom
        translate([-nodisp_case_w/3, -nodisp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([nodisp_case_w/3, -nodisp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([-nodisp_case_w/3, nodisp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([nodisp_case_w/3, nodisp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
    }
}

// Box-shaped outer shell
module box_shell() {
    cube([nodisp_case_w, nodisp_case_d, nodisp_case_h], center = true);
}

// ============================================================================
// LID (TOP HALF)
// ============================================================================
module nodisplay_lid() {
    difference() {
        // Flat plate
        cube([nodisp_case_w, nodisp_case_d, wall], center = true);
        
        // M3 clearance holes (countersunk)
        translate([-nodisp_case_w/3, 0, 0]) {
            screw_hole(wall + 0.1, countersink = true);
        }
        translate([nodisp_case_w/3, 0, 0]) {
            screw_hole(wall + 0.1, countersink = true);
        }
        translate([0, -nodisp_case_d/3, 0]) {
            screw_hole(wall + 0.1, countersink = true);
        }
        translate([0, nodisp_case_d/3, 0]) {
            screw_hole(wall + 0.1, countersink = true);
        }
    }
    
    // Locating pins on underside
    translate([-nodisp_case_w/3, 0, -wall/2]) {
        cylinder(d = 2, h = 2, $fn = 16);
    }
    translate([nodisp_case_w/3, 0, -wall/2]) {
        cylinder(d = 2, h = 2, $fn = 16);
    }
    translate([0, -nodisp_case_d/3, -wall/2]) {
        cylinder(d = 2, h = 2, $fn = 16);
    }
    translate([0, nodisp_case_d/3, -wall/2]) {
        cylinder(d = 2, h = 2, $fn = 16);
    }
}

// ============================================================================
// ASSEMBLY PREVIEW
// ============================================================================
module nodisplay_case_assembly() {
    color("LightBlue", 0.7) {
        nodisplay_base();
    }
    
    color("LightGreen", 0.7) {
        translate([0, 0, nodisp_case_h/2 + wall/2]) {
            nodisplay_lid();
        }
    }
}

// ============================================================================
// RENDER SELECTION
// ============================================================================
// Uncomment the part you want to render:

// Render base for 3D printing (print upside-down)
nodisplay_base();

// Render lid for 3D printing (print right-side-up)
// nodisplay_lid();

// Render assembly preview
// nodisplay_case_assembly();
