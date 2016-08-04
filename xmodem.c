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


/*
Xmodem handler. This will receive data over the software UART and write it
into the flash chip. As a hack, you can also press 'l' and the code
will emit whatever message was printf()'ed during the last few jtag upload
tries.
*/

#include <stdio.h>
#include <avr/wdt.h>
#include "flash25cxx.h"
#include "stdout.h"
#include "swuart.h"
#include "debug.h"
#include "io.h"
#include "xmodem.h"
#include <util/delay.h>

#define SOH 0x01
#define ACK 0x06
#define NAK 0x15
#define EOT 0x04

static int xmodemTimer;
#define MAXTIME 30000

//Get char from sw uart, but time out when xmodemTimer=0.
char getcharTimed() {
	if (xmodemTimer>=MAXTIME) return 0;
	while(!swUartHasRecved()) {
		wdt_reset();
		_delay_us(100);
		xmodemTimer++;
		if (xmodemTimer>=MAXTIME) return 0;
	}
	return getchar()&0xff;
}

void xmodemWriteFlash() {
	unsigned char block, invBlock, oldBlock=0;
	char data[128], byte, chsum;
	int x, y;
	char first=1;
	long addr=0;

	ioFlashEnable();
	ioUartEnable();
	putchar(NAK);
	while(1) {
		//First, wait for SOH or EOT (start of packet or end of transmission)
		do {
			xmodemTimer=0;
			y=getcharTimed();
			if (xmodemTimer==MAXTIME) putchar(NAK); //timeout
			if (y=='l') { //Debug: press 'l' to read out the log.
				dprintf("----LOG----\r\n");
				stdoutDumpEepromLog();
				dprintf("----END----\r\n");
			}
		} while (y!=SOH && y!=EOT);
		if (y==EOT) {
			//All done.
			putchar(ACK);
			return;
		}

		//Start receiving an xmodem block.
		xmodemTimer=0;
		chsum=0;
		
		block=getcharTimed(); //block number
		invBlock=getcharTimed(); //Inverted block number
	
		chsum=0; //Reset checksum
		for (x=0; x<128; x++) {
			byte=getcharTimed();
			data[x]=byte;
			chsum+=byte; //Update checksum
		}
		byte=getcharTimed(); //checksum should be this.
		if (byte==chsum && block==invBlock^0xff && block==((oldBlock+1)&255) && xmodemTimer<MAXTIME) {
			//Block seems OK. Commit to flash.
			//If this is the first sector, also erase the chip.
			ioFlashEnable();
			if (first) {
				f25cxxEraseChip();
				first=0;
			}
			//Program the page
			f25cxxPageProgramStart(addr);
			for (x=0; x<128; x++) {
				f25cxxPageProgramWrite(data[x]);
			}
			f25cxxPageProgramEnd();
			ioUartEnable();
			//Yay, block is written!
			putchar(ACK);
			addr+=128;
			oldBlock=block;
		} else {
			//Something went wron. Try again okplzthx.
			putchar(NAK);
		}
	}
}
