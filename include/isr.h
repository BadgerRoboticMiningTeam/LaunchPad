#ifndef _ISR_H_
#define _ISR_H_

#include <stdbool.h>
#include <stdint.h>

// This bit mask identifies which UART module triggered the interrupt.
// Least significant bit is set by UART0, most significant bit is set by UART7.
extern volatile uint8_t uartIntMask;

void uart0ISR(void);
void uart1ISR(void);
void uart2ISR(void);
void uart3ISR(void);
void uart4ISR(void);
void uart5ISR(void);
void uart6ISR(void);
void uart7ISR(void);

#endif