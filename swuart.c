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

//Software uart using timer0 and pcints. It should even be full-duplex!

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include "io.h"
#include "swuart.h"

static volatile unsigned char byteRecving, byteRecved;
static volatile char byteSending, byteSend;
static volatile char flags;
static volatile char rxState, txState;

#define F_RXDDONE (1<<0) //Set when a byte is received and in byteRecved
#define F_TXDDONE (1<<1) //Set when transmission is done and F_TXDEMPTY is true
#define F_TXDEMPTY (1<<2) //Set when the byte in byteSend is copied to byteSending

/*
Serial states:
- Start bit. Always low.
- 8 databits
- Stop bit. Always high.
*/

//Shared for rxState and txState
#define UART_IDLE 0
#define UART_START 1
#define UART_DATA0 2
// ...
#define UART_DATA7 9
#define UART_STOP 10
#define UART_FINISHED 10

//divider = F_CPU/(baudrate*8)
void swUartInit(char mydivider) {
	TCCR0A=2; //CTC, max=ocr0a
	TCCR0B=2; //count freq = F_CPU/8
	OCR0A=mydivider;
	rxState=0; txState=0;
	PCMSK=(1<<UART_RX_PCINT); //enable int on incoming data
	flags=F_TXDEMPTY|F_TXDDONE;
}

void swUartDisable() {
	while(!(flags&F_TXDDONE)) ; //Wait till UART is idle
	//Disable interrupts
	TIMSK&=~((1<<4)|(1<<3));
	GIMSK&=~(1<<5);
	//Reset UART
	rxState=0; txState=0;
	flags=F_TXDEMPTY|F_TXDDONE;
}

void swUartEnable() {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		//Enable interrupts
		TIMSK|=(1<<4); //enable OC0A, not yet OC0B
		GIMSK|=(1<<5); //enable PC ints
		//Clear flags
		TIFR=(1<<4)|(1<<3);
		GIFR=(1<<5);
	}
}

void swUartXmit(char b) {
	while(!(flags&F_TXDEMPTY)) ; //Wait till current TX is done.
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		//Set byte and flag so the timer routine will pick it up
		byteSend=b;
		flags&=~F_TXDEMPTY;
	}
}

char swUartRecv(void) {
	while(!swUartHasRecved());
	flags&=~F_RXDDONE;
	return byteRecved;
}

char swUartCanXmit(void) {
	return flags&F_TXDEMPTY;
}

char swUartHasRecved(void) {
	return flags&F_RXDDONE;
}

//Sending routine. Picks up a byte from byteSend and sends it, if F_TXDEMPTY=0
ISR(TIM0_COMPA_vect) { 
	if (txState==UART_IDLE) {
		if (!(flags&F_TXDEMPTY)) {
			//Need to send another byte. Emit start bit.
			ioUartSetTx(0);
			txState=UART_DATA0;
			byteSending=byteSend;
			flags|=F_TXDEMPTY;
			flags&=~F_TXDDONE;
		}
	} else if (txState>=UART_DATA0 && txState<=UART_DATA7) {
		//Send a data bit
		ioUartSetTx(byteSending&1);
		byteSending>>=1;
		txState++;
	} else if (txState==UART_STOP) {
		//Send the stop bit
		ioUartSetTx(1);
		txState++;
	} else { //if (txState==UART_FINISHED) {
		//Ok, all done.
		flags|=F_TXDDONE;
		txState=UART_IDLE;
	}
}

//Rx pin has wiggled!
ISR(PCINT0_vect) {
	unsigned char t;
	if (rxState==UART_IDLE && !ioUartGetRx()) {
		rxState=UART_START;
		t=TCNT0+(OCR0A>>1); //wait half a bit time before sampling
		if (t>=OCR0A) t-=OCR0A; //wraparound
		OCR0B=t;
		TIMSK|=(1<<3); //enable receive timer interrupt
		TIFR=(1<<3); //kill outstanding int request
	}
}

//Receives a byte.
ISR(TIM0_COMPB_vect) {
	if (rxState==UART_IDLE) {
		//Receiver is waiting for PCINT to start.
	} else if (rxState==UART_START) {
		//Catch wrong startbit, reset logic if this happens
		if (ioUartGetRx()) rxState=UART_IDLE; else rxState=UART_DATA0;
	} else if (rxState>=UART_DATA0 && rxState<=UART_DATA7) {
		//Receive a data bit
		byteRecving>>=1;
		if (ioUartGetRx()) byteRecving|=0x80;
		rxState++;
	} else if (rxState==UART_STOP) {
		//Check stop bit; if OK, indicate we've recved a byte.
		if (ioUartGetRx()) {
			byteRecved=byteRecving;
			flags|=F_RXDDONE;
		}
		rxState=UART_IDLE;
		TIMSK&=~(1<<3); //disable timer int
	}
}
