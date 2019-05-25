//
// (C) 2007 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this software.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// Commercial use means ANY distribution for payment, whether or
// not for profit.
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//
#include <avr/io.h>
#include "gpr.h"
#include "serial.h"
#include "kb.h"
#include "ti.h"

unsigned char is_up, isextended, shiftstate, capslock;
extern unsigned char ticols[8];
extern const unsigned char *pCheat;

extern void InjectCheatKey();

void (*CheckTIPolling)(void) = &CheckTIPollingFake;

// do nothing, leave outputs tristated (used before a keyboard is found)
void CheckTIPollingFake() {
	TRISTATE_TO_TI;
}

// Check and update the bits for polling from the TI computer
// This must be called very frequently to avoid dropped or
// worse, incorrect keys.
void CheckTIPollingReal() {
	// read the current value
	// just like the LEDs, the values are flipped from
	// what might be expected - 0 means active, 1 means
	// inactive. Only one pin should ever be 0 at a time
	static unsigned int old_x=0xffff;
	static int nChanges=0;
	unsigned char x=PINC;
	unsigned char nOut;

	// no selects means probably joystick active
	if (x==0xff) {
		// for joysticks to work, we need to tristate our output
		// we'll need to turn it back on later
		// Tristated means we have to set to input, pullup off.
		// as it's two steps, there is an intermediate, but
		// we change it in a particular order since output high and input w/pullup
		// will look the same externally.
		TRISTATE_TO_TI;
		return;
	}
	
	// x contains a column that has been selected
	// use the input value to set the correct output value
	switch (x) {
		// we'll only toggle on a single bit set
		// note the column values are wonky cause I screwed up the matrix table
		case 0xfe:	nOut=ticols[7]; break;
		case 0xfd:	nOut=ticols[3]; break;
		case 0xfb:	nOut=ticols[5]; break;
		case 0xf7:	nOut=ticols[1]; break;
		case 0xef:	nOut=ticols[6]; break;
		case 0xdf:	nOut=ticols[2]; break;

		default:
			// alpha lock has it's own test, unfortunately
			// but we only need to check in the default case
			if (0==(x&0x40)) {	
				flipled(7);
				if (capslock) {
					// This is not strictly accurate - KSCAN tends
					// to leave column 0 on at the same time so other
					// keys *could* be down. However, in practice no
					// valid key combinations make sense, so this is fine.
					nOut=0xef;
				} else {
					nOut=0xff;
				}
				break;
			}

			// nothing else SHOULD be possible, but we'll
			// try to handle weird cases and just shut off our output
			nOut=0xff;
			break;
	}

	if (nOut == 0xff) {
		// if we are not going to output anything anyway, just tri-state so
		// the internal keyboard can work
		TRISTATE_TO_TI;
	} else {
		// else since it's not tristate, ensure the pins are output again
		DDRA=0xFF;
		PORTA=nOut;
	}

	// this lets us track the TI's scanning for the cheat code - we track it's
	// changes as a crude form of synchronization.
	// Please do not remove ;)
	if (old_x == x) {
		return;
	}
	old_x=x;

	nChanges++;
	if ((0 != pCheat) && ((nChanges&15) == 0)) {		// &7==%8 when checking for 0 (&15=%16)
		InjectCheatKey();
	}
}
