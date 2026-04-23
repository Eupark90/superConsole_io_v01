#ifndef __IO_CONTROL_H
#define __IO_CONTROL_H

#include "main.h"

#define NUM_COLUMNS 14
#define NUM_ROWS 7

typedef struct __attribute__((packed)) {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} KeyboardReport_t;  /* 8 bytes — matches Interface 0 IN max packet */

typedef struct __attribute__((packed)) {
    uint8_t buttons;
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} MouseReport_t;     /* 4 bytes — matches Interface 1 IN max packet */

typedef struct __attribute__((packed)) {
    uint16_t buttons;
    uint8_t  lx;
    uint8_t  ly;
    uint8_t  rx;
    uint8_t  ry;
    uint8_t  l2;
    uint8_t  r2;
} GamepadReport_t;   /* 8 bytes — matches Interface 2 IN max packet */

void IO_Control_Init(void);
void IO_Control_Process(void);

#endif /* __IO_CONTROL_H */
