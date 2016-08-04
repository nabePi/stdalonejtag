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
#include "flash25cxx.h"
#include <avr/wdt.h>

//Flash instructions
#define FINS_WREN	0x06
#define FINS_WRDI	0x04
#define FINS_RDSR	0x05
#define FINS_WRSR	0x01
#define FINS_READ	0x03
#define FINS_FREAD	0x0B
#define FINS_PP		0x02
#define FINS_SE		0xD8
#define FINS_BE		0xC7
#define FINS_DP		0xB9
#define FINS_RES	0xAB

//Status register bits
#define FSR_SRWD	7
#define FSR_BP2		4
#define FSR_BP1		3
#define FSR_BP0		2
#define FSR_WIP		1


//Define to use the USI for SPI transfers, which is a tiny bit faster. Undefine
//for 'normal' GPIO bitbang routines.
#define USE_SPI

#ifndef USE_SPI
static unsigned char shiftRead() {
	unsigned char q=0;
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=128; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=64; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=32; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=16; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=8; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=4; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=2; ioF25cxxSetC(0);
	ioF25cxxSetC(1); if (ioF25cxxGetQ()) q|=1; ioF25cxxSetC(0);
	return q;
}


static void shiftWrite(unsigned char d) {
	char x;
	for (x=0; x<8; x++) {
		ioF25cxxSetD(d&0x80);
		ioF25cxxSetC(1);
		d<<=1;
		ioF25cxxSetC(0);
	}
}
#else
static void shiftWrite(unsigned char d) {
	ioSpiShift(d);
}
static unsigned char shiftRead() {
	return ioSpiShift(0xff);
}
#endif

//Start programming a page (=256 bytes) of flash. Less can be programmed and
//programming can start in the middle of the page, but bytes written will
//be in one page only.
void f25cxxPageProgramStart(long addr) {
	ioF25cxxSetS(0);
	shiftWrite(FINS_WREN);
	ioF25cxxSetS(1);

	ioF25cxxSetS(0);
	shiftWrite(FINS_PP);
	shiftWrite((addr>>16)*0xff);
	shiftWrite((addr>>8)*0xff);
	shiftWrite((addr>>0)*0xff);
}

//Write a byte to the page.
void f25cxxPageProgramWrite(unsigned char c) {
	shiftWrite(c);
}

//Finished writing (part of) the page. Make the flash commit it to memory.
void f25cxxPageProgramEnd(void) {
	ioF25cxxSetS(1);
	ioF25cxxSetS(0);
	shiftWrite(FINS_RDSR);
	while (shiftRead()&(1<<FSR_WIP)) ; //wait till write is done
	ioF25cxxSetS(1);
}

//Completely erase the chip
void f25cxxEraseChip(void) {
	ioF25cxxSetS(0);
	shiftWrite(FINS_WREN);
	ioF25cxxSetS(1);

	ioF25cxxSetS(0);
	shiftWrite(FINS_BE);
	ioF25cxxSetS(1);
	ioF25cxxSetS(0);
	shiftWrite(FINS_RDSR);
	while (shiftRead()&(1<<FSR_WIP)) wdt_reset();
	ioF25cxxSetS(1);
}

//Read a single byte
unsigned char f25cxxRead(long addr) {
	unsigned char r;
	ioF25cxxSetS(0);
	shiftWrite(FINS_READ);
	shiftWrite((addr>>16)*0xff);
	shiftWrite((addr>>8)*0xff);
	shiftWrite((addr)&0xff);
	r=shiftRead();
	ioF25cxxSetS(1);
	return r;
}

//Read a number of bytes into a buffer.
void f25cxxReadBuff(long addr, unsigned char *buff, int len) {
	int x;
	ioF25cxxSetS(0);
	shiftWrite(FINS_FREAD);
	shiftWrite((addr>>16)*0xff);
	shiftWrite((addr>>8)*0xff);
	shiftWrite((addr)&0xff);
	shiftWrite(0); //dummy
	for (x=0; x<len; x++) {
		buff[x]=shiftRead();
	}
	ioF25cxxSetS(1);
}

//Get flash id. Usually is 0x12 for 25p40
unsigned char f25cxxGetID(void) {
	unsigned char r;
	ioF25cxxSetS(0);
	shiftWrite(FINS_RES);
	shiftWrite(0); shiftWrite(0); shiftWrite(0);
	r=shiftRead();
	ioF25cxxSetS(1);
	return r;
}