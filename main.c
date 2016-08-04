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
#include "stdout.h"
#include "flash25cxx.h"
#include "xsvf.h"
#include "xmodem.h"
#include "debug.h"
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include "overclock.h"

static long addr=0;

//Byte read handler for the XSVF parser. Reads from flash, but does it
//CACHESIZE bytes at a time and will return bytes from that cache.

//CACHESIZE needs to be power of 2
#define CACHESIZE 32
unsigned char xsvfGetByte(void) {
	unsigned char data;
	static char cache[CACHESIZE];
	wdt_reset();
	if ((addr&(CACHESIZE-1))==0) {
		//refill cache
		ioFlashEnable();
		f25cxxReadBuff(addr, cache, CACHESIZE);
		ioJtagEnable();
	}
	data=cache[addr&(CACHESIZE-1)];
	addr++;
	return data;
}

//Main routine
int main(void) {
	int i=0;
	ioInit();
	overclockInit();
	wdt_enable(WDTO_1S);

	//Test flash.
	ioFlashEnable();
	i=f25cxxGetID();

	stdoutInitEeprom(); //log to eeprom
	dprintf("Pwr-on\n");

	overclockCpu(OVERCLOCK_MAX); //UPLOAD MORE QUICKER NAU!!!!!11
	//Retry uploading config a few times.
	for (i=0; i<5; i++) {
		if (i==3) {
			//We had a few retries. Perhaps try again at a lower speed?
			overclockCpu(OVERCLOCK_STD);
		}
		addr=0;
		if (xsvfRun()) {
			//Success! All done.
			dprintf("Done configuring: success.\n");
			break;
		}
	}

	//then drop to xmodem upload
	overclockCpu(OVERCLOCK_STD); //Serial needs normal clock speeds...
	stdoutInit(52-1); //19200 baud = 104, 38400 baud = 52
	ioUartEnable();
	sei(); //sw uart is interrupt driven, so enable interrupts
	dprintf("Dropping to xmodem.\r\n");
	ioFlashEnable();
	i=f25cxxGetID();
	ioUartEnable();
	printf("Flash ID=%i\r\n", i);
	xmodemWriteFlash();

	while(1);
}
