#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <string.h>
#include <stdio.h>

// ─── Pins ──────────────────────────────────────────────────────────
#define BTN_RIGHT    GPIO_NUM_0
#define BTN_MIDDLE   GPIO_NUM_1
#define BTN_LEFT     GPIO_NUM_2
#define BUZZER_PIN   GPIO_NUM_10
#define SDA_PIN      GPIO_NUM_6
#define SCL_PIN      GPIO_NUM_7
#define MOTOR_INPUT1 GPIO_NUM_9
#define MOTOR_INPUT2 GPIO_NUM_8

// ─── I2C / OLED ────────────────────────────────────────────────────
#define I2C_PORT     I2C_NUM_0
#define OLED_ADDR    0x3C
#define OLED_WIDTH   128
#define OLED_PAGES   8       // 64px height / 8px per page

static uint8_t framebuf[OLED_WIDTH * OLED_PAGES];  // 1 bit per pixel

// ── Raw I2C helpers ────────────────────────────────────────────────
static void i2c_init() {
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SDA_PIN,
        .scl_io_num       = SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 400000 },
        .clk_flags        = 0
    };
    i2c_param_config(I2C_PORT, &cfg);
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

static void oled_cmd(uint8_t cmd) {
    uint8_t buf[2] = { 0x00, cmd };
    i2c_master_write_to_device(I2C_PORT, OLED_ADDR, buf, 2, pdMS_TO_TICKS(10));
}

static void oled_init() {
    vTaskDelay(pdMS_TO_TICKS(100));
    const uint8_t cmds[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock divide
        0xA8, 0x3F, // multiplex ratio (64-1)
        0xD3, 0x00, // display offset
        0x40,       // start line
        0x8D, 0x14, // charge pump on
        0x20, 0x00, // horizontal addressing
        0xA1,       // segment remap
        0xC8,       // com scan direction
        0xDA, 0x12, // com pins
        0x81, 0xCF, // contrast
        0xD9, 0xF1, // precharge
        0xDB, 0x40, // vcomh
        0xA4,       // output follows RAM
        0xA6,       // normal (not inverted)
        0xAF        // display on
    };
    for (size_t i = 0; i < sizeof(cmds); i++) oled_cmd(cmds[i]);
}

static void oled_flush() {
    // Set full address window
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);  // columns 0-127
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);    // pages 0-7

    // Send framebuffer in 16-byte chunks
    uint8_t chunk[17];
    chunk[0] = 0x40;   // data mode
    for (int i = 0; i < OLED_WIDTH * OLED_PAGES; i += 16) {
        memcpy(&chunk[1], &framebuf[i], 16);
        i2c_master_write_to_device(I2C_PORT, OLED_ADDR, chunk, 17, pdMS_TO_TICKS(10));
    }
}

// ─── Drawing primitives ────────────────────────────────────────────
static void fb_clear() {
    memset(framebuf, 0, sizeof(framebuf));
}

static void fb_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= 64) return;
    int page = y / 8;
    int bit  = y % 8;
    if (on) framebuf[page * OLED_WIDTH + x] |=  (1 << bit);
    else    framebuf[page * OLED_WIDTH + x] &= ~(1 << bit);
}

static void fb_rect(int x, int y, int w, int h, bool filled) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            if (filled || row == y || row == y+h-1 || col == x || col == x+w-1)
                fb_pixel(col, row, true);
}

