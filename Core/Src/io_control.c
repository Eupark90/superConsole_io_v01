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

static GPIO_TypeDef* column_ports[NUM_COLUMNS] = {
    GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB,
    GPIOD, GPIOC, GPIOC, GPIOC, GPIOA, GPIOB, GPIOA
};

static uint16_t column_pins[NUM_COLUMNS] = {
    GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_3,
    GPIO_PIN_2, GPIO_PIN_12, GPIO_PIN_11, GPIO_PIN_10, GPIO_PIN_15, GPIO_PIN_11, GPIO_PIN_6
};

static GPIO_TypeDef* row_ports[NUM_ROWS] = {
    GPIOA, GPIOC, GPIOC, GPIOB, GPIOB, GPIOB, GPIOB
};

static uint16_t row_pins[NUM_ROWS] = {
    GPIO_PIN_7, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_10
};

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

    for (int c = 0; c < NUM_COLUMNS; c++) {
        /* Drive column HIGH: diode (A=column, K=switch side) forward-biases,
           allowing current to reach the row when switch is closed */
        HAL_GPIO_WritePin(column_ports[c], column_pins[c], GPIO_PIN_SET);
        /* Short settle time for signal to propagate */
        __NOP(); __NOP(); __NOP(); __NOP();
        for (int r = 0; r < NUM_ROWS; r++) {
            /* Row reads HIGH when key is pressed (pulled up through diode+switch) */
            if (HAL_GPIO_ReadPin(row_ports[r], row_pins[r]) == GPIO_PIN_SET) {
                KeyMap_t key = keymap[c][r];
                switch (key.type) {
                    case TYPE_KEYBOARD:
                        if (key_count < 6) kb_report.keycodes[key_count++] = key.value;
                        break;
                    case TYPE_MODIFIER:
                        kb_report.modifiers |= key.value;
                        break;
                    case TYPE_GAMEPAD:
                        gp_report.buttons |= (1 << key.value);
                        break;
                    case TYPE_FN:
                        fn_pressed = 1;
                        break;
                    default: break;
                }
            }
        }
        /* Restore column LOW (inactive) before scanning next column */
        HAL_GPIO_WritePin(column_ports[c], column_pins[c], GPIO_PIN_RESET);
    }

    if (fn_pressed) {
        for (int i = 0; i < 6; i++) {
            if (kb_report.keycodes[i] == 0x0C) kb_report.keycodes[i] = 0x46; // PrintScreen
            if (kb_report.keycodes[i] == 0x12) kb_report.keycodes[i] = 0x47; // Scroll Lock
        }
    }

    Read_ADC();
    gp_report.l2 = adc_values[0];
    gp_report.lx = adc_values[1];
    gp_report.ly = 255 - adc_values[2]; // Y-axis inverted
    gp_report.ry = 255 - adc_values[3]; // Y-axis inverted
    gp_report.rx = adc_values[4];
    gp_report.r2 = adc_values[5];

    // Only send if changed or mode requires
    if (memcmp(&kb_report, &prev_kb_report, sizeof(kb_report)) != 0) {
        if (USB_SendReport((uint8_t*)&kb_report, sizeof(kb_report)) == USBD_OK) {
            memcpy(&prev_kb_report, &kb_report, sizeof(kb_report));
        }
    }

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
        int8_t wheel = (int8_t)(128 - (int16_t)adc_values[2]); // Y-axis inverted

        /* Deadzone ±25/128: covers joystick mechanical center variation and ADC noise */
        if (mx    >= -25 && mx    <= 25) mx    = 0;
        if (my    >= -25 && my    <= 25) my    = 0;
        if (wheel >= -25 && wheel <= 25) wheel = 0;

        mouse_report.x     = mx    / 8;
        mouse_report.y     = my    / 8;
        mouse_report.wheel = wheel / 16;

        /* Send on movement OR button state change (button-only clicks must not be dropped) */
        if (mouse_report.x != 0 || mouse_report.y != 0 || mouse_report.wheel != 0 ||
            mouse_report.buttons != prev_mouse_buttons) {
            USB_SendReport((uint8_t*)&mouse_report, sizeof(mouse_report));
            prev_mouse_buttons = mouse_report.buttons;
        }
    }
}
