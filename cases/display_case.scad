// Display Unit Case for LDG Controller
// Features angled front+top panel with dovetail hinge
// Use OpenSCAD to render and export STL for 3D printing

include <common.scad>

// ============================================================================
// DISPLAY CASE PARAMETERS
// ============================================================================
$fn = 50;

// Calculated dimensions
panel_length = disp_case_d / cos(tilt_angle);  // length along tilted surface
panel_vertical_rise = disp_case_d * tan(tilt_angle);  // vertical rise from front to back

// ============================================================================
// BASE (BOTTOM HALF)
// ============================================================================
module display_base() {
    difference() {
        union() {
            // Wedge-shaped shell
            wedge_shell();
            
            // Dovetail channel at front top
            translate([0, -disp_case_d/2, disp_case_h_front]) {
                rotate([90, 0, 0]) {
                    dovetail_channel(disp_case_w - 2*wall, dt_num_fingers, dt_depth);
                }
            }
        }
        
        // Hollow interior
        translate([0, 0, floor_t]) {
            wedge_interior();
        }
        
        // Nut traps at back top (for lid screws)
        translate([-disp_case_w/4, disp_case_d/2 - wall, disp_case_h_back - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        translate([disp_case_w/4, disp_case_d/2 - wall, disp_case_h_back - m3_nut_dep]) {
            nut_trap(m3_nut_dep + 0.1);
        }
        
        // PowerPole pocket on back wall
        translate([0, disp_case_d/2 - wall/2, disp_case_h_back/2]) {
            rotate([90, 0, 0]) {
                powerpole_pocket();
            }
        }
        
        // Cable hole on back wall
        translate([disp_case_w/4, disp_case_d/2 - wall/2, disp_case_h_back/3]) {
            rotate([90, 0, 0]) {
                cable_grommet_hole(10);
            }
        }
        
        // Vent slots on left side
        translate([-disp_case_w/2 + wall/2, 0, disp_case_h_back/2]) {
            rotate([0, 90, 0]) {
                vent_slots(4, vent_spacing, vent_slot_l, vent_slot_w);
            }
        }
        
        // Vent slots on right side
        translate([disp_case_w/2 - wall/2, 0, disp_case_h_back/2]) {
            rotate([0, 90, 0]) {
                vent_slots(4, vent_spacing, vent_slot_l, vent_slot_w);
            }
        }
        
        // Rubber feet recesses on bottom
        translate([-disp_case_w/3, -disp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([disp_case_w/3, -disp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([-disp_case_w/3, disp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
        translate([disp_case_w/3, disp_case_d/3, 0]) {
            rubber_foot(rubber_foot_dia, rubber_foot_dep);
        }
    }
    
    // ESP32 standoffs inside
    translate([0, -disp_case_d/4, floor_t]) {
        esp32_standoffs(5);
    }
}

// Wedge-shaped outer shell
module wedge_shell() {
    // Create wedge by extruding a trapezoidal profile
    linear_extrude(height = disp_case_w, center = true, convexity = 10) {
        polygon([
            [-disp_case_d/2, 0],
            [disp_case_d/2, 0],
            [disp_case_d/2, disp_case_h_back],
            [-disp_case_d/2, disp_case_h_front]
        ]);
    }
}

// Wedge-shaped interior (for hollowing)
module wedge_interior() {
    linear_extrude(height = disp_case_w - 2*wall, center = true, convexity = 10) {
        polygon([
            [-disp_case_d/2 + wall, 0],
            [disp_case_d/2 - wall, 0],
            [disp_case_d/2 - wall, disp_case_h_back - floor_t],
            [-disp_case_d/2 + wall, disp_case_h_front - floor_t]
        ]);
    }
}

// ============================================================================
// FRONT+TOP PANEL (TOP HALF)
// ============================================================================
module display_front_top() {
    difference() {
        union() {
            // Main panel (tilted)
            translate([0, -disp_case_d/2, disp_case_h_front]) {
                rotate([-tilt_angle, 0, 0]) {
                    // Flat panel
                    translate([0, panel_length/2, 0]) {
                        cube([disp_case_w, panel_length, wall], center = true);
                    }
                }
            }
            
            // Dovetail tongue at bottom edge
            translate([0, -disp_case_d/2, disp_case_h_front]) {
                rotate([90 - tilt_angle, 0, 0]) {
                    dovetail_tongue(disp_case_w - 2*wall, dt_num_fingers, dt_depth);
                }
            }
        }
        
        // M3 clearance holes at top edge
        translate([-disp_case_w/4, disp_case_d/2 - wall, disp_case_h_back]) {
            rotate([-tilt_angle, 0, 0]) {
                screw_hole(wall + 0.1, countersink = true);
            }
        }
        translate([disp_case_w/4, disp_case_d/2 - wall, disp_case_h_back]) {
            rotate([-tilt_angle, 0, 0]) {
                screw_hole(wall + 0.1, countersink = true);
            }
        }
        
        // Display PCB recess (on inside face)
        translate([0, -disp_case_d/2 + panel_length/2 * cos(tilt_angle), 
                   disp_case_h_front + panel_length/2 * sin(tilt_angle)]) {
            rotate([-tilt_angle, 0, 0]) {
                translate([0, 0, wall/2]) {
                    display_pcb_recess(2);
                }
            }
        }
        
        // Display screen cutout (on outside face)
        translate([0, -disp_case_d/2 + panel_length/2 * cos(tilt_angle), 
                   disp_case_h_front + panel_length/2 * sin(tilt_angle)]) {
            rotate([-tilt_angle, 0, 0]) {
                translate([0, 0, -wall/2 - 0.1]) {
                    display_screen_cutout();
                }
            }
        }
    }
    
    // Display standoffs (on inside face)
    translate([0, -disp_case_d/2 + panel_length/2 * cos(tilt_angle), 
               disp_case_h_front + panel_length/2 * sin(tilt_angle)]) {
        rotate([-tilt_angle, 0, 0]) {
            translate([0, 0, wall/2]) {
                display_standoffs(3);
            }
        }
    }
}

// ============================================================================
// ASSEMBLY PREVIEW
// ============================================================================
module display_case_assembly() {
    color("LightBlue", 0.7) {
        display_base();
    }
    
    color("LightGreen", 0.7) {
        display_front_top();
    }
}

// ============================================================================
// RENDER SELECTION
// ============================================================================
// Uncomment the part you want to render:

// Render base for 3D printing (print upside-down)
display_base();

// Render front+top panel for 3D printing (print right-side-up)
// display_front_top();

// Render assembly preview
// display_case_assembly();
