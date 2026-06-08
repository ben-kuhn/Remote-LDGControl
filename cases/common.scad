// Common parameters and modules for LDG Controller cases
// Shared between display_case.scad and nodisplay_case.scad

// ============================================================================
// PRINT PARAMETERS
// ============================================================================
wall        = 2.5;    // wall thickness
floor_t     = 2.5;    // floor thickness
tol         = 0.3;    // sliding fit clearance
layer_h     = 0.2;    // assumed layer height

// ============================================================================
// FASTENER PARAMETERS
// ============================================================================
m3_dia      = 3.4;    // M3 clearance hole
m3_nut_af   = 5.6;    // M3 nut across-flats
m3_nut_dep  = 2.5;    // M3 nut trap depth
m3_head_dia = 6.0;    // M3 countersunk head diameter
m3_head_dep = 2.0;    // M3 countersunk head depth

// ============================================================================
// COMPONENT DIMENSIONS
// ============================================================================

// BTT TFT35-SPI V2.1 Display
tft_pcb_w          = 110;   // PCB width
tft_pcb_h          = 60;    // PCB height
tft_pcb_t          = 1.6;   // PCB thickness
tft_screen_w       = 73;    // active screen width
tft_screen_h       = 52;    // active screen height
tft_bezel_top      = 3;     // top bezel
tft_bezel_bottom   = 5;     // bottom bezel
tft_hole_spacing_x = 105;   // mounting hole X spacing
tft_hole_spacing_y = 55.5;  // mounting hole Y spacing
tft_hole_dia       = 3.2;   // mounting hole diameter

// NodeMCU ESP32S
nodemcu_w          = 54;    // PCB width
nodemcu_h          = 28;    // PCB height
nodemcu_t          = 1.6;   // PCB thickness
nodemcu_usb_w      = 8;     // USB connector width
nodemcu_usb_h      = 4;     // USB connector height
nodemcu_usb_overhang = 3;   // USB overhang beyond PCB edge
nodemcu_hole_spacing_x = 48; // mounting hole X spacing (estimated)
nodemcu_hole_spacing_y = 22; // mounting hole Y spacing (estimated)

// PowerPole 2-pos housing (Anderson PP15/30/45)
pp_w        = 16.8;   // housing width
pp_h        = 8.5;    // housing height
pp_lip      = 0.8;    // retaining lip thickness

// Mini-DIN 4-pin connector
minidin_dia = 13;     // panel cutout diameter

// LM2940 TO-220 regulator with heatsink
reg_w       = 10;     // regulator body width
reg_h       = 15;     // regulator body height
reg_d       = 4.5;    // regulator body depth
heatsink_w  = 25;     // heatsink width
heatsink_h  = 20;     // heatsink height
heatsink_d  = 15;     // heatsink depth

// ============================================================================
// CASE DIMENSIONS
// ============================================================================

// Display case
disp_case_w      = 130;   // case width
disp_case_d      = 120;   // case depth
disp_case_h_back = 60;    // back wall height
disp_case_h_front = 15;   // front lip height
tilt_angle       = 20;    // display tilt angle (degrees from vertical)

// Nodisplay case
nodisp_case_w = 90;       // case width
nodisp_case_d = 65;       // case depth
nodisp_case_h = 40;       // case height

// ============================================================================
// DOVETAIL PARAMETERS
// ============================================================================
dt_finger_w   = 8;        // finger width
dt_angle      = 60;       // dovetail angle (degrees)
dt_clearance  = 0.4;      // clearance per side
dt_depth      = 6;        // tongue/channel depth
dt_num_fingers = 4;       // number of fingers

// ============================================================================
// OTHER PARAMETERS
// ============================================================================
rubber_foot_dia = 6;      // rubber foot diameter
rubber_foot_dep = 1;      // rubber foot recess depth
vent_slot_w     = 3;      // vent slot width
vent_slot_l     = 15;     // vent slot length
vent_spacing    = 5;      // spacing between vent slots

// ============================================================================
// UTILITY MODULES
// ============================================================================

// 2D rounded rectangle
module rounded_rect(w, h, r) {
    offset(r = r) {
        square([w - 2*r, h - 2*r], center = true);
    }
}

// M3 nut trap (hexagonal pocket)
module nut_trap(depth = m3_nut_dep) {
    cylinder(d = m3_nut_af / cos(30), h = depth, $fn = 6);
}

// M3 screw hole with optional countersink
module screw_hole(h = 20, countersink = false) {
    cylinder(d = m3_dia, h = h, $fn = 24);
    if (countersink) {
        translate([0, 0, -m3_head_dep]) {
            cylinder(d1 = m3_dia, d2 = m3_head_dia, h = m3_head_dep, $fn = 24);
        }
    }
}

// Cylindrical standoff with M3 bore
module standoff(od, height, bore = m3_dia) {
    difference() {
        cylinder(d = od, h = height, $fn = 32);
        translate([0, 0, -0.1]) {
            cylinder(d = bore, h = height + 0.2, $fn = 24);
        }
    }
}

