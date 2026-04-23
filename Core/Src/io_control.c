#include "io_control.h"
#include "usbd_custom_hid_if.h"
#include "usbd_customhid.h"
#include <string.h>

extern ADC_HandleTypeDef hadc;
extern USBD_HandleTypeDef hUsbDeviceFS;

static KeyboardReport_t kb_report;
static MouseReport_t mouse_report;
static GamepadReport_t gp_report;

static KeyboardReport_t prev_kb_report;
static GamepadReport_t prev_gp_report;
static uint8_t prev_mouse_buttons;

/* Non-blocking send: if endpoint is busy this frame, caller retries next loop iteration */
static int8_t USB_SendReport(uint8_t *report, uint16_t len) {
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    USBD_CUSTOM_HID_HandleTypeDef *hhid = (USBD_CUSTOM_HID_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (!hhid) return USBD_FAIL;
    if (hhid->state != CUSTOM_HID_IDLE) return USBD_BUSY;
    return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, len);
}

typedef enum {
    TYPE_NONE,
    TYPE_KEYBOARD,
    TYPE_GAMEPAD,
    TYPE_MODIFIER,
    TYPE_FN
} KeyType_t;

typedef struct {
    KeyType_t type;
    uint8_t value;
} KeyMap_t;

// HID Modifiers
#define MOD_LCTRL  0x01
#define MOD_LSHIFT 0x02
#define MOD_LALT   0x04
#define MOD_LGUI   0x08
#define MOD_RCTRL  0x10
#define MOD_RSHIFT 0x20
#define MOD_RALT   0x40
#define MOD_RGUI   0x80

// Gamepad Buttons (Bit indices)
#define GP_L1      0
#define GP_R1      1
#define GP_L2      2
#define GP_R2      3
#define GP_L3      4
#define GP_R3      5
#define GP_SELECT  6
#define GP_START   7
#define GP_A       8
#define GP_B       9
#define GP_X       10
#define GP_Y       11
#define GP_UP      12
#define GP_DOWN    13
#define GP_LEFT    14
#define GP_RIGHT   15

