#ifndef __I2C_H
#define __I2C_H

#include "stm32f10x.h"

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_WaitAck(void);
void I2C_SendAck(uint8_t ack);
uint8_t I2C_WriteByte(uint8_t data);
uint8_t I2C_ReadByte(void);

#endif