// 5x7 font — ASCII 32-127
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x5F,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14}, // #
    {0x24,0x2A,0x7F,0x2A,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00}, // )
    {0x14,0x08,0x3E,0x08,0x14}, // *
    {0x08,0x08,0x3E,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x08,0x14,0x22,0x41,0x00}, // 
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x00,0x41,0x22,0x14,0x08}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3E}, // @
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x7F,0x41,0x41,0x00}, // [
    {0x02,0x04,0x08,0x10,0x20}, // backslash
    {0x00,0x41,0x41,0x7F,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04}, // ^
    {0x40,0x40,0x40,0x40,0x40}, // _
    {0x00,0x01,0x02,0x04,0x00}, // `
    {0x20,0x54,0x54,0x54,0x78}, // a
    {0x7F,0x48,0x44,0x44,0x38}, // b
    {0x38,0x44,0x44,0x44,0x20}, // c
    {0x38,0x44,0x44,0x48,0x7F}, // d
    {0x38,0x54,0x54,0x54,0x18}, // e
    {0x08,0x7E,0x09,0x01,0x02}, // f
    {0x0C,0x52,0x52,0x52,0x3E}, // g
    {0x7F,0x08,0x04,0x04,0x78}, // h
    {0x00,0x44,0x7D,0x40,0x00}, // i
    {0x20,0x40,0x44,0x3D,0x00}, // j
    {0x7F,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7F,0x40,0x00}, // l
    {0x7C,0x04,0x18,0x04,0x78}, // m
    {0x7C,0x08,0x04,0x04,0x78}, // n
    {0x38,0x44,0x44,0x44,0x38}, // o
    {0x7C,0x14,0x14,0x14,0x08}, // p
    {0x08,0x14,0x14,0x18,0x7C}, // q
    {0x7C,0x08,0x04,0x04,0x08}, // r
    {0x48,0x54,0x54,0x54,0x20}, // s
    {0x04,0x3F,0x44,0x40,0x20}, // t
    {0x3C,0x40,0x40,0x40,0x3C}, // u
    {0x1C,0x20,0x40,0x20,0x1C}, // v
    {0x3C,0x40,0x30,0x40,0x3C}, // w
    {0x44,0x28,0x10,0x28,0x44}, // x
    {0x0C,0x50,0x50,0x50,0x3C}, // y
    {0x44,0x64,0x54,0x4C,0x44}, // z
};

static void fb_char(int x, int y, char c) {
    if (c < 32 || c > 122) return;
    const uint8_t* glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++)
            fb_pixel(x + col, y + row, (bits >> row) & 1);
    }
}

static void fb_str(int x, int y, const char* s) {
    while (*s) { fb_char(x, y, *s++); x += 6; }
}

static void fb_bar(int x, int y, int w, int h, int value) {
    fb_rect(x, y, w, h, false);                        // outline
    int fill = (value * (w - 2)) / 100;
    if (fill > 0) fb_rect(x + 1, y + 1, fill, h - 2, true);  // fill
}

// ─── Sprites (16x16) ───────────────────────────────────────────────
// Each sprite: 16 rows, each row is a uint16_t bitmask (bit15 = leftmost pixel)
static const uint16_t sprHappy[16] = {
    0x0000, 0x1FF0, 0x2008, 0x4004,
    0x4CD2, 0x4CD2, 0x4002, 0x4002,
    0x4222, 0x41C2, 0x4002, 0x2008,
    0x1FF0, 0x0000, 0x0000, 0x0000
};
static const uint16_t sprSad[16] = {
    0x0000, 0x1FF0, 0x2008, 0x4004,
    0x4CD2, 0x4CD2, 0x4002, 0x4002,
    0x4002, 0x41C2, 0x4222, 0x2008,
    0x1FF0, 0x0000, 0x0000, 0x0000
};
static const uint16_t sprNeutral[16] = {
    0x0000, 0x1FF0, 0x2008, 0x4004,
    0x4CD2, 0x4CD2, 0x4002, 0x4002,
    0x43E2, 0x4002, 0x4002, 0x2008,
    0x1FF0, 0x0000, 0x0000, 0x0000
};

static void fb_sprite(int x, int y, const uint16_t* spr) {
    for (int row = 0; row < 16; row++)
        for (int col = 0; col < 16; col++)
            fb_pixel(x + col, y + row, (spr[row] >> (15 - col)) & 1);
}

// ─── App State ─────────────────────────────────────────────────────
enum AppMode { MODE_TIMER, MODE_PET };
static AppMode currentMode = MODE_TIMER;

