#ifdef WITH_DISPLAY

#include "display_ui.h"
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

DisplayUI* DisplayUI::s_instance = nullptr;
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

DisplayUI::DisplayUI()
    : m_tft(nullptr), m_tuner(nullptr),
      m_scr(nullptr), m_meter(nullptr),
      m_bandLabel(nullptr), m_scaleLabel(nullptr),
      m_modeLabel(nullptr), m_antLabel(nullptr),
      m_ssidLabel(nullptr), m_ipLabel(nullptr),
      m_btnMemTune(nullptr), m_btnFullTune(nullptr),
      m_btnToggle(nullptr), m_btnBypass(nullptr), m_btnAuto(nullptr),
      m_highScale(true), m_maxPower(1000.0f),
      m_fwdPower(0), m_refPower(0), m_swr(1.0f) {
    s_instance = this;
}

DisplayUI::~DisplayUI() {
    if (m_tft) delete m_tft;
    s_instance = nullptr;
}

bool DisplayUI::begin(TunerProtocol* tuner) {
    m_tuner = tuner;
    m_tft = new TFT_eSPI();
    m_tft->init();
    m_tft->setRotation(DISPLAY_ROTATION);
    m_tft->fillScreen(TFT_BLACK);

    lv_init();
    ts.begin();
    ts.setRotation(1);

    lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[DISPLAY_WIDTH * 10];
    static lv_color_t buf2[DISPLAY_WIDTH * 10];
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_WIDTH * 10);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = displayFlush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    setupTouch();

    m_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(m_scr, lv_color_hex(0x0a0a1a), LV_PART_MAIN);
    lv_obj_set_style_border_width(m_scr, 0, LV_PART_MAIN);
    lv_scr_load(m_scr);

    // ============================================================
    // LEFT PANEL: Cross-needle meter (~300x300)
    // ============================================================
    m_meter = lv_obj_create(m_scr);
    lv_obj_set_size(m_meter, 300, 300);
    lv_obj_align(m_meter, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(m_meter, lv_color_hex(0x111122), LV_PART_MAIN);
    lv_obj_set_style_border_width(m_meter, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_meter, 0, LV_PART_MAIN);
    lv_obj_add_flag(m_meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_meter, drawMeterEvent, LV_EVENT_DRAW_POST, NULL);
    lv_obj_add_event_cb(m_meter, meterTapEvent, LV_EVENT_CLICKED, NULL);

    // Band label
    m_bandLabel = lv_label_create(m_scr);
    lv_label_set_text(m_bandLabel, "Band: --");
    lv_obj_set_style_text_color(m_bandLabel, lv_color_hex(0x00ff00), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_bandLabel, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align_to(m_bandLabel, m_meter, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 5);

    // Scale label
    m_scaleLabel = lv_label_create(m_scr);
    lv_label_set_text(m_scaleLabel, "1000W");
    lv_obj_set_style_text_color(m_scaleLabel, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_scaleLabel, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align_to(m_scaleLabel, m_meter, LV_ALIGN_OUT_BOTTOM_RIGHT, -10, 5);

    // ============================================================
    // RIGHT PANEL: Info + buttons (~150px wide)
    // ============================================================
    int rightX = 310;

    // SSID
    m_ssidLabel = lv_label_create(m_scr);
    lv_label_set_text_fmt(m_ssidLabel, "SSID: %s", WiFi.SSID().c_str());
    lv_obj_set_style_text_color(m_ssidLabel, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_ssidLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(m_ssidLabel, LV_ALIGN_TOP_RIGHT, -5, 5);

    // IP
    m_ipLabel = lv_label_create(m_scr);
    lv_label_set_text_fmt(m_ipLabel, "IP: %s", WiFi.localIP().toString().c_str());
    lv_obj_set_style_text_color(m_ipLabel, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_ipLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(m_ipLabel, LV_ALIGN_TOP_RIGHT, -5, 22);

    // Mode
    m_modeLabel = lv_label_create(m_scr);
    lv_label_set_text(m_modeLabel, "Mode: --");
    lv_obj_set_style_text_color(m_modeLabel, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_modeLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(m_modeLabel, LV_ALIGN_TOP_RIGHT, -5, 45);

    // Antenna
    m_antLabel = lv_label_create(m_scr);
    lv_label_set_text(m_antLabel, "ANT: --");
    lv_obj_set_style_text_color(m_antLabel, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(m_antLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(m_antLabel, LV_ALIGN_TOP_RIGHT, -5, 65);

    // Buttons
    int btnX = rightX + 5;
    int btnW = 140;
    int btnH = 36;
    int btnY = 95;
    int btnGap = 5;

    const struct { lv_obj_t** btn; const char* label; lv_color_t color; lv_color_t pressed; } btns[] = {
        {&m_btnMemTune,   "MEM TUNE",   lv_color_hex(0xcc3333), lv_color_hex(0xff4444)},
        {&m_btnFullTune,  "FULL TUNE",  lv_color_hex(0xcc3333), lv_color_hex(0xff4444)},
        {&m_btnToggle,    "TOGGLE ANT", lv_color_hex(0x2255aa), lv_color_hex(0x3366cc)},
        {&m_btnBypass,    "BYPASS",     lv_color_hex(0xcc8800), lv_color_hex(0xffaa00)},
        {&m_btnAuto,      "AUTO MODE",  lv_color_hex(0x2255aa), lv_color_hex(0x3366cc)},
    };

    for (int i = 0; i < 5; i++) {
        *btns[i].btn = lv_btn_create(m_scr);
        lv_obj_set_size(*btns[i].btn, btnW, btnH);
        lv_obj_set_pos(*btns[i].btn, btnX, btnY);
        lv_obj_set_style_bg_color(*btns[i].btn, btns[i].color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(*btns[i].btn, btns[i].pressed, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(*btns[i].btn, 4, LV_PART_MAIN);

        lv_obj_t* lbl = lv_label_create(*btns[i].btn);
        lv_label_set_text(lbl, btns[i].label);
        lv_obj_center(lbl);

        btnY += btnH + btnGap;
    }

    lv_obj_add_event_cb(m_btnMemTune, btnMemTuneEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(m_btnFullTune, btnFullTuneEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(m_btnToggle, btnToggleEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(m_btnBypass, btnBypassEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(m_btnAuto, btnAutoEvent, LV_EVENT_CLICKED, NULL);

    return true;
}

void DisplayUI::loop() {
    lv_timer_handler();
    delay(5);
}

void DisplayUI::updateMeter(const tuner_meter_t* meter) {
    m_fwdPower = (float)meter->forward_power_watts;
    m_refPower = (float)meter->reflected_power_watts;
    m_swr = (float)meter->swr;

    lv_obj_invalidate(m_meter);

    char buf[32];
    snprintf(buf, sizeof(buf), "Band: %s", TunerProtocol::bandToString(meter->band));
    lv_label_set_text(m_bandLabel, buf);
}

void DisplayUI::updateStatus(tuner_mode_t mode, tuner_ant_t antenna) {
    switch (mode) {
        case MODE_AUTO:    lv_label_set_text(m_modeLabel, "Mode: AUTO"); break;
        case MODE_MANUAL:  lv_label_set_text(m_modeLabel, "Mode: MANUAL"); break;
        case MODE_BYPASS:  lv_label_set_text(m_modeLabel, "Mode: BYPASS"); break;
        default:           lv_label_set_text(m_modeLabel, "Mode: --"); break;
    }
    switch (antenna) {
        case ANT_A: lv_label_set_text(m_antLabel, "ANT: A"); break;
        case ANT_B: lv_label_set_text(m_antLabel, "ANT: B"); break;
        default:    lv_label_set_text(m_antLabel, "ANT: --"); break;
    }
}

void DisplayUI::setupTouch() {
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t* drv, lv_indev_data_t* data) {
        if (s_instance) s_instance->readTouch(drv, data);
    };
    lv_indev_drv_register(&indev_drv);
}

void DisplayUI::readTouch(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        data->state = LV_INDEV_STATE_PR;
        data->point.x = map(p.x, 0, 4095, 0, DISPLAY_WIDTH);
        data->point.y = map(p.y, 0, 4095, 0, DISPLAY_HEIGHT);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void DisplayUI::drawMeterEvent(lv_event_t* e) {
    if (s_instance) s_instance->drawCrossNeedle(e);
}

void DisplayUI::drawCrossNeedle(lv_event_t* e) {
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);
    lv_obj_t* obj = lv_event_get_target(e);

    int w = lv_obj_get_width(obj);
    int h = lv_obj_get_height(obj);
    lv_coord_t x = lv_obj_get_x(obj);
    lv_coord_t y = lv_obj_get_y(obj);

    // Meter face center and radius
    int cx = w / 2;
    int cy = h - 25; // pivot line near bottom
    int radius = w / 2 - 20;

    // Draw scale arc (180 degrees, left to right)
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0x444444);
    arc_dsc.width = 2;
    lv_point_t arc_center = {(lv_coord_t)cx, (lv_coord_t)(cy - 10)};
    lv_draw_arc(draw_ctx, &arc_dsc, &arc_center, radius, 180, 360);

    // Draw tick marks
    int numTicks = 11;
    for (int i = 0; i <= numTicks; i++) {
        float angle = 3.14159f * (1.0f - (float)i / numTicks);
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        int tickLen = (i % 5 == 0) ? 12 : 6;
        lv_point_t p1 = {(lv_coord_t)(cx + (int)(cos_a * (radius + 5))),
                         (lv_coord_t)((cy - 10) - (int)(sin_a * (radius + 5)))};
        lv_point_t p2 = {(lv_coord_t)(cx + (int)(cos_a * (radius + 5 + tickLen))),
                         (lv_coord_t)((cy - 10) - (int)(sin_a * (radius + 5 + tickLen)))};

        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = (i % 5 == 0) ? lv_color_hex(0xffffff) : lv_color_hex(0x888888);
        line_dsc.width = (i % 5 == 0) ? 2 : 1;
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

        // Labels on major ticks
        if (i % 5 == 0) {
            int val = (int)((float)i / numTicks * m_maxPower);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", val);

            lv_draw_label_dsc_t label_dsc;
            lv_draw_label_dsc_init(&label_dsc);
            label_dsc.color = lv_color_hex(0xcccccc);
            label_dsc.font = &lv_font_montserrat_14;
            label_dsc.align = LV_TEXT_ALIGN_CENTER;

            int lx = cx + (int)(cos_a * (radius - 20));
            int ly = (cy - 10) - (int)(sin_a * (radius - 20));
            lv_area_t label_area;
            label_area.x1 = (lv_coord_t)(lx - 15);
            label_area.y1 = (lv_coord_t)(ly - 8);
            label_area.x2 = (lv_coord_t)(lx + 15);
            label_area.y2 = (lv_coord_t)(ly + 8);
            lv_draw_label(draw_ctx, &label_dsc, &label_area, buf, NULL);
        }
    }

    // Draw pivot dots
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);

    // Forward needle pivot (bottom-left)
    int fwdPx = cx - radius + 15;
    int fwdPy = cy;
    rect_dsc.bg_color = lv_color_hex(0x00aa00);
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    lv_area_t pivot_area = {(lv_coord_t)(fwdPx - 5), (lv_coord_t)(fwdPy - 5),
                            (lv_coord_t)(fwdPx + 5), (lv_coord_t)(fwdPy + 5)};
    lv_draw_rect(draw_ctx, &rect_dsc, &pivot_area);

    // Reflected needle pivot (bottom-right)
    int refPx = cx + radius - 15;
    int refPy = cy;
    rect_dsc.bg_color = lv_color_hex(0xaa0000);
    lv_area_t pivot_area2 = {(lv_coord_t)(refPx - 5), (lv_coord_t)(refPy - 5),
                             (lv_coord_t)(refPx + 5), (lv_coord_t)(refPy + 5)};
    lv_draw_rect(draw_ctx, &rect_dsc, &pivot_area2);

    // Calculate needle angles (0 to 180 degrees)
    float fwdAngle = 3.14159f * (1.0f - fminf(m_fwdPower / m_maxPower, 1.0f));
    float refAngle = 3.14159f * (1.0f - fminf(m_refPower / m_maxPower, 1.0f));

    // Draw forward needle (green, pivots from bottom-left)
    int fwdLen = radius * 2 - 30;
    lv_point_t fwdStart = {(lv_coord_t)fwdPx, (lv_coord_t)fwdPy};
    lv_point_t fwdEnd = {(lv_coord_t)(fwdPx + (int)(cosf(fwdAngle) * fwdLen)),
                         (lv_coord_t)(fwdPy - (int)(sinf(fwdAngle) * fwdLen))};

    lv_draw_line_dsc_t needle_dsc;
    lv_draw_line_dsc_init(&needle_dsc);
    needle_dsc.color = lv_color_hex(0x00ff00);
    needle_dsc.width = 3;
    needle_dsc.round_end = true;
    lv_draw_line(draw_ctx, &needle_dsc, &fwdStart, &fwdEnd);

    // Draw reflected needle (red, pivots from bottom-right)
    int refLen = radius * 2 - 30;
    lv_point_t refStart = {(lv_coord_t)refPx, (lv_coord_t)refPy};
    lv_point_t refEnd = {(lv_coord_t)(refPx - (int)(cosf(refAngle) * refLen)),
                         (lv_coord_t)(refPy - (int)(sinf(refAngle) * refLen))};

    needle_dsc.color = lv_color_hex(0xff0000);
    needle_dsc.width = 3;
    lv_draw_line(draw_ctx, &needle_dsc, &refStart, &refEnd);

    // Draw SWR indicator text in center
    char swrBuf[16];
    snprintf(swrBuf, sizeof(swrBuf), "%.1f:1", m_swr);
    lv_draw_label_dsc_t swr_dsc;
    lv_draw_label_dsc_init(&swr_dsc);
    swr_dsc.color = (m_swr > 2.0f) ? lv_color_hex(0xff4444) :
                    (m_swr > 1.5f) ? lv_color_hex(0xffaa00) : lv_color_hex(0x00ff00);
    swr_dsc.font = &lv_font_montserrat_24;
    swr_dsc.align = LV_TEXT_ALIGN_CENTER;

    lv_area_t swr_area = {(lv_coord_t)(cx - 30), (lv_coord_t)(cy - radius / 2 - 12),
                          (lv_coord_t)(cx + 30), (lv_coord_t)(cy - radius / 2 + 12)};
    lv_draw_label(draw_ctx, &swr_dsc, &swr_area, swrBuf, NULL);
}

void DisplayUI::displayFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    s_instance->m_tft->startWrite();
    s_instance->m_tft->setAddrWindow(area->x1, area->y1, w, h);
    s_instance->m_tft->pushColors((uint16_t*)&color_p->full, w * h, true);
    s_instance->m_tft->endWrite();

    lv_disp_flush_ready(disp);
}

void DisplayUI::btnMemTuneEvent(lv_event_t* e) {
    if (s_instance && s_instance->m_tuner) s_instance->m_tuner->memoryTune();
}

void DisplayUI::btnFullTuneEvent(lv_event_t* e) {
    if (s_instance && s_instance->m_tuner) s_instance->m_tuner->fullTune();
}

void DisplayUI::btnToggleEvent(lv_event_t* e) {
    if (s_instance && s_instance->m_tuner) s_instance->m_tuner->toggleAntenna();
}

void DisplayUI::btnBypassEvent(lv_event_t* e) {
    if (s_instance && s_instance->m_tuner) s_instance->m_tuner->bypass();
}

void DisplayUI::btnAutoEvent(lv_event_t* e) {
    if (s_instance && s_instance->m_tuner) s_instance->m_tuner->setAutoMode();
}

void DisplayUI::meterTapEvent(lv_event_t* e) {
    if (!s_instance) return;
    s_instance->m_highScale = !s_instance->m_highScale;
    s_instance->m_maxPower = s_instance->m_highScale ? 1000.0f : 100.0f;
    lv_label_set_text(s_instance->m_scaleLabel, s_instance->m_highScale ? "1000W" : "100W");
    lv_obj_invalidate(s_instance->m_meter);
}

#endif // WITH_DISPLAY
