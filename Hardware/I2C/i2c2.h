#ifndef __I2C2_H
#define __I2C2_H

#include "stm32f10x.h"

void I2C2_Init(void);
void I2C2_Start(void);
void I2C2_Stop(void);
uint8_t I2C2_WaitAck(void);
void I2C2_SendAck(uint8_t ack);
uint8_t I2C2_WriteByte(uint8_t data);
uint8_t I2C2_ReadByte(void);

#endif
