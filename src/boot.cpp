#include "boot.h"

#include <M5Unified.h>
#include <esp_system.h>

#include "lgfx/v1/panel/Panel_IT8951.hpp"
#include "lgfx/v1/touch/Touch_GT911.hpp"

// Build with -DBOOT_FORCE_STOCK to exercise the fallback path (stock autodetect with reset).

namespace boot {

namespace {
uint32_t g_t0Us = 0;
Start g_start = Start::Stock;
uint32_t g_readyWaitMs = 0;  // how long BUSY was low at start-up (controller booting)

const char* resetReasonName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "poweron";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "wdt";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

// ---- display start-up without the IT8951 reset ------------------------------------------------

constexpr int kPinPwr = 2;    // main power hold
constexpr int kPinSdCs = 4;   // SD chip select (shared SPI bus, keep deselected)
constexpr int kPinEpdCs = 15;
constexpr int kPinEpdRst = 23;
constexpr int kPinEpdBusy = 27;
constexpr uint32_t kPanelSizeId = 0x03C0021C;  // 960 (0x03C0) x 540 (0x021C)

// The parts live for the whole run (M5.Display keeps pointers to them).
struct NoResetParts {
    lgfx::Panel_IT8951 panel;
    lgfx::Bus_SPI bus;
    lgfx::Touch_GT911 touch;
};
NoResetParts* g_parts = nullptr;

// Grants access to the protected board id of M5GFX (pointer-to-member through a derived class).
struct BoardAccess : public M5GFX {
    static void set(M5GFX& g, lgfx::board_t b) { g.*(&BoardAccess::_board) = b; }
};

void configureBus(lgfx::Bus_SPI& bus, uint32_t writeHz, uint32_t readHz) {
    auto cfg = bus.config();
    cfg.spi_host = SPI3_HOST;
    cfg.dma_channel = 1;
    cfg.spi_mode = 0;
    cfg.use_lock = true;
    cfg.pin_mosi = GPIO_NUM_12;
    cfg.pin_miso = GPIO_NUM_13;
    cfg.pin_sclk = GPIO_NUM_14;
    cfg.pin_dc = -1;
    cfg.spi_3wire = false;
    cfg.freq_write = writeHz;
    cfg.freq_read = readHz;
    bus.config(cfg);
}

// Same pin state M5GFX's autodetect establishes before it talks to the EPD, minus the reset
// pulse: RST is only held high.
void preparePins() {
    pinMode(kPinPwr, OUTPUT);
    digitalWrite(kPinPwr, HIGH);
    pinMode(kPinSdCs, OUTPUT);
    digitalWrite(kPinSdCs, HIGH);
    pinMode(kPinEpdCs, OUTPUT);
    digitalWrite(kPinEpdCs, HIGH);
    pinMode(kPinEpdRst, OUTPUT);
    digitalWrite(kPinEpdRst, HIGH);
}

// GET_DEV_INFO exactly as in M5GFX.cpp (M5Paper autodetect): wait for BUSY, send 0x6000 0x0302,
// wait, read preamble 0x1000 + dummy word, then 40 bytes. The first 4 bytes are width/height.
bool probeController(lgfx::Bus_SPI& bus, uint32_t& idOut) {
    idOut = 0;
    pinMode(kPinEpdBusy, INPUT);
    uint32_t t = millis();
    while (digitalRead(kPinEpdBusy) == LOW) {
        if (millis() - t > 1024) return false;  // never ready
        delay(1);
    }
    g_readyWaitMs = millis() - t;
    configureBus(bus, 8000000, 8000000);
    if (!bus.init()) return false;
    bus.beginTransaction();
    digitalWrite(kPinEpdCs, LOW);
    bus.writeData(__builtin_bswap16(0x6000), 16);
    bus.writeData(__builtin_bswap16(0x0302), 16);
    t = millis();
    bus.wait();
    digitalWrite(kPinEpdCs, HIGH);
    while (digitalRead(kPinEpdBusy) == LOW) {
        if (millis() - t > 192) break;
        delay(1);
    }
    digitalWrite(kPinEpdCs, LOW);
    bus.writeData(__builtin_bswap16(0x1000), 16);
    bus.writeData(__builtin_bswap16(0x0000), 16);
    uint8_t buf[40];
    bus.beginRead();
    bus.readBytes(buf, 40, false);
    bus.endRead();
    bus.endTransaction();
    digitalWrite(kPinEpdCs, HIGH);
    // The bus stays initialised, as in M5GFX's autodetect: releasing it would glitch the SPI
    // pins while the controller is selected-idle. Panel init re-inits it at the final clock.
    idOut = (uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | buf[3];
    return idOut == kPanelSizeId;
}

// Gives M5.Display the panel. Mirrors M5GFX::autodetect (panel/touch settings) and
// LGFX_Device::init_impl (state set-up after panel init), without the reset pulse.
bool attachNoResetDisplay() {
    preparePins();
    g_parts = new NoResetParts();
    uint32_t id = 0;
    const bool ok = probeController(g_parts->bus, id);
    boot::log("controller_id", (long)id);
    if (!ok) return false;

    configureBus(g_parts->bus, 40000000, 20000000);
    g_parts->panel.bus(&g_parts->bus);
    {
        auto cfg = g_parts->panel.config();
        cfg.panel_height = 540;
        cfg.panel_width = 960;
        cfg.pin_cs = GPIO_NUM_15;
        cfg.pin_rst = GPIO_NUM_23;
        cfg.pin_busy = GPIO_NUM_27;
        cfg.offset_rotation = 3;
        g_parts->panel.config(cfg);
    }
    {
        auto cfg = g_parts->touch.config();
        cfg.pin_int = GPIO_NUM_36;
        cfg.pin_sda = GPIO_NUM_21;
        cfg.pin_scl = GPIO_NUM_22;
        cfg.i2c_port = I2C_NUM_1;
        cfg.freq = 400000;
        cfg.x_min = 0;
        cfg.x_max = 539;
        cfg.y_min = 0;
        cfg.y_max = 959;
        cfg.offset_rotation = 1;
        cfg.bus_shared = false;
        g_parts->touch.config(cfg);
        g_parts->panel.touch(&g_parts->touch);
    }

    auto& d = M5.Display;
    d.setPanel(&g_parts->panel);
    d.setBaseColor(TFT_WHITE);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    if (!g_parts->panel.init(false)) return false;  // init(false): no reset pulse
    d.startWrite();
    d.invertDisplay(g_parts->panel.getInvert());
    d.setColorDepth(g_parts->panel.getWriteDepth());
    d.setRotation(g_parts->panel.getRotation());
    d.setPivot(d.width() >> 1, d.height() >> 1);
    d.endWrite();
    d.setBrightness(d.getBrightness());
    const bool touchOk = g_parts->panel.initTouch();
    boot::log("touch_init", touchOk ? "ok" : "fail");
    // From here M5GFX::init_impl returns early, so M5.begin() keeps this display.
    BoardAccess::set(d, lgfx::board_t::board_M5Paper);
    return true;
}
}  // namespace

uint32_t elapsedMs() { return (micros() - g_t0Us) / 1000; }
Start startMode() { return g_start; }
// The IT8951 loses power on a hardware reset (EN: reset button, USB RTS) and takes ~0.85 s to boot
// with an empty image memory. After a software restart it just keeps running.
bool controllerWasCold() { return g_start == Start::NoReset && g_readyWaitMs > 100; }

void log(const char* key, const char* value) { Serial.printf("BOOT: %s=%s\n", key, value); }
void log(const char* key, long value) { Serial.printf("BOOT: %s=%ld\n", key, value); }

void mark(const char* stage) { Serial.printf("BOOT: t_%s_ms=%u\n", stage, (unsigned)elapsedMs()); }

void begin() {
    Serial.begin(115200);
    g_t0Us = micros();
    mark("pre_begin");

    auto cfg = M5.config();
    cfg.serial_baudrate = 0;    // Serial is started above
    cfg.clear_display = false;  // keep the frozen screen across resets
    cfg.output_power = false;   // 5 V to the external port
    cfg.internal_imu = false;   // no IMU on M5Paper
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.external_imu = false;
    cfg.external_rtc = false;
    cfg.led_brightness = 0;
    cfg.external_display_value = 0;  // no module/unit displays

    const uint32_t d0 = micros();
    bool noReset = false;
#ifndef BOOT_FORCE_STOCK
    noReset = attachNoResetDisplay();
#else
    Serial.println("BOOT: forced_stock=1");
#endif
    if (!noReset) {
        // Fallback: drop the half-built parts, then the stock autodetect runs with the IT8951
        // reset pulse and its ~0.8 s recovery.
        M5.Display.setPanel(nullptr);
        if (g_parts) g_parts->bus.release();
        delete g_parts;
        g_parts = nullptr;
        M5.Display.init_without_reset(false);
    }
    const uint32_t d1 = micros();
    g_start = noReset ? Start::NoReset : Start::Stock;
    M5.begin(cfg);
    const uint32_t d2 = micros();

    log("start", noReset ? "noreset" : "stock");
    if (noReset) {
        log("controller", controllerWasCold() ? "cold" : "warm");
        log("controller_ready_wait_ms", (long)g_readyWaitMs);
    }
    Serial.printf("BOOT: display_start_ms=%u\n", (unsigned)((d1 - d0) / 1000));
    Serial.printf("BOOT: m5_begin_rest_ms=%u\n", (unsigned)((d2 - d1) / 1000));
    Serial.printf("BOOT: m5_begin_total_ms=%u\n", (unsigned)((d2 - d0) / 1000));
    mark("post_begin");
}

void logHealth() {
    log("reset_reason", resetReasonName(esp_reset_reason()));
    log("battery_mv", (long)M5.Power.getBatteryVoltage());
}

}  // namespace boot