static const KeyMap_t keymap[NUM_COLUMNS][NUM_ROWS] = {
    {{TYPE_GAMEPAD, GP_L1}, {TYPE_KEYBOARD, 0x29}, {TYPE_KEYBOARD, 0x35}, {TYPE_KEYBOARD, 0x2B}, {TYPE_KEYBOARD, 0x39}, {TYPE_MODIFIER, MOD_LSHIFT}, {TYPE_MODIFIER, MOD_LCTRL}}, // C0
    {{TYPE_GAMEPAD, GP_L3}, {TYPE_KEYBOARD, 0x3A}, {TYPE_KEYBOARD, 0x1E}, {TYPE_KEYBOARD, 0x14}, {TYPE_KEYBOARD, 0x04}, {TYPE_KEYBOARD, 0x1D}, {TYPE_MODIFIER, MOD_LGUI}}, // C1
    {{TYPE_GAMEPAD, GP_LEFT}, {TYPE_KEYBOARD, 0x3B}, {TYPE_KEYBOARD, 0x1F}, {TYPE_KEYBOARD, 0x1A}, {TYPE_KEYBOARD, 0x16}, {TYPE_KEYBOARD, 0x1B}, {TYPE_MODIFIER, MOD_LALT}}, // C2
    {{TYPE_GAMEPAD, GP_UP}, {TYPE_KEYBOARD, 0x3C}, {TYPE_KEYBOARD, 0x20}, {TYPE_KEYBOARD, 0x08}, {TYPE_KEYBOARD, 0x07}, {TYPE_KEYBOARD, 0x06}, {TYPE_KEYBOARD, 0x2C}}, // C3
    {{TYPE_GAMEPAD, GP_DOWN}, {TYPE_KEYBOARD, 0x3D}, {TYPE_KEYBOARD, 0x21}, {TYPE_KEYBOARD, 0x15}, {TYPE_KEYBOARD, 0x09}, {TYPE_KEYBOARD, 0x19}, {TYPE_MODIFIER, MOD_RALT}}, // C4
    {{TYPE_GAMEPAD, GP_RIGHT}, {TYPE_KEYBOARD, 0x3E}, {TYPE_KEYBOARD, 0x22}, {TYPE_KEYBOARD, 0x17}, {TYPE_KEYBOARD, 0x0A}, {TYPE_KEYBOARD, 0x05}, {TYPE_MODIFIER, MOD_RGUI}}, // C5
    {{TYPE_GAMEPAD, GP_SELECT}, {TYPE_KEYBOARD, 0x3F}, {TYPE_KEYBOARD, 0x23}, {TYPE_KEYBOARD, 0x1C}, {TYPE_KEYBOARD, 0x0B}, {TYPE_KEYBOARD, 0x11}, {TYPE_KEYBOARD, 0x65}}, // C6
    {{TYPE_GAMEPAD, GP_START}, {TYPE_KEYBOARD, 0x40}, {TYPE_KEYBOARD, 0x24}, {TYPE_KEYBOARD, 0x18}, {TYPE_KEYBOARD, 0x0D}, {TYPE_KEYBOARD, 0x10}, {TYPE_MODIFIER, MOD_RCTRL}}, // C7
    {{TYPE_GAMEPAD, GP_X}, {TYPE_KEYBOARD, 0x41}, {TYPE_KEYBOARD, 0x25}, {TYPE_KEYBOARD, 0x0C}, {TYPE_KEYBOARD, 0x0E}, {TYPE_KEYBOARD, 0x36}, {TYPE_FN, 0}}, // C8
    {{TYPE_GAMEPAD, GP_Y}, {TYPE_KEYBOARD, 0x42}, {TYPE_KEYBOARD, 0x26}, {TYPE_KEYBOARD, 0x12}, {TYPE_KEYBOARD, 0x0F}, {TYPE_KEYBOARD, 0x37}, {TYPE_KEYBOARD, 0x50}}, // C9
    {{TYPE_GAMEPAD, GP_A}, {TYPE_KEYBOARD, 0x43}, {TYPE_KEYBOARD, 0x27}, {TYPE_KEYBOARD, 0x13}, {TYPE_KEYBOARD, 0x33}, {TYPE_KEYBOARD, 0x38}, {TYPE_KEYBOARD, 0x51}}, // C10
    {{TYPE_GAMEPAD, GP_B}, {TYPE_KEYBOARD, 0x44}, {TYPE_KEYBOARD, 0x2D}, {TYPE_KEYBOARD, 0x2F}, {TYPE_KEYBOARD, 0x34}, {TYPE_MODIFIER, MOD_RSHIFT}, {TYPE_KEYBOARD, 0x4F}}, // C11
    {{TYPE_GAMEPAD, GP_R3}, {TYPE_KEYBOARD, 0x45}, {TYPE_KEYBOARD, 0x2E}, {TYPE_KEYBOARD, 0x30}, {TYPE_KEYBOARD, 0x28}, {TYPE_KEYBOARD, 0x52}, {TYPE_KEYBOARD, 0x4B}}, // C12
    {{TYPE_GAMEPAD, GP_R1}, {TYPE_KEYBOARD, 0x4C}, {TYPE_KEYBOARD, 0x2A}, {TYPE_KEYBOARD, 0x31}, {TYPE_KEYBOARD, 0x4A}, {TYPE_KEYBOARD, 0x4D}, {TYPE_KEYBOARD, 0x4E}}, // C13
};

static GPIO_TypeDef* const column_ports[NUM_COLUMNS] = {
    GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB,
    GPIOD, GPIOC, GPIOC, GPIOC, GPIOA, GPIOB, GPIOA
};

static const uint16_t column_pins[NUM_COLUMNS] = {
    GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_3,
    GPIO_PIN_2, GPIO_PIN_12, GPIO_PIN_11, GPIO_PIN_10, GPIO_PIN_15, GPIO_PIN_11, GPIO_PIN_6
};

/* Row pins (R0=PA7, R1=PC4, R2=PC5, R3=PB0, R4=PB1, R5=PB2, R6=PB10)
   Read all rows via 3 IDR register reads per column scan. */

/* Debounce: mechanical switches bounce 5-20ms. Accept a state change only
   after the raw reading has been stable for DEBOUNCE_MS consecutive ms.
   Per-column bitmask — one bit per row (bits 0-6). */
