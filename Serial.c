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
#include <stdio.h>
#include <avr/io.h>
#include "gpr.h"
#include "serial.h"
#include "ti.h"

void init_uart(void)
{
	// set up serial port and the PS2 lines on port D
	// (init_kb() sets up the interrupt lines)
	UBRRH = 0;		// writes UBRRH

	// set to 115,200 baud based on CPU speed
#if F_CPU == 16000000
	UBRRL = 8;
#elif F_CPU == 20000000
	UBRRL=10;
#else
#error CPU Speed not correctly set (F_CPU)
#endif

	UCSRA = 0;		// clear all errors and flags
	UCSRB = (1<<TXEN);	// TX enable 
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);	// write UCSRC, 8N1
}

#ifdef DEBUGMODE
// relies on the Mega16 TX buffers
void putserialchar(int c) {
	// wait for the chip to mark ready...
	while ( !( UCSRA & (1<<UDRE)) ) {
		flipled(5);
	}
	// write the data
	UDR=c;
	flipled(4);
}
#endif
