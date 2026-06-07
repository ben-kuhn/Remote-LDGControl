#ifdef WITH_DISPLAY

#include "display_ui.h"
#include <WiFi.h>
#include <Wire.h>
#include <math.h>

// NS2009 I2C touch controller (BTT TFT35-SPI V2.1)
// I2C address: 0x48
#define NS2009_ADDR 0x48
#define NS2009_CMD_READ_X 0xC0
#define NS2009_CMD_READ_Y 0xD0

DisplayUI* DisplayUI::s_instance = nullptr;

static bool ns2009Read(uint16_t* x, uint16_t* y) {
    Wire.requestFrom(NS2009_ADDR, (uint8_t)5);
    if (Wire.available() < 5) return false;
    Wire.read(); // skip status
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    *x = (xh << 5) | (xl >> 3);
    *y = (yh << 5) | (yl >> 3);
    return true;
}

static bool tsTouched() {
    Wire.beginTransmission(NS2009_ADDR);
    Wire.write(NS2009_CMD_READ_X);
    return (Wire.endTransmission() == 0);
}

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
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    lv_disp_draw_buf_t draw_buf;
    // 5-line draw buffers (~4.8 KB each). LVGL recommends ~1/10 of the
    // screen height as the minimum; halving from 10 lines keeps perf
    // reasonable while saving ~9.6 KB of dram0.bss — the difference
    // between the display variant linking or not.
    static lv_color_t buf1[DISPLAY_WIDTH * 5];
    static lv_color_t buf2[DISPLAY_WIDTH * 5];
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_WIDTH * 5);

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
    if (tsTouched()) {
        uint16_t x, y;
        if (ns2009Read(&x, &y)) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = map(x, 200, 3900, 0, DISPLAY_WIDTH);
            data->point.y = map(y, 200, 3900, 0, DISPLAY_HEIGHT);
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
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

    // Geometry — mirrors data/index.html drawMeter(). Pivots sit at the
    // bottom corners; arc spans the upper half. Forward pivot is bottom-
    // RIGHT, reflected is bottom-LEFT (matches CLAUDE.md convention).
    const int pad   = 20;
    const int cx    = w / 2;
    const int cy    = h - pad;
    const int radius = w / 2 - pad;
    const float PI_F = 3.14159265f;

    // Pivot positions
    const int fwdPx = cx + radius;  // bottom-right
    const int fwdPy = cy;
    const int refPx = cx - radius;  // bottom-left
    const int refPy = cy;

    auto tipOnArc = [&](float phi, int& tx, int& ty) {
        tx = cx + (int)(radius * cosf(phi));
        ty = cy + (int)(radius * sinf(phi));  // screen coords; sin direct
    };

    // ----- Scale arc -----
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0x555555);
    arc_dsc.width = 2;
    lv_point_t arc_center = {(lv_coord_t)cx, (lv_coord_t)cy};
    lv_draw_arc(draw_ctx, &arc_dsc, &arc_center, radius, 180, 360);

    // ----- SWR curves (drawn behind needles) -----
    static const float swrValues[]   = { 1.5f, 2.0f, 3.0f, 5.0f, 10.0f };
    static const uint32_t swrColors[] = { 0x84cc16, 0xeab308, 0xf97316, 0xef4444, 0xa855f7 };
    const int swrCount = sizeof(swrValues) / sizeof(swrValues[0]);
    lv_draw_line_dsc_t curve_dsc;
    lv_draw_line_dsc_init(&curve_dsc);
    curve_dsc.width = 1;
    curve_dsc.opa = LV_OPA_60;
    for (int si = 0; si < swrCount; si++) {
        const float swr = swrValues[si];
        const float gamma = (swr - 1.0f) / (swr + 1.0f);
        const float gammaSq = gamma * gamma;
        curve_dsc.color = lv_color_hex(swrColors[si]);
        bool havePrev = false;
        int prevX = 0, prevY = 0;
        for (int i = 1; i <= 40; i++) {
            const float fwdFrac = (float)i / 40.0f;
            const float refFrac = fwdFrac * gammaSq;
            if (refFrac > 1.0f) continue;
            const float fwdPhi = PI_F + fwdFrac * (PI_F / 2.0f);
            const float refPhi = 2.0f * PI_F - refFrac * (PI_F / 2.0f);
            int fwdTx, fwdTy, refTx, refTy;
            tipOnArc(fwdPhi, fwdTx, fwdTy);
            tipOnArc(refPhi, refTx, refTy);
            // Intersection of (fwdPx,fwdPy)-(fwdTx,fwdTy) and (refPx,refPy)-(refTx,refTy)
            const float dx1 = fwdTx - fwdPx, dy1 = fwdTy - fwdPy;
            const float dx2 = refTx - refPx, dy2 = refTy - refPy;
            const float denom = dx1 * dy2 - dy1 * dx2;
            if (fabsf(denom) < 0.001f) continue;
            const float t = ((refPx - fwdPx) * dy2 - (refPy - fwdPy) * dx2) / denom;
            const int ix = fwdPx + (int)(t * dx1);
            const int iy = fwdPy + (int)(t * dy1);
            if (havePrev) {
                lv_point_t p1 = {(lv_coord_t)prevX, (lv_coord_t)prevY};
                lv_point_t p2 = {(lv_coord_t)ix,    (lv_coord_t)iy};
                lv_draw_line(draw_ctx, &curve_dsc, &p1, &p2);
            }
            prevX = ix; prevY = iy; havePrev = true;
        }
    }

    // ----- Tick marks + labels for the two scales -----
    auto drawScale = [&](float phiZero, float phiFull, uint32_t color) {
        const int N = 8;
        for (int i = 0; i <= N; i++) {
            const float f = (float)i / (float)N;
            const float phi = phiZero + (phiFull - phiZero) * f;
            const float cos_a = cosf(phi);
            const float sin_a = sinf(phi);
            const bool major = (i % 2 == 0);
            const int tickLen = major ? 10 : 5;
            lv_point_t p1 = {(lv_coord_t)(cx + (int)(cos_a * (radius + 2))),
                             (lv_coord_t)(cy + (int)(sin_a * (radius + 2)))};
            lv_point_t p2 = {(lv_coord_t)(cx + (int)(cos_a * (radius + 2 + tickLen))),
                             (lv_coord_t)(cy + (int)(sin_a * (radius + 2 + tickLen)))};
            lv_draw_line_dsc_t tick_dsc;
            lv_draw_line_dsc_init(&tick_dsc);
            tick_dsc.color = major ? lv_color_hex(color) : lv_color_hex(0x666666);
            tick_dsc.width = major ? 2 : 1;
            lv_draw_line(draw_ctx, &tick_dsc, &p1, &p2);
            if (major && i < N) {
                int val = (int)(f * m_maxPower);
                char buf[8];
                snprintf(buf, sizeof(buf), "%d", val);
                lv_draw_label_dsc_t label_dsc;
                lv_draw_label_dsc_init(&label_dsc);
                label_dsc.color = lv_color_hex(color);
                label_dsc.font = &lv_font_montserrat_14;
                label_dsc.align = LV_TEXT_ALIGN_CENTER;
                int lx = cx + (int)(cos_a * (radius - 16));
                int ly = cy + (int)(sin_a * (radius - 16));
                lv_area_t label_area = {(lv_coord_t)(lx - 16), (lv_coord_t)(ly - 8),
                                         (lv_coord_t)(lx + 16), (lv_coord_t)(ly + 8)};
                lv_draw_label(draw_ctx, &label_dsc, &label_area, buf, NULL);
            }
        }
    };
    drawScale(PI_F,         1.5f * PI_F, 0x22c55e);  // forward, left-half
    drawScale(2.0f * PI_F,  1.5f * PI_F, 0xef4444);  // reflected, right-half

    // Shared max-scale label above the apex
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", (int)m_maxPower);
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.color = lv_color_hex(0xcccccc);
        label_dsc.font = &lv_font_montserrat_14;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t a = {(lv_coord_t)(cx - 22), (lv_coord_t)(cy - radius - 18),
                       (lv_coord_t)(cx + 22), (lv_coord_t)(cy - radius - 2)};
        lv_draw_label(draw_ctx, &label_dsc, &a, buf, NULL);
    }

    // ----- Needles (tip on arc) -----
    const float fwdFrac = fminf(m_fwdPower / m_maxPower, 1.0f);
    const float refFrac = fminf(m_refPower / m_maxPower, 1.0f);
    const float fwdPhi  = PI_F + fwdFrac * (PI_F / 2.0f);
    const float refPhi  = 2.0f * PI_F - refFrac * (PI_F / 2.0f);
    int fwdTx, fwdTy, refTx, refTy;
    tipOnArc(fwdPhi, fwdTx, fwdTy);
    tipOnArc(refPhi, refTx, refTy);

    lv_draw_line_dsc_t needle_dsc;
    lv_draw_line_dsc_init(&needle_dsc);
    needle_dsc.width = 3;
    needle_dsc.round_end = true;

    needle_dsc.color = lv_color_hex(0x00ff00);
    lv_point_t fwdStart = {(lv_coord_t)fwdPx, (lv_coord_t)fwdPy};
    lv_point_t fwdEnd   = {(lv_coord_t)fwdTx, (lv_coord_t)fwdTy};
    lv_draw_line(draw_ctx, &needle_dsc, &fwdStart, &fwdEnd);

    needle_dsc.color = lv_color_hex(0xff0000);
    lv_point_t refStart = {(lv_coord_t)refPx, (lv_coord_t)refPy};
    lv_point_t refEnd   = {(lv_coord_t)refTx, (lv_coord_t)refTy};
    lv_draw_line(draw_ctx, &needle_dsc, &refStart, &refEnd);

    // ----- Pivot dots on top of needles -----
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = LV_RADIUS_CIRCLE;
    rect_dsc.bg_color = lv_color_hex(0x00aa00);
    lv_area_t fwdPivot = {(lv_coord_t)(fwdPx - 5), (lv_coord_t)(fwdPy - 5),
                          (lv_coord_t)(fwdPx + 5), (lv_coord_t)(fwdPy + 5)};
    lv_draw_rect(draw_ctx, &rect_dsc, &fwdPivot);
    rect_dsc.bg_color = lv_color_hex(0xaa0000);
    lv_area_t refPivot = {(lv_coord_t)(refPx - 5), (lv_coord_t)(refPy - 5),
                          (lv_coord_t)(refPx + 5), (lv_coord_t)(refPy + 5)};
    lv_draw_rect(draw_ctx, &rect_dsc, &refPivot);

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
