#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#include "main.h"

void UART_Protocol_Init(void);
void UART_Protocol_Process(void);
void UART_Protocol_RxCpltCallback(UART_HandleTypeDef *huart);
void UART_Protocol_TxCpltCallback(UART_HandleTypeDef *huart);

#endif /* __UART_PROTOCOL_H */
