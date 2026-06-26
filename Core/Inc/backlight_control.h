#ifndef __BACKLIGHT_CONTROL_H
#define __BACKLIGHT_CONTROL_H

#include "main.h"

void Backlight_Init(void);
void Backlight_SetPercent(uint8_t percent);
void Backlight_Increase(void);
void Backlight_Decrease(void);
uint8_t Backlight_GetPercent(void);

#endif /* __BACKLIGHT_CONTROL_H */