// Dovetail tongue (male)
module dovetail_tongue(width, num_fingers, length) {
    finger_pitch = width / num_fingers;
    finger_width = finger_pitch * 0.7;
    
    for (i = [0 : num_fingers - 1]) {
        x = -width/2 + finger_pitch/2 + i * finger_pitch;
        translate([x, 0, 0]) {
            linear_extrude(height = length) {
                polygon([
                    [-finger_width/2, 0],
                    [finger_width/2, 0],
                    [finger_width/2 - dt_depth * tan(dt_angle/2), dt_depth],
                    [-finger_width/2 + dt_depth * tan(dt_angle/2), dt_depth]
                ]);
            }
        }
    }
}

// Dovetail channel (female)
module dovetail_channel(width, num_fingers, length) {
    finger_pitch = width / num_fingers;
    finger_width = finger_pitch * 0.7 + 2 * dt_clearance;
    
    for (i = [0 : num_fingers - 1]) {
        x = -width/2 + finger_pitch/2 + i * finger_pitch;
        translate([x, 0, 0]) {
            linear_extrude(height = length) {
                polygon([
                    [-finger_width/2, 0],
                    [finger_width/2, 0],
                    [finger_width/2 - dt_depth * tan(dt_angle/2), dt_depth],
                    [-finger_width/2 + dt_depth * tan(dt_angle/2), dt_depth]
                ]);
            }
        }
    }
}

// Ventilation slots
module vent_slots(count, spacing, slot_length, slot_width) {
    for (i = [0 : count - 1]) {
        translate([0, i * spacing, 0]) {
            cube([slot_width, slot_length, wall + 0.1], center = true);
        }
    }
}

// PowerPole housing pocket (open on one side)
module powerpole_pocket() {
    // Main pocket
    cube([pp_w + 2*tol, pp_h + 2*tol, wall + 0.1], center = true);
    // Retaining lip (smaller opening on inside face)
    translate([0, 0, wall/2]) {
        cube([pp_w - 2, pp_h - 2, pp_lip], center = true);
    }
}

// Cable grommet hole with strain relief bosses
module cable_grommet_hole(dia) {
    cylinder(d = dia, h = wall + 0.1, $fn = 32, center = true);
    // Strain relief bosses
    translate([-dia/2 - 3, 0, 0]) {
        cylinder(d = 3, h = wall + 2, $fn = 16, center = true);
    }
    translate([dia/2 + 3, 0, 0]) {
        cylinder(d = 3, h = wall + 2, $fn = 16, center = true);
    }
}

// Rubber foot recess
module rubber_foot(dia, depth) {
    cylinder(d = dia, h = depth, $fn = 24);
}

// ============================================================================
// DISPLAY-SPECIFIC MODULES
// ============================================================================

// Display mounting standoffs (for TFT35 PCB)
module display_standoffs(height = 3) {
    standoff_od = 6;
    
    // Four corner standoffs
    positions = [
        [-tft_hole_spacing_x/2, -tft_hole_spacing_y/2],
        [tft_hole_spacing_x/2, -tft_hole_spacing_y/2],
        [-tft_hole_spacing_x/2, tft_hole_spacing_y/2],
        [tft_hole_spacing_x/2, tft_hole_spacing_y/2]
    ];
    
    for (pos = positions) {
        translate([pos[0], pos[1], 0]) {
            standoff(standoff_od, height);
        }
    }
}

// Display screen cutout
module display_screen_cutout() {
    // Screen opening (slightly larger than active area)
    cube([tft_screen_w + 2, tft_screen_h + 2, wall + 0.1], center = true);
}

// Display PCB recess pocket
module display_pcb_recess(depth = 2) {
    cube([tft_pcb_w + 2*tol, tft_pcb_h + 2*tol, depth], center = true);
}

// ============================================================================
// ESP32-SPECIFIC MODULES
// ============================================================================

// ESP32 NodeMCU mounting standoffs
module esp32_standoffs(height = 5) {
    standoff_od = 6;
    
    // Four corner standoffs (estimated positions)
    positions = [
        [-nodemcu_hole_spacing_x/2, -nodemcu_hole_spacing_y/2],
        [nodemcu_hole_spacing_x/2, -nodemcu_hole_spacing_y/2],
        [-nodemcu_hole_spacing_x/2, nodemcu_hole_spacing_y/2],
        [nodemcu_hole_spacing_x/2, nodemcu_hole_spacing_y/2]
    ];
    
    for (pos = positions) {
        translate([pos[0], pos[1], 0]) {
            standoff(standoff_od, height);
        }
    }
}

// ESP32 USB cutout
module esp32_usb_cutout() {
    cube([nodemcu_usb_w + 2, nodemcu_usb_h + 2, wall + 0.1], center = true);
}
