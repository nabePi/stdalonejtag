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


//Xsvf parser. Implemented with Xilinx XAPP503 and openocds xsvf.c 
//(http://openocd.sourceforge.net/doc/doxygen/html/xsvf_8c_source.html)
//as references.

#include "jtag.h"
#include "debug.h" //for dprintf
#include "io.h"
#include <util/delay.h>


#define MAXTDOBYTES 128 //Bytes the AVR is able to shift out
#define MAXTDIBYTES 8 //Bytes the AVR is able to check against clocked in bytes.

//xsvf instructions
#define XCOMPLETE	0x00
#define XTDOMASK	0x01
#define XSIR		0x02
#define XSDR		0x03
#define XRUNTEST	0x04
#define XREPEAT		0x07
#define XSDRSIZE	0x08
#define XSDRTDO		0x09
#define XSETSDRMASKS 0x0a
#define XSDRINC		0x0b
#define XSDRB		0x0c
#define XSDRC		0x0d
#define XSDRE		0x0e
#define XSDRTDOB	0x0f
#define XSDRTDOC	0x10
#define XSDRTDOE	0x11
#define XSTATE		0x12
#define XENDIR		0x13
#define XENDDR		0x14
#define XSIR2		0x15
#define XCOMMENT	0x16
#define XWAIT		0x17

//This needs to be defined somewhere else. It should return 'the next' byte read
//from the xsvf file.
extern unsigned char xsvfGetByte(void);

static unsigned char *tdiData;
static unsigned char *tdoExpected;
static unsigned char *tdoMask;
static unsigned long sdrsize;

static unsigned long getLong(void) {
	int x;
	unsigned long ret=0;
	for (x=0; x<4; x++) {
		ret<<=8;
		ret+=xsvfGetByte();
	}
//	printf("ret=%lx\n", ret);
	return ret;
}

//Gives the minimum amount of bytes the amount of bits given can be stored in.
static unsigned int byteLenForBits(unsigned int noBits) {
	return (noBits+7)>>3;
}

//Shift noBits of bits from tdiData to the JTAG port. If checkTdo is set, also
//compares the outputted data to tdoExpected, if checkTdoMask is set after
//applying tdoMask to it.
static char shiftBits(int noBits, char checkTdo, char checkTdoMask, char endraisetms) {
	unsigned char bitcount, in, bpos, check, x;
	char ret=1;
	int left=noBits;
	bpos=0;
	while (left>0) {
		bitcount=(left>8)?8:left;
		in=jtagShift(tdiData[bpos], (left<8)?left:8, ((left<=8) && endraisetms));
		if (bpos<MAXTDIBYTES) {
			tdiData[bpos]=in; //for display if fail
			if (checkTdo && bpos<MAXTDIBYTES) {
				check=tdoExpected[bpos];
				if (checkTdoMask) {
					in=in&tdoMask[bpos];
					check=check&tdoMask[bpos];
				}
				if (in!=check) ret=0;
			}
		}
		left-=8;
		bpos++;
	}
	if (!ret) {
		dprintf("Shift fail. Expected: ");
		for (x=0; x<((noBits+7)>>3); x++) dprintf("%02x ", tdoExpected[x]);
		dprintf("got ");
		for (x=0; x<((noBits+7)>>3); x++) dprintf("%02x ", tdiData[x]);
		dprintf("\n");
	}
	return ret;
}

//Sends out bits without checking anything.
static void shiftOutBitsQuick(int nobits, char endraisetms) {
	unsigned char *p=tdiData;
	unsigned char x;
	unsigned char bytes=((nobits-1)>>3);
	//This loop shifts out all bytes... except the very last one
	for (x=0; x<bytes; x++) {
		jtagShiftOutByte(*p);
		p++;
	}
	//Shift out last byte, using jtagShift for the endraisetms clause
	x=nobits-(bytes*8);
	jtagShift(*p, x, endraisetms);
	return;
}


static void readBuffer(unsigned char *dest, int bits) {
	int x;
	int no=byteLenForBits(bits);
	unsigned char *p=&dest[no-1]; //Stored msb-first in xsvf file, but we want lsb-first
	for (x=0; x<no; x++) {
		*p=xsvfGetByte();
		--p;
	}
}