struct Pet { int happiness; unsigned long age; };
static Pet pet = { 80, 0 };

enum Screen { SCREEN_MAIN, SCREEN_PLAY };
static Screen currentScreen = SCREEN_MAIN;

static bool canFeed      = false;
static bool motorRunning = false;

// Timer
static int  timerMinutes = 0;
static int  timerSeconds = 0;
static int initialTimerMinutes = 0;
static bool timerRunning = false;
static bool timerDone    = false;
static bool study_session_active = false;
static unsigned long lastTimerTick = 0;
static unsigned long lastPetUpdate = 0;

// Button tracking
static unsigned long lastButtonPress   = 0;
static unsigned long btnRightHoldStart = 0;
static bool          btnRightHeld      = false;

// ─── Helpers ───────────────────────────────────────────────────────
static unsigned long millis_now() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

static void playTone(int freq_hz, int duration_ms) {
    int half_us = 1000000 / (freq_hz * 2);
    int cycles  = (freq_hz * duration_ms) / 1000;
    for (int i = 0; i < cycles; i++) {
        gpio_set_level(BUZZER_PIN, 1);
        esp_rom_delay_us(half_us);
        gpio_set_level(BUZZER_PIN, 0);
        esp_rom_delay_us(half_us);
    }
}

static void startMotor() {
    gpio_set_level(MOTOR_INPUT1, 1);
    gpio_set_level(MOTOR_INPUT2, 0);
    motorRunning = true;
}

static void stopMotor() {
    gpio_set_level(MOTOR_INPUT1, 0);
    gpio_set_level(MOTOR_INPUT2, 0);
    motorRunning = false;
}

// ─── Timer logic ───────────────────────────────────────────────────
static void onTimerComplete() {
    canFeed      = true;
    timerDone    = true;
    timerRunning = false;
    playTone(1000, 500);
    startMotor();

    if (study_session_active) {
        // If it was a study session, give extra happiness boost
        pet.happiness += 30;
        if (pet.happiness > 100) pet.happiness = 100;
    }
}

static void updateTimer() {
    if (!timerRunning || timerDone) return;
    if (millis_now() - lastTimerTick >= 1000) {
        lastTimerTick = millis_now();
        if (timerSeconds > 0) {
            timerSeconds--;
        } else if (timerMinutes > 0) {
            timerMinutes--;
            timerSeconds = 59;
        } else {
            onTimerComplete();
        }
    }
}

static void renderTimer() {
    fb_clear();

    char buf[16];
    // Big MM:SS in the center
    snprintf(buf, sizeof(buf), "%02d:%02d", timerMinutes, timerSeconds);
    fb_str(34, 24, buf);   // x=34 centers 5 chars * 6px = 30px in 128px

    fb_str(0, 0, timerRunning ? "  TIMER RUNNING" : (timerDone ? "  TIMER DONE!" : "  SET TIMER"));

    fb_str(0, 56, timerDone ? "Long press: pet side" : "+M   +S   Start/Stop");

    oled_flush();
}

// ─── Pet logic ─────────────────────────────────────────────────────
static void updatePet() {
    if (millis_now() - lastPetUpdate > 10000) {
        pet.happiness--;
        if (pet.happiness < 0) pet.happiness = 0;
        pet.age += 10;
        lastPetUpdate = millis_now();
    }
}

static void handleScreenLogic() {
    if (currentScreen == SCREEN_PLAY) {
        if (canFeed) {
            pet.happiness += 20;
            if (pet.happiness > 100) pet.happiness = 100;
            canFeed = false;
            playTone(1500, 100);
        }
        currentScreen = SCREEN_MAIN;
    }
}

