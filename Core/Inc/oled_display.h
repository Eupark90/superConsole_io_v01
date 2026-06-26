#ifndef __OLED_DISPLAY_H
#define __OLED_DISPLAY_H

#include "main.h"

void OLED_Display_Init(void);
void OLED_Display_Process(void);
void OLED_Display_RequestRefresh(void);
void OLED_Display_I2C_TxCpltCallback(I2C_HandleTypeDef *hi2c);
void OLED_Display_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);

#endif /* __OLED_DISPLAY_H */