#define DEBOUNCE_MS 8
static uint8_t  db_state[NUM_COLUMNS]; // last accepted (debounced) bitmask
static uint8_t  db_raw  [NUM_COLUMNS]; // last raw bitmask
static uint32_t db_time [NUM_COLUMNS]; // tick when db_raw last changed

static __attribute__((always_inline)) inline
void apply_key(int c, int r, uint8_t *key_count, uint8_t *fn_pressed) {
    const KeyMap_t *k = &keymap[c][r];
    switch (k->type) {
        case TYPE_KEYBOARD: if (*key_count < 6) kb_report.keycodes[(*key_count)++] = k->value; break;
        case TYPE_MODIFIER: kb_report.modifiers  |= k->value; break;
        case TYPE_GAMEPAD:  gp_report.buttons    |= (1u << k->value); break;
        case TYPE_FN:       *fn_pressed = 1; break;
        default: break;
    }
}

void IO_Control_Init(void) {
    memset(&kb_report, 0, sizeof(kb_report));
    kb_report.report_id = 1;
    memset(&mouse_report, 0, sizeof(mouse_report));
    mouse_report.report_id = 2;
    memset(&gp_report, 0, sizeof(gp_report));
    gp_report.report_id = 3;
    gp_report.lx = gp_report.ly = gp_report.rx = gp_report.ry = 127;
    
    memcpy(&prev_kb_report, &kb_report, sizeof(kb_report));
    memcpy(&prev_gp_report, &gp_report, sizeof(gp_report));
    prev_mouse_buttons = 0;
    memset(db_state, 0, sizeof(db_state));
    memset(db_raw,   0, sizeof(db_raw));
    memset(db_time,  0, sizeof(db_time));
}

static uint8_t adc_values[6];

void Read_ADC(void) {
    HAL_ADC_Start(&hadc);
    for (int i = 0; i < 6; i++) {
        if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
            adc_values[i] = HAL_ADC_GetValue(&hadc) >> 4;
        }
    }
    HAL_ADC_Stop(&hadc);
}

