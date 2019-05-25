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

void StartCmd();
int send_cmd(unsigned char cmd, unsigned char par);
int getps2char(void);
void put_kbbuff(unsigned char c);
void decode(unsigned char sc);
void init_kb(void);
void UpdateKbLEDs();
unsigned char remapnumlock(unsigned char in);
unsigned char remapscrolllock(unsigned char in);
void InjectCheatKey();

#define KEY_LEDS 0xED, 1
#define KEY_RESET 0xFF, 1

#define LED_SCROLL 0x01
#define LED_NUM    0x02
#define LED_CAPS   0x04

#define DATA_LOW	PORTD&=~0x08; DDRD|=0x08;
#define DATA_HIGH	DDRD&=~0x08;  PORTD|=0x08;

#define CLOCK_LOW		cli(); GICR=0; DDRD|=0x04; PORTD&=~0x04;
#define CLOCK_NORMAL	edge=0; DDRD&=~0x04; MCUCR=2; GICR=(1<<INT0); bitcount=11; GIFR=0xff; sei();