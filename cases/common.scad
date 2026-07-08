// Common parameters and utility modules for LDG Controller cases
// Shared between display_case.scad and nodisplay_case.scad

// ============================================================================
// PRINT PARAMETERS
// ============================================================================
wall    = 2.5;
floor_t = 2.5;
tol     = 0.3;

// ============================================================================
// FASTENER PARAMETERS
// ============================================================================
m3_dia      = 3.4;
m3_nut_af   = 5.6;
m3_nut_dep  = 2.5;
m3_head_dia = 6.0;
m3_head_dep = 2.0;

// ============================================================================
// COMPONENT DIMENSIONS
// ============================================================================

// BTT TFT35-SPI V2.1 Display
tft_pcb_w          = 110;
tft_pcb_h          = 60;
tft_pcb_t          = 1.6;
tft_screen_w       = 73;
tft_screen_h       = 52;
tft_hole_spacing_x = 105;
tft_hole_spacing_y = 55.5;
tft_hole_dia       = 3.2;

// NodeMCU ESP32S
nodemcu_w    = 75;
nodemcu_h    = 62;
nodemcu_t    = 1.6;
nodemcu_usb_w = 8;
nodemcu_usb_h = 4;
nodemcu_hole_x = 71.5;
nodemcu_hole_y = 57.5;

// ESP32 screw-terminal breakout board
breakout_w = 58;
breakout_h = 52;
breakout_t = 10;

// PowerPole 2-pos housing (Anderson PP15/30/45)
pp_w   = 16.8;
pp_h   = 8.5;
pp_lip = 0.8;

// Mini-DIN 4-pin connector
minidin_dia = 13;

// ============================================================================
// UTILITY MODULES
// ============================================================================

module rounded_rect(w, h, r) {
    offset(r = r) {
        square([w - 2*r, h - 2*r], center = true);
    }
}

module nut_trap(depth = m3_nut_dep) {
    cylinder(d = m3_nut_af / cos(30), h = depth, $fn = 6);
}

module screw_hole(h = 20, countersink = false) {
    cylinder(d = m3_dia, h = h, $fn = 24);
    if (countersink) {
        translate([0, 0, -m3_head_dep]) {
            cylinder(d1 = m3_dia, d2 = m3_head_dia, h = m3_head_dep, $fn = 24);
        }
    }
}

module standoff(od, height, bore = m3_dia) {
    difference() {
        cylinder(d = od, h = height, $fn = 32);
        translate([0, 0, -0.1]) {
            cylinder(d = bore, h = height + 0.2, $fn = 24);
        }
    }
}

module rubber_foot(dia, depth) {
    cylinder(d = dia, h = depth, $fn = 24);
}

module vent_slots(count, spacing, slot_length, slot_width) {
    for (i = [0 : count - 1]) {
        translate([0, i * spacing, 0]) {
            cube([slot_width, slot_length, wall + 0.1], center = true);
        }
    }
}

module powerpole_pocket() {
    cube([pp_w + 2*tol, pp_h + 2*tol, wall + 0.1], center = true);
    translate([0, 0, wall/2]) {
        cube([pp_w - 2, pp_h - 2, pp_lip], center = true);
    }
}

// ============================================================================
// ESP32 / BREAKOUT MODULES
// ============================================================================

module esp32_standoffs(height = 5) {
    od = 6;
    positions = [
        [-nodemcu_hole_x/2, -nodemcu_hole_y/2],
        [ nodemcu_hole_x/2, -nodemcu_hole_y/2],
        [-nodemcu_hole_x/2,  nodemcu_hole_y/2],
        [ nodemcu_hole_x/2,  nodemcu_hole_y/2]
    ];
    for (p = positions) {
        translate([p[0], p[1], 0]) standoff(od, height);
    }
}

module esp32_usb_cutout() {
    cube([nodemcu_usb_w + 2, nodemcu_usb_h + 2, wall + 0.1], center = true);
}

// ============================================================================
// DISPLAY MODULES
// ============================================================================

module display_standoffs(height = 3) {
    od = 6;
    positions = [
        [-tft_hole_spacing_x/2, -tft_hole_spacing_y/2],
        [ tft_hole_spacing_x/2, -tft_hole_spacing_y/2],
        [-tft_hole_spacing_x/2,  tft_hole_spacing_y/2],
        [ tft_hole_spacing_x/2,  tft_hole_spacing_y/2]
    ];
    for (p = positions) {
        translate([p[0], p[1], 0]) standoff(od, height);
    }
}

module display_screen_cutout() {
    cube([tft_screen_w + 2, tft_screen_h + 2, wall + 0.1], center = true);
}

module display_pcb_recess(depth = 2) {
    cube([tft_pcb_w + 2*tol, tft_pcb_h + 2*tol, depth], center = true);
}
