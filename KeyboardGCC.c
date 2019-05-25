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
// Based on the AVR sample code, adapted to the Mega16
// by M.Brent/Tursi
// 
// Hooking it up:
// PortA is all output to CRU (rows)
// 0 - >0006
// 1 - >0008
// 2 - >000A
// 3 - >000C
// 4 - >000E
// 5 - >0010
// 6 - >0012
// 7 - >0014
//
// PortB is all output to LEDs (for debugging)
// 0 - key release code coming
// 1 - extended keycode coming
// 2 - shift key down
// 3 - caps lock active
// 4 - serial data out
// 5 - waiting serial data out to come ready/keyboard cmd out
// 6 - keyboard data in
// 7 - system ready/alpha lock scanned
//
// PortC is all input from keyboard (columns):
// 0 - Column 0 (pin 12) LSB
// 1 - Column 1 (pin 13) 
// 2 - Column 2 (pin 14) 
// 3 - Column 3 (pin 15) 
// 4 - Column 4 (pin 9)  
// 5 - Column 5 (pin 8)  
// 6 - Alpha lock (pin 6)
// 7 - future use (wire to joystick 2/pin 7 for mouse support?) MSB
//
// PortD is general IO:
// 0 - Serial RX for debugging only (disabled in release builds) (115.2k 8N1)
// 1 - Serial TX for debugging only (disabled in release builds) (115.2k 8N1)
// 2 - PS/2 Clock input (interrupt 0)
// 3 - PS/2 Data IO
// 4 - Optional output - Alt-F9 pressed
// 5 - Optional output - Alt-F10 pressed
// 6 - Optional output - Alt-F11 pressed
// 7 - Optional output - Alt-F12 pressed
//
// Run this puppy at 16MHz or 20MHz (external clock), O3 seems fine
// 20 is preferred, occasional glitches at 16.

#include <avr/io.h>
#include <avr/interrupt.h>
#include "gpr.h"
#include "serial.h"
#include "kb.h"
#include "ti.h"

extern const signed char scan2ti994aflat[128][4];
extern volatile unsigned char kbtestok;

int main(void)
{
	unsigned char key;
	int nCntdown;

#if 0
	// little code to test the memory layout of the table	
	signed char row;
	signed const char *pDat;
	pDat=scan2ti994aflat[1];
	row=pgm_read_byte_near(pDat);
	pDat++;
	row=pgm_read_byte_near(pDat);
	pDat++;
	row=pgm_read_byte_near(pDat);
	pDat++;
	row=pgm_read_byte_near(pDat);
	pDat++;
#endif

	// startup the system
	low_level_init();	// init hardware
	init_uart(); 	// Initializes the UART transmit buffer
	init_kb(); 		// Initialize keyboard reception

	CheckTIPolling = &CheckTIPollingFake;
//	CheckTIPolling = &CheckTIPollingReal;


	putserialchar('1');
	putserialchar('.');
//	delay(100);

	kbtestok=0;
	// wait and see if the keyboard reports okay on it's own
	nCntdown=100;	// about 1s
	while (nCntdown--) {
		if (kbtestok) {
			break;
		}
		delay(1);
	}

	if (kbtestok == 0) {
		putserialchar('2');
		putserialchar('.');
		send_cmd(KEY_RESET);

		while (kbtestok==0) {
			delay(1);
		}
	}

	// we believe we have a keyboard now! So activate!
	CheckTIPolling = &CheckTIPollingReal;

	putserialchar('3');
	putserialchar('.');
	
	UpdateKbLEDs();

	flipled(7);

	putserialchar('O');
	putserialchar('K');
	putserialchar(' ');

	while(1)
	{
		key=getps2char();
		flipled(6);
		putserialchar(key);
	}
}