void IO_Control_Process(void) {
    uint8_t mode = HAL_GPIO_ReadPin(Mode_Switch_GPIO_Port, Mode_Switch_Pin);
    HAL_GPIO_WritePin(Mode_GPIO_Port, Mode_Pin, mode == GPIO_PIN_RESET ? GPIO_PIN_SET : GPIO_PIN_RESET);

    memset(kb_report.keycodes, 0, 6);
    kb_report.modifiers = 0;
    gp_report.buttons = 0;
    uint8_t key_count = 0;
    uint8_t fn_pressed = 0;

    uint32_t now = HAL_GetTick();

    for (int c = 0; c < NUM_COLUMNS; c++) {
        GPIO_TypeDef *port = column_ports[c];
        uint16_t      pin  = column_pins[c];

        port->BSRR = pin;
        /* 8 NOPs = ~167ns at 48MHz — enough for diode+switch+trace RC to settle */
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();

        /* 3 IDR reads capture all 7 rows simultaneously */
        uint32_t idrA = GPIOA->IDR;
        uint32_t idrB = GPIOB->IDR;
        uint32_t idrC = GPIOC->IDR;

        port->BRR = pin;

        /* Pack raw row states into a bitmask (bit i = row i) */
        uint8_t raw = 0;
        if (idrA & GPIO_PIN_7)  raw |= (1 << 0);
        if (idrC & GPIO_PIN_4)  raw |= (1 << 1);
        if (idrC & GPIO_PIN_5)  raw |= (1 << 2);
        if (idrB & GPIO_PIN_0)  raw |= (1 << 3);
        if (idrB & GPIO_PIN_1)  raw |= (1 << 4);
        if (idrB & GPIO_PIN_2)  raw |= (1 << 5);
        if (idrB & GPIO_PIN_10) raw |= (1 << 6);

        /* Debounce: only accept raw when it has been stable for DEBOUNCE_MS */
        if (raw != db_raw[c]) {
            db_raw[c]  = raw;
            db_time[c] = now;
        }
        if (db_raw[c] != db_state[c] && (now - db_time[c]) >= DEBOUNCE_MS) {
            db_state[c] = db_raw[c];
        }

        /* Apply the debounced state to reports */
        uint8_t bits = db_state[c];
        if (bits & (1 << 0)) apply_key(c, 0, &key_count, &fn_pressed);
        if (bits & (1 << 1)) apply_key(c, 1, &key_count, &fn_pressed);
        if (bits & (1 << 2)) apply_key(c, 2, &key_count, &fn_pressed);
        if (bits & (1 << 3)) apply_key(c, 3, &key_count, &fn_pressed);
        if (bits & (1 << 4)) apply_key(c, 4, &key_count, &fn_pressed);
        if (bits & (1 << 5)) apply_key(c, 5, &key_count, &fn_pressed);
        if (bits & (1 << 6)) apply_key(c, 6, &key_count, &fn_pressed);
    }

    if (fn_pressed) {
        for (int i = 0; i < 6; i++) {
            if (kb_report.keycodes[i] == 0x0C) kb_report.keycodes[i] = 0x46; // PrintScreen
            if (kb_report.keycodes[i] == 0x12) kb_report.keycodes[i] = 0x47; // Scroll Lock
        }
    }

    /* --- KEYBOARD: send immediately after matrix scan, before any ADC blocking --- */
    if (memcmp(&kb_report, &prev_kb_report, sizeof(kb_report)) != 0) {
        if (USB_SendReport((uint8_t*)&kb_report, sizeof(kb_report)) == USBD_OK) {
            memcpy(&prev_kb_report, &kb_report, sizeof(kb_report));
        }
    }

    /* --- ANALOG: rate-limited to 8ms (125 Hz). ADC blocks ~29us per call so
       keeping it off the keyboard path eliminates the main latency source. --- */
    static uint32_t last_adc_ms = 0;
    if (now - last_adc_ms >= 8) {
        last_adc_ms = now;

        Read_ADC();
        gp_report.l2 = adc_values[0];
        gp_report.lx = adc_values[1];
        gp_report.ly = 255 - adc_values[2]; // Y-axis inverted
        gp_report.ry = 255 - adc_values[3]; // Y-axis inverted
        gp_report.rx = adc_values[4];
        gp_report.r2 = adc_values[5];

        if (mode == GPIO_PIN_SET) { // Gamepad Mode
            if (memcmp(&gp_report, &prev_gp_report, sizeof(gp_report)) != 0) {
                if (USB_SendReport((uint8_t*)&gp_report, sizeof(gp_report)) == USBD_OK) {
                    memcpy(&prev_gp_report, &gp_report, sizeof(gp_report));
                }
            }
        } else { // Mouse Mode
            uint8_t mouse_buttons = 0;

            /* R1 (digital matrix button) -> left click */
            if (gp_report.buttons & (1 << GP_R1)) {
                mouse_buttons |= 0x01;
            }
            /* R2 analog trigger (PA5, Hall sensor) -> right click.
               Threshold: deviation >64 from center(128) handles both sensor polarities. */
            if (adc_values[5] > 192 || adc_values[5] < 64) {
                mouse_buttons |= 0x02;
            }
            /* L3 (left stick click) -> middle button / wheel click */
            if (gp_report.buttons & (1 << GP_L3)) {
                mouse_buttons |= 0x04;
            }

            mouse_report.buttons = mouse_buttons;

            int8_t mx    = (int8_t)((int16_t)adc_values[4] - 128);
            int8_t my    = (int8_t)(128 - (int16_t)adc_values[3]); // Y-axis inverted
            int8_t wheel = (int8_t)((int16_t)adc_values[2] - 128); // direction: push up = scroll up

            /* Deadzone ±25/128: covers joystick mechanical center variation and ADC noise */
            if (mx    >= -25 && mx    <= 25) mx    = 0;
            if (my    >= -25 && my    <= 25) my    = 0;
            if (wheel >= -25 && wheel <= 25) wheel = 0;

            mouse_report.x     = mx    / 8;
            mouse_report.y     = my    / 8;
            mouse_report.wheel = wheel / 32;

            /* Send on movement OR button state change */
            if (mouse_report.x != 0 || mouse_report.y != 0 || mouse_report.wheel != 0 ||
                mouse_report.buttons != prev_mouse_buttons) {
                USB_SendReport((uint8_t*)&mouse_report, sizeof(mouse_report));
                prev_mouse_buttons = mouse_report.buttons;
            }
        }
    }
}
