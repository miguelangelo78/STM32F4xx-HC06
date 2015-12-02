#ifndef HC06_H_
#define HC06_H_

#include <stdio.h>

void HC06_Init(uint32_t speed);
void HC06_PutChar(uint16_t c);
void HC06_PutStr(char *str);
void HC06_ClearRxBuffer(void);
uint8_t HC06_Test(void);
uint8_t HC06_SetBaud(uint32_t speed);
uint8_t HC06_SetName(char *name);
uint8_t HC06_SetPin(char *pin);

#endif
