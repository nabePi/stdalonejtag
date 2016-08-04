
//JTAG state machine states
#define JTAG_TESTLOGICRESET		0
#define JTAG_RUNTEST			1
#define JTAG_SELECTDRSCAN		2
#define JTAG_CAPTUREDR			3
#define JTAG_SHIFTDR			4
#define JTAG_EXIT1DR			5
#define JTAG_PAUSEDR			6
#define JTAG_EXIT2DR			7
#define JTAG_UPDATEDR			8
#define JTAG_SELECTIRSCAN		9
#define JTAG_CAPTUREIR			10
#define JTAG_SHIFTIR			11
#define JTAG_EXIT1IR			12
#define JTAG_PAUSEIR			13
#define JTAG_EXIT2IR			14
#define JTAG_UPDATEUR			15

unsigned char jtagShift(unsigned char data, unsigned char bits, unsigned char endraisetms);
void jtagShiftOutByte(unsigned char data);
void jtagGotoState(unsigned char state);
void jtagReset(void);
