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


#include <stdio.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "swuart.h"
#include "overclock.h"

//When JTAG and flash are connected, the device doesn't have a serial port
//to write stuff to. Instead, it will write it to an EEPROM ring buffer. The 
//format of the EEPROM ring buffer is that the last byte written always
//is 0xFF. Skip that and all the following 0xFFs, wrap around if needed, and
//you'll arrive at the first byte written (and not overwritten yet).

//Write a character to the EEPROM, to later read out.
//We need to set the CPU clock to normal values while writing, else the
//write may fail.
static int eeprom_putchar(char c, FILE *stream) {
	int pos=0;
	//Save CPU sped and reset to normal speed
	char oldOverclockState=overclockGetState();
	overclockCpu(OVERCLOCK_STD);
	//Find the last written byte
	while (eeprom_read_byte(pos)!=0xff && pos<512) pos++;
	if (pos==512) pos=0;  //Shouldn't happen.
	//Write the byte and the following 0xFF
	eeprom_update_byte(pos, c);
	eeprom_update_byte(pos+1, 0xff);
	eeprom_busy_wait();
	//Reset CPU speed.
	overclockCpu(oldOverclockState);
	return 0;
}

//Dump whatever we've stored to EEPROM to stdout.
void stdoutDumpEepromLog(void) {
	int pos=511;
	int x;
	unsigned char b;
	//Find start of buffer
	while (eeprom_read_byte(pos)!=0xff && pos>0) pos--;
	//Write bytes to stdout
	for (x=0; x<512; x++) {
		b=eeprom_read_byte(pos++);
		if (b=='\n') putchar('\r');
		if (b<128) putchar(b);
		if (pos>=512) pos=0;
	}
}


static int uart_putchar(char c, FILE *stream) {
	swUartXmit(c);
	return 0;
}

static int uart_getchar(FILE *stream) {
	int i=swUartRecv();
	return i&255;
}

static FILE eepstdout = FDEV_SETUP_STREAM(eeprom_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE mystdin = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

void stdoutInit(int ubr) {
	swUartInit(ubr);
	stdout = &mystdout;
	stdin = &mystdin;
}

void stdoutInitEeprom(void) {
	stdout = &eepstdout;
}