static void renderPet() {
    fb_clear();

    const uint16_t* spr = (pet.happiness < 30) ? sprSad
                        : (pet.happiness >= 60) ? sprHappy
                        :                         sprNeutral;
    fb_sprite(56, 1, spr);

    char buf[20];
    fb_str(0, 20, "HAP");
    fb_bar(22, 20, 80, 7, pet.happiness);

    snprintf(buf, sizeof(buf), "Age %lum", pet.age / 60);
    fb_str(0, 32, buf);

    if (canFeed)  fb_str(0, 56, "Middle=Play! (reward)");
    else          fb_str(0, 56, "Mid=Play  Long=Timer");

    oled_flush();
}

// ─── Buttons ───────────────────────────────────────────────────────
static void checkButtons() {
    unsigned long now = millis_now();

    // ── RIGHT: long press = switch mode, short press = start/stop or stop motor ──
    if (gpio_get_level(BTN_RIGHT) == 0) {
        if (btnRightHoldStart == 0) btnRightHoldStart = now;

        if (now - btnRightHoldStart > 1000 && !btnRightHeld) {
            btnRightHeld  = true;
            currentMode   = (currentMode == MODE_TIMER) ? MODE_PET : MODE_TIMER;
            playTone(900, 80);
        }
    } else {
        if (btnRightHoldStart > 0 && !btnRightHeld) {
            // Short press
            if (currentMode == MODE_TIMER && !timerDone) {
                if (!timerRunning && (timerMinutes > 0 || timerSeconds > 0)) {
                    initialTimerMinutes = timerMinutes;
                    study_session_active = (initialTimerMinutes >= 50); // Consider it a study session if 25+ mins set
                    timerRunning  = true;
                    lastTimerTick = now;
                    playTone(1200, 80);
                } else {
                    timerRunning = false;
                    playTone(600, 80);
                }
            } else if (motorRunning) {
                stopMotor();
                playTone(800, 80);
            }
        }
        btnRightHoldStart = 0;
        btnRightHeld      = false;
    }

    // Debounce for LEFT and MIDDLE
    if (now - lastButtonPress < 200) return;

    // ── LEFT + MIDDLE simultaneously: reset timer ──
    if (gpio_get_level(BTN_LEFT) == 0 && gpio_get_level(BTN_MIDDLE) == 0) {
        timerMinutes = 0; timerSeconds = 0;
        timerRunning = false; timerDone = false;
        stopMotor();
        playTone(500, 200);
        lastButtonPress = now;
        return;
    }

    // ── LEFT: +1 minute ──
    if (gpio_get_level(BTN_LEFT) == 0) {
        if (currentMode == MODE_TIMER && !timerRunning && !timerDone) {
            if (++timerMinutes > 99) timerMinutes = 99;
            playTone(1000, 40);
        }
        lastButtonPress = now;
    }
    // ── MIDDLE: +1 sec (timer) or play (pet) ──
    else if (gpio_get_level(BTN_MIDDLE) == 0) {
        if (currentMode == MODE_TIMER && !timerRunning && !timerDone) {
            if (++timerSeconds >= 60) { timerSeconds = 0; timerMinutes++; }
            playTone(1000, 40);
        } else if (currentMode == MODE_PET) {
            currentScreen = SCREEN_PLAY;
            playTone(1200, 50);
        }
        lastButtonPress = now;
    }
}

// ─── Entry point ───────────────────────────────────────────────────
extern "C" void app_main() {
    i2c_init();
    oled_init();

    // Buttons
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL<<BTN_LEFT)|(1ULL<<BTN_MIDDLE)|(1ULL<<BTN_RIGHT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);

    gpio_set_direction(BUZZER_PIN,   GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_INPUT1, GPIO_MODE_OUTPUT);
    gpio_set_direction(MOTOR_INPUT2, GPIO_MODE_OUTPUT);
    stopMotor();

    // Boot screen
    fb_clear();
    fb_str(28, 20, "Tamagotchi!");
    fb_str(16, 36, "Long press=switch");
    oled_flush();
    vTaskDelay(pdMS_TO_TICKS(1500));

    while (1) {
        checkButtons();
        if (currentMode == MODE_TIMER) {
            updateTimer();
            renderTimer();
        } else {
            updatePet();
            handleScreenLogic();
            renderPet();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}