unsigned char xsvfRun(void) {
	const int doExplain=0;
	int x=0, len=0;
	unsigned char ins;
	unsigned long runtestcycles=0;
	unsigned char endirstate=1, enddrstate=1;
	unsigned char localdata1[MAXTDOBYTES];
	unsigned char localdata2[MAXTDIBYTES*2];
	sdrsize=32;

	tdiData=&localdata1[0];
	tdoExpected=&localdata2[0];
	tdoMask=&localdata2[MAXTDIBYTES];

	dprintf("Xsvf parse start\n");
	jtagReset();
	while(1) {
		ins=xsvfGetByte();
		if (doExplain) dprintf("Xsvf: %x ", (int)ins);
		if (ins==XTDOMASK) {
			if (doExplain) dprintf("XTDOMASK\n");
			readBuffer(tdoMask, sdrsize);
		} else if (ins==XREPEAT) {
			if (doExplain) dprintf("XREPEAT\n");
			xsvfGetByte(); //Unimplemented.
		} else if (ins==XRUNTEST) {
			if (doExplain) dprintf("XRUNTEST\n");
			runtestcycles=getLong();
		} else if (ins==XSIR || ins==XSIR2) {
			if (doExplain) dprintf("XSIR[2]\n");
			len=xsvfGetByte();
			if (ins==XSIR2) len|=(xsvfGetByte()<<8);
			readBuffer(tdiData, len);
			jtagGotoState(JTAG_SHIFTIR);
			shiftBits(len, 0, 0, 1);
		} else if (ins==XSDR || ins==XSDRTDO) {
			if (doExplain) dprintf("XSDR[TDO]\n");
			jtagGotoState(JTAG_SHIFTDR);
			readBuffer(tdiData, sdrsize);
			if (ins==XSDRTDO) readBuffer(tdoExpected, sdrsize);
			if (!shiftBits(sdrsize, 1, 1, 1)) return 0;
		} else if (ins==XSDRSIZE) {
			if (doExplain) dprintf("XSDRSIZE\n");
			sdrsize=getLong();
			if (sdrsize>(MAXTDOBYTES*8)) {
				dprintf("Can't handle xsvf! Requested sdrsize: %i bits, can handle %i! Increase MAXTDOBYTES plz.\n", sdrsize, MAXTDOBYTES*8);
				return 1; //not 0 because sw will retry then.
			}
			if (doExplain) dprintf("sdrsize=%li\n", sdrsize);
		} else if (ins==XRUNTEST) {
			if (doExplain) dprintf("XRUNTEST\n");
			runtestcycles=getLong();
		} else if (ins==XSDRB || ins==XSDRC || ins==XSDRE) {
			//OPTIMIZE HERE! This is where an average FPGA upload
			//will spend most of its time.
			if (doExplain) dprintf("XSDR[BCE]\n");
			if (ins==XSDRB) jtagGotoState(JTAG_SHIFTDR);
			readBuffer(tdiData, sdrsize);
//			shiftBits(sdrsize, 0, 0, (ins==XSDRE)); //slow variant
			shiftOutBitsQuick(sdrsize, (ins==XSDRE)); //quick variant
			if (ins==XSDRE) jtagGotoState(enddrstate);
		} else if (ins==XSDRTDOB || ins==XSDRTDOC || ins==XSDRTDOE) {
			if (doExplain) dprintf("XSDRTDO[BCE]\n");
			if (ins==XSDRTDOB) jtagGotoState(JTAG_SHIFTDR);
			readBuffer(tdiData, sdrsize);
			readBuffer(tdoExpected, sdrsize);
			if (!shiftBits(sdrsize, 1, 0, ins!=XSDRTDOE)) return 0;
			if (ins==XSDRTDOE) {
				jtagGotoState(enddrstate);
			}
		} else if (ins==XCOMPLETE) {
			dprintf("Xsvf done!\n");
			return 1;
		} else if (ins==XSTATE) {
			if (doExplain) dprintf("XSTATE\n");
			jtagGotoState(xsvfGetByte());
		} else if (ins==XENDIR) {
			if (xsvfGetByte()) endirstate=JTAG_PAUSEIR; else endirstate=JTAG_RUNTEST;
		} else if (ins==XENDDR) {
			if (xsvfGetByte()) endirstate=JTAG_PAUSEDR; else endirstate=JTAG_RUNTEST;
		} else {
			dprintf("Xsvf: Invalid instruction %x\n", (int)ins);
			return 0;
		}

		//Finish XSIR and XSDR command
		if (ins==XSIR || x==XSIR2 || ins==XSDR) {
			if (runtestcycles!=0) {
				long w;
				jtagGotoState(JTAG_RUNTEST);
				for (w=0; w<runtestcycles; w++) {
					jtagShift(0, 1, 0);
					_delay_ms(1);
				}
			} else {
				jtagGotoState((ins==XSIR)?endirstate:enddrstate);
			}
		}
	}
}




