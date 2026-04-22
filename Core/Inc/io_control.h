#ifndef __IO_CONTROL_H
#define __IO_CONTROL_H

#include "main.h"

#define NUM_COLUMNS 14
#define NUM_ROWS 7

typedef struct {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} KeyboardReport_t;

typedef struct {
    uint8_t report_id;
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} MouseReport_t;

typedef struct {
    uint8_t report_id;
    uint16_t buttons;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint8_t l2;
    uint8_t r2;
} GamepadReport_t;

void IO_Control_Init(void);
void IO_Control_Process(void);

#endif /* __IO_CONTROL_H */
