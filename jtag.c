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

#include <avr/pgmspace.h>
#include "io.h"
#include "jtag.h"
#include "debug.h"

static const unsigned char nextStates[] PROGMEM={
		0x01, //0
		0x21, //1
		0x93, //2
		0x54, //3
		0x54, //4
		0x86, //5
		0x76, //6
		0x84, //7
		0x21, //8
		0x0A, //9
		0xCB, //a
		0xCB, //b
		0xFD, //c
		0xED, //d
		0xFB, //e
		0x21  //f
};

//Routing table adapted from //http://www.freescale.com/webapp/sps/site/overview.jsp?code=784_LPBBJTAGTAP
static const unsigned int routingTable[] PROGMEM={
		0b0000000000000001, 
		0b1111111111111101, 
		0b1111111000000011, 
		0b1111111111100111, 
		0b1111111111101111, 
		0b1111111100001111, 
		0b1111111110111111, 
		0b1111111100001111, 
		0b1111111011111101, 
		0b0000000111111111, 
		0b1111001111111111, 
		0b1111011111111111, 
		0b1000011111111111, 
		0b1101111111111111, 
		0b1000011111111111, 
		0b0111111111111101
	};


//unsigned char ioJtagClock(unsigned char tdo, unsigned char tms) {
static unsigned char jtagCurrState;

static unsigned char nextState(unsigned char tms) {
	//When index is currstate, high nibble is nextstate when tms=1, low is nextstate when tms=0
	unsigned char r;
	r=pgm_read_byte(&nextStates[(int)jtagCurrState]);
	if (tms) return (r>>4); else return r&0xF;
}

static unsigned char route(unsigned char currState, unsigned char wantedState) {
	//We want to get from currState to wantedState. To get one step closer 
	//to the goal, we need to set TMS to the wantedState'th bit of 
	//routingTable[currentState].
	return (pgm_read_word(&routingTable[(int)currState])&(1<<wantedState))?1:0;
}

void jtagReset(void) {
	int x;
	//Reset JTAG chain. 5 times should work, doubled for safety.
	for (x=0; x<32; x++) ioJtagClock(1, 1);
	jtagCurrState=JTAG_TESTLOGICRESET;
}

void jtagGotoState(unsigned char state) {
	unsigned char tms;
	while (state!=jtagCurrState) {
		tms=route(jtagCurrState, state);
		ioJtagClock(1, tms);
//		dprintf("Going from state %i to %i  through %i by tms %i\n", jtagCurrState, state, nextState(tms), tms);
		jtagCurrState=nextState(tms);
	}
//	dprintf("Arrived in state %i.\n", jtagCurrState);
}

unsigned char jtagShift(unsigned char data, unsigned char bits, unsigned char endraisetms) {
	//Can be only shift-dr or shift-ir; we need tms=0 to stay there.
	unsigned char x, b;
	unsigned char out=0;
	unsigned char msk=1;
#ifdef DEBUG
	if (jtagCurrState!=JTAG_SHIFTIR && jtagCurrState!=JTAG_SHIFTDR && jtagCurrState!=JTAG_RUNTEST) {
		dprintf("Warning: shifting in incompatible state %i\n", (int)jtagCurrState);
	}
#endif
	for (x=0; x<bits; x++) {
		b=ioJtagClock(data&1, (endraisetms & (x==bits-1)));
//		dprintf("%i", data&1);
//		dprintf("%i", b?1:0);
		data>>=1;
		if (b) out|=msk;
		msk<<=1;
	}
	if (endraisetms) {
		jtagCurrState=nextState(1);
//		dprintf("TMS high on end of data transfer: moving to state %i\n",jtagCurrState);
	}
	return out;
}

//Quicker version of the above, for complete bytes only & restricted to outputting.
void jtagShiftOutByte(unsigned char data) {
	ioJtagClockOutOnly(data&1);
	ioJtagClockOutOnly(data&2);
	ioJtagClockOutOnly(data&4);
	ioJtagClockOutOnly(data&8);
	ioJtagClockOutOnly(data&16);
	ioJtagClockOutOnly(data&32);
	ioJtagClockOutOnly(data&64);
	ioJtagClockOutOnly(data&128);
}
