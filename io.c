/*
Firmware for an ATTiny85 to be able to receive and execute xsvf files
to a JTAG-enabled device.
(C) 2012 Jeroen Domburg (jeroen AT spritesmods.com)

This program is free software: you can redistribute it and/or modify
t under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "io.h"
#include <avr/io.h>
#include <util/delay.h>
#include "debug.h"
#include "swuart.h"

//Shared pins:
//Pin	Flash chip	Jtag		Sw uart
//PB0	F25CXX_D	JTAG_TDI	x
//PB1	F25CXX_Q	JTAG_TDO	UART_RXD
//PB2	F25CXX_C	JTAG_TMS	X
//PB3	F25CXX_S	x			x
//PB4	x			JTAG_TCK	UART_TXD


static unsigned char jtagState=(1<<JTAG_TMS)|(1<<JTAG_TDI)|(1<<JTAG_TCK);
static unsigned char prevState;

#define IO_JTAG 0
#define IO_UART 1
#define IO_FLASH 2

void ioInit(void) {
//	ioUartEnable();
}

//The IO pins share 3 functions: JTAG, flash interface and software UART. 
//The following routines switch between the functions.

void ioUartEnable(void) { 
	if (prevState==IO_JTAG) jtagState=PORTB;
	USICR=0;
	DDRB|=(1<<UART_TXD)|(1<<F25CXX_S);
	PORTB|=(1<<F25CXX_S)|(1<<UART_TXD);
	DDRB&=~(1<<UART_RXD);
	PORTB&=~(1<<UART_RXD);
	swUartEnable();
	prevState=IO_UART;
}

void ioJtagEnable(void) {
	if (prevState==IO_UART) swUartDisable();
	USICR=0;
	PORTB=jtagState;
	DDRB|=(1<<JTAG_TMS)|(1<<JTAG_TDI)|(1<<JTAG_TCK);
	DDRB&=~(1<<JTAG_TDO);
	prevState=IO_JTAG;
}

void ioFlashEnable(void) {
	if (prevState==IO_UART) swUartDisable();
	if (prevState==IO_JTAG) jtagState=PORTB;
	DDRB|=(1<<F25CXX_D)|(1<<F25CXX_C)|(1<<F25CXX_S);
	DDRB&=~(1<<F25CXX_Q);
	PORTB|=(1<<F25CXX_Q)|(1<<F25CXX_S);
	PORTB&=~((1<<F25CXX_D)|(1<<F25CXX_C));
	prevState=IO_FLASH;
}

//Shift a byte in and out using the USI, which is connected to the flash.
unsigned char ioSpiShift(unsigned char d) {
	USIDR=d;
#define USIHI (1<<4)|(1<<0)
#define USILO (1<<4)|(1<<0)|(1<<1)
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	USICR=USIHI; USICR=USILO;
	return USIDR;
}

//Generate one JTAG TCK pulse, with given tdi and tms values.
//Returns the state of the tdo pin.
unsigned char ioJtagClock(unsigned char tdi, unsigned char tms) {
	unsigned char ret;
	if (tms) PORTB|=(1<<JTAG_TMS); else PORTB&=~(1<<JTAG_TMS);
	if (tdi) PORTB|=(1<<JTAG_TDI); else PORTB&=~(1<<JTAG_TDI);
	PORTB|=(1<<JTAG_TCK);
	ret=PINB&(1<<JTAG_TDO);
	PORTB&=~(1<<JTAG_TCK);
	return ret;
}

//Quicker version, for outputting data only. Also presumes TMS already is in the
//correct state.
void ioJtagClockOutOnly(unsigned char tdi) {
	if (tdi) PORTB|=(1<<JTAG_TDI); else PORTB&=~(1<<JTAG_TDI);
//	PORTB&=~(1<<JTAG_TMS);
	PORTB|=(1<<JTAG_TCK);
	PORTB&=~(1<<JTAG_TCK);
}


