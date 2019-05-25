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
#include <avr/interrupt.h>
#include "gpr.h"
#include "serial.h"
#include "kb.h"
#include "ti.h"

int low_level_init(void)
{
	// Set up port A for output to the TI CRU
//	DDRA=0xff;	// set as output
//	PORTA=0xff;	// all off (data is inverted!)
	TRISTATE_TO_TI;		// leave outputs off until we find a keyboard

	// Set up the LEDs on port B so I can see if anything happens ;)
	DDRB=0xff;	// set as output
	PORTB=0xff;	// all LEDs off

	// Set up Port C for input from the TI column select
	DDRC=0x00;	// now we set the lines to all input
	PORTC=0xff;	// enable internal pull ups

	// Set up Port D data line (PD3). 
//	DDRD=0xff;	// first, we set all outputs to latch our 0 value
//	PORTD=0x00;	// latch zeros
	DDRD=0x00;	// Now set as inputs
	PORTD=0xff;	// And enable pull-ups. 

	return 1;
}

#ifdef DEBUGMODE
void flipled(char led) {
	PORTB^=(1<<led);
}
#endif

#ifdef DEBUGMODE
void print_hexbyte(unsigned char i)
{
	unsigned char h, l;
	
	h = i & 0xF0; // High nibble
	h = h>>4;
	h = h + '0';
	if (h > '9')
	h = h + 7;
	l = (i & 0x0F)+'0'; // Low nibble
	
	if (l > '9')
		l = l + 7;
	
	putserialchar(h);
	putserialchar(l);
}
#endif

// external function for keyboard timeouts
extern void resetstate();
extern volatile int timeout;

// hand tuned to delay about 1/100th second at 16MHz with -O3
void delay(unsigned char d)
{
	// the volatile prevents the compiler from doing a lot of optimizing of these loops
	volatile unsigned char i,j,k;

	if (d) {
		PORTA=0xff;

		for(i=0; i<d; i++) {
			for(j=0; j<50; j++) {
				for(k=0; k<80; k++) {
					CheckTIPolling();
				}
	   		}
			if (timeout) {
				timeout--;
				if (!timeout) {
					resetstate();
				}
			}
		}
	} else {
		if (timeout) {
			timeout--;
			if (!timeout) {
				resetstate();
			}
		}
	}
}
