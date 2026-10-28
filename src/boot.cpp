#include "boot.h"

#include <M5Unified.h>
#include <esp_system.h>
#include <soc/timer_group_struct.h>
#include <esp_task_wdt.h>

#include "crash_guard.h"

#include <esp_sleep.h>

#include "lgfx/v1/panel/Panel_IT8951.hpp"
#include "lgfx/v1/touch/Touch_GT911.hpp"

// Build with -DBOOT_WDT_S=<seconds> to change the setup watchdog timeout (default 15).
// Build with -DBOOT_FORCE_STOCK to exercise the fallback path (stock autodetect with reset).

namespace boot {

namespace {
uint32_t g_t0Us = 0;
Start g_start = Start::Stock;
uint32_t g_readyWaitMs = 0;  // how long BUSY was low at start-up (controller booting)
bool g_recoveredCold = false;  // we reset the controller ourselves: its image memory is garbage

#ifndef BOOT_WDT_S
#define BOOT_WDT_S 15
#endif
constexpr uint32_t kWatchdogS = BOOT_WDT_S;
constexpr uint32_t kReadyWaitMs = 300;  // BUSY should be high between commands
constexpr uint16_t kCmdMemBstEnd = 0x0015;  // IT8951 MEM_BST_END (Panel_IT8951.inl:54)

// Crash counter in RTC memory: survives watchdog resets, panics and ESP.restart(); garbage after
// power-on (screen_track::crashGuard checks the magic).
RTC_NOINIT_ATTR screen_track::CrashState g_crashState;
// Set just before the give-up deep sleep; a wake-up from that sleep then starts with the stock
// fallback instead of the normal path.
constexpr uint32_t kRetryMagic = 0x7E7B0071u;
RTC_NOINIT_ATTR uint32_t g_retryMagic;
constexpr int kPinRockerPush = 38;  // BtnB: external pull-up, low when pressed
screen_track::BootMode g_bootMode = screen_track::BootMode::Normal;
uint32_t g_crashCount = 0;

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
    pinMode(kPinEpdBusy, INPUT_PULLUP);
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
    // The library's own waits last up to 4 s each; check BUSY first and, if the controller is
    // stuck, let init() pulse the reset line (the image memory is then garbage: cold).
    bool useReset = false;
    if (!boot::waitBusyHigh(kReadyWaitMs, "pre_init")) {
        useReset = true;
        g_recoveredCold = true;
    }
    if (!g_parts->panel.init(useReset)) return false;  // init(false): no reset pulse
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
bool controllerWasCold() {
    return g_start == Start::NoReset && (g_readyWaitMs > 100 || g_recoveredCold);
}
bool fallbackActive() { return g_bootMode == screen_track::BootMode::StockFallback; }
void feed() { esp_task_wdt_reset(); }

bool waitBusyHigh(uint32_t timeoutMs, const char* what) {
    pinMode(kPinEpdBusy, INPUT_PULLUP);  // keep the pull-up Panel_IT8951::init() set
    const uint32_t t = millis();
    while (digitalRead(kPinEpdBusy) == LOW) {
        if (millis() - t >= timeoutMs) {
            log("busy_timeout", what);
            return false;
        }
        delay(1);
    }
    return true;
}

bool endBurstRead() {
    if (M5.getBoard() != m5::board_t::board_M5Paper) return false;
    auto* p = static_cast<lgfx::Panel_IT8951*>(M5.Display.getPanel());
    if (!p || !p->getBus() || !waitBusyHigh(kReadyWaitMs, "bst_end")) return false;
    // Raw command like probeController, with the panel's own conventions: inside a transaction CS
    // is low only while sending; endWrite() releases CS and the bus as Panel_IT8951 expects.
    auto* bus = p->getBus();
    p->startWrite();
    bus->wait();
    digitalWrite(kPinEpdCs, HIGH);
    bool ok = waitBusyHigh(kReadyWaitMs, "bst_end_cs");
    if (ok) {
        digitalWrite(kPinEpdCs, LOW);
        bus->writeData(__builtin_bswap16(0x6000) | (uint32_t)__builtin_bswap16(kCmdMemBstEnd) << 16, 32);
        bus->wait();
    }
    digitalWrite(kPinEpdCs, HIGH);
    p->endWrite();
    return ok && waitBusyHigh(kReadyWaitMs, "after_bst_end");
}

bool ensureControllerReady(const char* where) {
    if (waitBusyHigh(kReadyWaitMs, where)) return true;
    // Stuck: hardware reset of the controller and a full panel init (rotation is restored by it).
    log("controller_recover", where);
    auto* p = static_cast<lgfx::Panel_IT8951*>(M5.Display.getPanel());
    if (p) p->init(true);
    g_recoveredCold = true;
    feed();
    return false;
}

void log(const char* key, const char* value) { Serial.printf("BOOT: %s=%s\n", key, value); }
void log(const char* key, long value) { Serial.printf("BOOT: %s=%ld\n", key, value); }

void mark(const char* stage) {
    feed();
    Serial.printf("BOOT: t_%s_ms=%u\n", stage, (unsigned)elapsedMs());
}

void setupDone() {
    // Setup finished: the next reset is not a crash loop, and later code must not be killed by
    // a watchdog that only guards the start-up path.
    esp_task_wdt_delete(NULL);
    g_crashState = screen_track::crashGuard(g_crashState, false).next;
}

void begin() {
    // 4 KB receive buffer: the putfile transfer (task 020) lets the host send 1 KB between ACKs.
    const size_t rxSize = Serial.setRxBufferSize(4096);  // must precede begin()
    Serial.begin(115200);
    Serial.printf("BOOT: serial_rx_buffer=%u\n", (unsigned)rxSize);
    g_t0Us = micros();
    // Crash counter first: decides between the normal and the fallback start-up, and whether to
    // give up. A watchdog reset or a panic counts; anything else (power-on, button, software
    // restart, deep sleep) starts over.
    const esp_reset_reason_t rr = esp_reset_reason();
    const bool crashed = rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_WDT ||
                         rr == ESP_RST_PANIC;
    // Woken from the give-up sleep by the rocker: retry, starting with the stock fallback (count 1
    // plus this crash = kFallbackAfter).
    const bool retry = rr == ESP_RST_DEEPSLEEP && g_retryMagic == kRetryMagic;
    g_retryMagic = 0;
    if (retry) {
        g_crashState.magic = screen_track::kCrashMagic;
        g_crashState.count = screen_track::kFallbackAfter - 1;
    }
    const auto cd = screen_track::crashGuard(g_crashState, crashed || retry);
    g_crashState = cd.next;
    g_bootMode = cd.mode;
    g_crashCount = cd.next.count;
    // Arm the watchdog for this task (the loop task): a hang anywhere in setup ends in a reset.
    const esp_err_t wdtRc = esp_task_wdt_init(kWatchdogS, true);  // already running: updates the timeout
    esp_task_wdt_add(NULL);
    mark("pre_begin");
    log("reset_reason_early", resetReasonName(rr));
    log("crash_count", (long)g_crashCount);
    log("wdt_timeout_s", (long)kWatchdogS);
    log("wdt_init_rc", (long)wdtRc);
    // The timeout actually programmed into the hardware watchdog (MWDT0 stage 0): ticks x prescale
    // / 80 kHz-per-ms, so that "15000" confirms the 15 s timeout is in effect.
    log("wdt_effective_ms",
        (long)((uint64_t)TIMERG0.wdt_config2 * (uint64_t)TIMERG0.wdt_config1.clk_prescale / 80000ull));
    if (g_bootMode == screen_track::BootMode::GiveUp) {
        // Even the stock start-up keeps crashing. Do not loop: switch the device off. (Pressing
        // the power button starts it again with a clean counter.)
        log("give_up", 1);
        Serial.flush();
        esp_task_wdt_delete(NULL);
        g_crashState = screen_track::CrashState();
        g_retryMagic = kRetryMagic;
        pinMode(kPinPwr, OUTPUT);
        digitalWrite(kPinPwr, LOW);
        // On battery the power is gone now. On USB the device stays up: the rocker push wakes it.
        esp_sleep_enable_ext0_wakeup((gpio_num_t)kPinRockerPush, 0);
        delay(1000);
        esp_deep_sleep_start();
    }
    if (g_bootMode == screen_track::BootMode::StockFallback) log("boot_mode", "stock_fallback");

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
    if (g_bootMode == screen_track::BootMode::Normal) noReset = attachNoResetDisplay();
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
