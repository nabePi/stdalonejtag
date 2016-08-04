#include <avr/io.h>

//This is actually only true for the un-overclocked CPU. Ah well...
#define F_CPU 16000000

//IO definitions
#define F25CXX_D PB1
#define F25CXX_C PB2
#define F25CXX_S PB3
#define F25CXX_Q PB0

#define UART_TXD PB4
#define UART_RXD PB0
#define UART_RX_PCINT 0

#define JTAG_TMS PB2
#define JTAG_TDI PB1
#define JTAG_TDO PB0
#define JTAG_TCK PB4

//Macros, for speed
#define ioUartSetTx(p) if (p) PORTB|=(1<<UART_TXD); else PORTB&=~(1<<UART_TXD)
#define ioUartGetRx() (PINB&(1<<UART_RXD))
#define ioF25cxxGetQ() (PINB&(1<<F25CXX_Q))
#define ioF25cxxSetD(p) if (p) PORTB|=(1<<F25CXX_D); else PORTB&=~(1<<F25CXX_D)
#define ioF25cxxSetS(p) if (p) PORTB|=(1<<F25CXX_S); else PORTB&=~(1<<F25CXX_S)
#define ioF25cxxSetC(p) if (p) PORTB|=(1<<F25CXX_C); else PORTB&=~(1<<F25CXX_C)

void ioInit(void);
unsigned char ioJtagClock(unsigned char tdi, unsigned char tms);
void ioJtagClockOutOnly(unsigned char tdi);
void ioInit(void);
void ioUartEnable(void);
void ioJtagEnable(void);
void ioFlashEnable(void);
unsigned char ioSpiShift(unsigned char d);
