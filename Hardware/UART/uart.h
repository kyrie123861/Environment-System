#ifndef __UART_H
#define __UART_H

#include "stm32f10x.h"
#include <stdio.h>

void UART_Init(void);
void UART_SendChar(uint8_t ch);
uint8_t UART_ReceiveChar(void);
uint8_t UART_ReadByte(uint8_t *data);
uint8_t UART_Available(void);
void UART_Flush(void);

#endif
