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
#include <string.h>
#include "gpr.h"
#include "kb.h"
#include "scancodes.h"
#include "serial.h"
#include "ti.h"

#define BUFF_SIZE 64
//#define DEBUG_KB_ERRS

unsigned char edge, bitcount;// 0 = neg. 1 = pos.
unsigned char kb_buffer[BUFF_SIZE];
unsigned char *inpt, *outpt;
unsigned char buffcnt;

// used to count down between bits
volatile int timeout;
#define TIMEOUTCOUNT 0xffff
// Note: this means we can never resend the Diagnostic echo command
#define DUMMY_OUT 0xEE

// keyboard commands out
volatile unsigned char outcmd;
volatile unsigned char outparity;
volatile signed char outcount;
volatile unsigned char lastout=DUMMY_OUT;
volatile unsigned char lastpar;
volatile unsigned char outstarted;
volatile unsigned char ack_cmd, ack_parity, ack_valid;
volatile unsigned char kbtestok=0;

// keyboard state (note commented zeros indicate vars that are assumed initialized to zero!)
unsigned char is_up /*=0*/, isextended /*= 0*/, shiftstate/*=0*/, capslock=1, lockedshiftstate/*=0*/;
unsigned char scrolllock/*=0*/, numlock=1;
unsigned char fctnrefcount,shiftrefcount,ctrlrefcount;
unsigned char ignorecount;	// used to ignore the break key
unsigned char ticols[8]={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
unsigned char cheatcode[10] /*={0,0,0,0,0,0,0,0,0,0};*/;
// cheat code is up,up,down,down,left,right,left,right,b,a (trigger with enter)
// because we don't save the meta information, it's also 88224646ba (digits on numpad)
unsigned const char cheatmatch[10]={ 0x75,0x75,0x72,0x72,0x6B,0x74,0x6B,0x74,0x32,0x1c };
volatile const unsigned char *pCheat/*=0*/;
volatile int abortCheat/*=0*/;
int cheatidx/*=0*/;			// cheat code buffer is a ring buffer to reduce overhead
// Track whether the last key required a fctn, shift, or control press
unsigned char fLastMeta/*=0*/;
unsigned char nLastRow=-1, nLastCol=-1;
unsigned char outputclear/*=0*/;	// used for controlling external pins

enum LASTMETA {
	METANONE = 0,

	METAFCTN,
	METACTRL,
	METASHIFT
};

#define SHIFT_COL 7
#define FCTN_ROW 4
#define SHIFT_ROW 5
#define CTRL_ROW 6

void init_kb(void)
{
	inpt = kb_buffer;	// Initialize buffer
	outpt = kb_buffer;
	buffcnt = 0;
	outcount = -1;

	ack_valid=0;		// no acknowledge byte pending
	fctnrefcount=0;

	shiftrefcount=0;
	ctrlrefcount=0;
	ignorecount=0;

	DATA_HIGH;			// high is idle
	CLOCK_NORMAL;		// activates input interrupts
}

void resetstate() {
	// called after no activity from the kb for a while, 
	// we just reset our counters to correct any potential misalignments

	// else, let's reset our state machine
	// but don't reset the TI's state machine
	if (outstarted) {
		// abort our command
		CLOCK_LOW;
	}

	putserialchar('*');

	inpt = kb_buffer;	// Initialize buffer
	outpt = kb_buffer;
	buffcnt = 0;
	outcount = -1;
	ignorecount=0;

//	ack_valid=0;	// if that ack ever comes in, we will need something to feed the keyboard

	bitcount = 11;

	if (outstarted) {
		delay(1);
		CLOCK_NORMAL;
		outstarted=0;
	}
}

void StartCmd() {
	// To send a command, we start by pulling the data line low, then we just send data
	// with the clocks. To prevent racing with the keyboard, it's correct to pull the clock
	// line low for 60uS or more first, and release the clock after lowering data
	volatile char k,j;
	j=0;

	if (outstarted) {
		return;
	}

	// This does the clock reset (I think)
	CLOCK_LOW;	// inhibit clock
	
	for (j=0; j<5; j++) {
		for (k=0; k<90; k++);
	}

	DATA_LOW;	// start bit ready

	for (j=0; j<5; j++) {
		for (k=0; k<90; k++);
	}

	outstarted=1;
	lastout=outcmd;
	lastpar=outparity;

	PORTB=0xdf;

	CLOCK_NORMAL;	// allow clock again
}

// returns 0 if the previous command is still sending
int send_cmd(unsigned char cmd, unsigned char par) 
{

#ifdef DEBUGMODE
	print_hexbyte(cmd);
#endif

	outcmd=cmd;
	outparity=par;
	outstarted=0;
	outcount=10;

   	StartCmd();

  	return 1;
}
 
//SIGNAL(SIG_INTERRUPT0)
ISR(INT0_vect)
{
	static unsigned char data;	// Holds the received scan code or output data

	flipled(6);
	timeout=TIMEOUTCOUNT;		// we got a bit, make sure we wait for another
	if (outstarted) {
		// we're sending data!
		if (edge) 				// Routine entered at rising edge
		{
			MCUCR = 2;			// Set next interrupt on falling edge
			edge = 0;
		} else { 				// Routine entered at falling edge
			MCUCR = 3;			// Set next interrupt on rising edge
			edge = 1;
			
			if (outcount >= 0) {
				// load our data bit
				if (outcount > 2) {
					flipled(0);
					if (outcmd&0x01) {
						DATA_HIGH;
	              	} else {
						DATA_LOW;
					}
					outcmd>>=1;
	  			} else {
					if (outcount == 2) {
						PORTB&=0xfd;
						// parity
						if (outparity) {
							DATA_HIGH;
		              	} else {
							DATA_LOW;
						}
					} else {
						if (outcount == 1) {
							// stop bit
							DATA_HIGH;
						} else {
							if (outcount == 0) {
								// otherwise, we're waiting for an ack
								if (0 == (PIND & 8)) {
									outstarted=0;
									bitcount=11;
									PORTB|=0x20;
								} else {
									outcount++;
								}
							} 
						}
					}
				}
				outcount--;
			}
		}
	} else {
		// we're receiving data!
		if (!edge) 						// Routine entered at falling edge
		{
			if(bitcount < 11 && bitcount > 2)	// Bit 3 to 10 is data. Parity bit,
			{ 									// start and stop bits are ignored.
				data = (data >> 1);
				if (PIND & 8) {
					data = data | 0x80;	// Store a '1'
				}
			}
			
			bitcount--;

			MCUCR = 3;					// Set next interrupt on rising edge
			edge = 1;
		} else { 						// Routine entered at rising edge
			if(bitcount == 0)			// All bits received
			{
				// if the cheat code was running, we need to finish it off at the end of the current
				// line - we can't touch the state till it's done
				if (pCheat) {
					abortCheat=1;
				} else {
					decode(data);
				}

				bitcount = 11;
				// Check for pending output data
				if (outcount>0) {
				  StartCmd();
	            }
			}

			MCUCR = 2;					// Set next interrupt on falling edge
			edge = 0;
		}
	}
}

unsigned char getextbit(unsigned char sc) {
	unsigned char bit=0x00;

	CheckTIPolling();

	switch (sc) {
	case 0x01:	// F9
		bit=0x10;
		putserialchar('%');
		putserialchar('4');
		break;

	case 0x09:	// F10
		bit=0x20;
		putserialchar('%');
		putserialchar('5');
		break;

	case 0x78:	// F11
		bit=0x40;
		putserialchar('%');
		putserialchar('6');
		break;

	case 0x07:	// F12
		bit=0x80;
		putserialchar('%');
		putserialchar('7');
		break;
	}

	return bit;
}

void handleexton(unsigned char sc) {
	// set an external pin (low) based on scancode passed (F9-F12)
	unsigned char bit;

	bit=getextbit(sc);

	if (0 != bit) {
		// set bit to low output and set clear code
		PORTD&=~bit;
		DDRD|=bit;
		outputclear|=bit;
	}
}

void handleextoff(unsigned char sc) {
	// clear an external pin (tristate) based on scancode passed (F9-F12)
	unsigned char bit;

	bit=getextbit(sc);

	if (0 != bit) {
		// set bit to input
		DDRD&=~bit;
		PORTD|=bit;
		outputclear&=~bit;
		putserialchar('%');
	}
}

void ParseRowCol(signed char row, signed char col, unsigned char fup) {
	if ((-1 == row) || (-1 == col)) {
		return;
	}

	if (col == SHIFT_COL) {
		if (row == FCTN_ROW) {
			// FCTN
			if (fup) {
				if (fctnrefcount) fctnrefcount--;
			} else {
				fctnrefcount++;
			}
			if (fctnrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
			}
			return;
		}
		if (row == SHIFT_ROW) {
			// Shift
			if (fup) {
				if (shiftrefcount) shiftrefcount--;
			} else {
				shiftrefcount++;
			}
			if (shiftrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
			}
			return;
		}
		if (row == CTRL_ROW) {
			// Ctrl
			if (fup) {
				if (ctrlrefcount) ctrlrefcount--;
			} else {
				ctrlrefcount++;
			}
			if (ctrlrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
			}
			return;
		}
	}
	if (!fup) {
		ticols[col]&=(unsigned char)~(1<<row);
	} else {
		ticols[col]|=(unsigned char)(1<<row);
	}
}

// new decode for TI
void decode(unsigned char sc)
{
	static unsigned short nLastChar=0x00ff;	// used to detect autorepeat and ignore it
	const signed char *pDat;

#ifdef DEBUGMODE
	print_hexbyte(sc);
#endif

	// block ALT-= unless it's ALT-CTRL-=
	if (sc == 0x55) {		// '='
		if ((ctrlrefcount <= 0) && (fctnrefcount > 0)) {
			// return if FCTN is pressed but not CTRL
			return;
		}
	}

	if (ignorecount > 0) {
		if (sc != 0xf0) {
			ignorecount--;
		}
		return;
	}

	// else parse the key
	switch (sc) {
		case 0xFA:	// keyboard acknowledge
			if (ack_valid != 0) {
				send_cmd(ack_cmd,ack_parity);
			}
			ack_valid=0;
			ack_parity=0;
//			putserialchar('A');
//			putserialchar('C');
//			putserialchar('K');
			break;

		case 0xAA:	// keyboard self test passed
			kbtestok=1;
			fctnrefcount=0;
			shiftrefcount=0;
			ctrlrefcount=0;

#ifdef DEBUG_KB_ERRS
			old=PORTB;
			PORTB=0x00;
			delay(25);
			PORTB=old;
			delay(50);
//			putserialchar('O');
//			putserialchar('K');
#endif
			break;

		case 0xEE:	// echo response
			break;

		case 0xFE:	// resend command
			if (lastout != DUMMY_OUT) {
				send_cmd(lastout, lastpar);
			}

#ifdef DEBUG_KB_ERRS
			old=PORTB;
			PORTB=0x00;
			delay(25);
			PORTB=0xff;
			delay(25);
			PORTB=0x00;
			delay(25);
			PORTB=old;
			delay(50);
//			putserialchar('R');
//			putserialchar('e');
//			putserialchar('s');
#endif
			break;

		case 0xfc:	// keyboard error - keyboard is disabled, so we need to reset it
			kbtestok=0;
			send_cmd(KEY_RESET);
			break;

		case 0x00:
		case 0xff:	// both of these are errors or buffer overflows
			// TODO: we should take some action here
#ifdef DEBUG_KB_ERRS
			old=PORTB;
			PORTB=0x00;
			delay(25);
			PORTB=0xff;
			delay(25);
			PORTB=0x00;
			delay(25);
			PORTB=0xff;
			delay(25);
			PORTB=0x00;
			delay(25);
			PORTB=old;
			delay(50);
//			putserialchar('E');
//			putserialchar('r');
//			putserialchar('r');
#endif
			break;

		// special keys
		case 0xF0:	// key up
			is_up=1;
			break;

		// these cases mean that just pressing SHIFT can not be detected on the TI
		// this is a minor problem for the sake of bypassing ramdisk startup
		// The reason is that a shifted key on the PC may not be shifted on the TI,
		// so we don't want to activate it. The left and right Windows keys have been remapped to
		// shift for that case, assuming the keyboard boots faster than the ramdisk ;)
		case 0x12:	// left SHIFT
		case 0x59:	// right SHIFT
			if (!isextended) {
				if (is_up) {
					shiftstate=0;
				} else {
					shiftstate=1;
				}
			}
			isextended=0;
			is_up=0;
			break;

		case 0xe0:	// extended code follows
			isextended=1;
			break;

		case 0xe1:	// break sequence - ignore next two keys
			ignorecount=2;
			break;

		case 0x58:	// caps lock
			if (!isextended) {
				if (is_up) {
					is_up=0;
				} else {
					if (!ack_valid) {
						// don't tromp on top of another change
						capslock=!capslock;
						UpdateKbLEDs();
					}
				}
			}
			isextended=0;
			break;

		case 0x7e:	// scroll lock
			if (isextended) {
				// this is control-break, then, parse normally
				goto dodefault;
			}
			if (is_up) {
				is_up=0;
			} else {
				if (!ack_valid) {
					scrolllock=!scrolllock;
					UpdateKbLEDs();
				}
			}
			break;

		case 0x77:	// num lock
			if (!isextended) {
				if (is_up) {
					is_up=0;
				} else {
					if (!ack_valid) {
						numlock=!numlock;
						UpdateKbLEDs();
					}
				}
			}
			isextended=0;
			break;

#ifdef DEBUGMODE
		case 0x76:	// ESC override (note: code may crash if you hit this too fast!)
			if (!is_up) {
				int idx;
				putserialchar('\r');
				putserialchar('\n');
				putserialchar('S');
				print_hexbyte(shiftrefcount);
				putserialchar('\r');
				putserialchar('\n');
				putserialchar('C');
				print_hexbyte(ctrlrefcount);
				putserialchar('\r');
				putserialchar('\n');
				putserialchar('F');
				print_hexbyte(fctnrefcount);
				putserialchar('\r');
				putserialchar('\n');
				// and print the TI CRU map
				for (idx=0; idx<8; idx++) {
					print_hexbyte(ticols[idx]);
					putserialchar('\r');
					putserialchar('\n');
				}
			}
			break;
#endif

		// Check for Ctrl+Alt+Delete (reset the keyboard chip itself!)
		case 0x71:
			if (isextended) {
				// Delete key
				if (is_up) {
					// activate on release (note this uses the TI versions!)
					if ((ctrlrefcount > 0) && (fctnrefcount > 0)) {
						// Perform a reset
						void (*reset)()=NULL;
						cli();
						putserialchar('X');
						putserialchar('X');
						putserialchar('X');
						reset();
						// never returns
					}
				}
			}
			// if we make it here, then process normally
			goto dodefault;
			break;

		case 0x83:
			sc=0x7f;		// special case for F7, remap to less than 0x80
			goto dodefault;

		// check for magic hardware sequence Alt+F9 through Alt+F12
		// Must stay right above the default case so we can fall through!
		case 0x01:	// F9
		case 0x09:	// F10
		case 0x78:	// F11
		case 0x07:	// F12
			if (is_up) {
				if (outputclear) {
					handleextoff(sc);
					// clear the otherwise missed up handling
					lockedshiftstate=0;
					nLastChar=0x00ff;	// clear last char - now that we've released we can press again ;)
					isextended=0;
					is_up=0;
					break;
				}
			} else {
				if (fctnrefcount > 0) {
					// this is an external key request
					handleexton(sc);
					// clear potential flag that we'd otherwise miss
					isextended=0;
					break;
				}
			}
			// else, fall through to default and handle it
		default:	// any other key
dodefault:
			// certain keys are remapped for numlock and scroll lock
			if (!numlock) {
				sc=remapnumlock(sc);
			}
			if (scrolllock) {
				sc=remapscrolllock(sc);
			}

			CheckTIPolling();

			if (sc < 0x80) {
				unsigned short nThisChar;

				if (isextended) {
					pDat=scan2ti994aextend[sc];
				} else if ((shiftstate)||(lockedshiftstate)) {
					pDat=scan2ti994ashift[sc];
					lockedshiftstate=1;
				} else {
					pDat=scan2ti994aflat[sc];
				}

				// check for and ignore repeated characters
				// to avoid screwing up the meta key refcounts
				if (isextended) {
					nThisChar=0xe000|sc;
				} else {
					nThisChar=sc;
				}
					
				// Up codes don't autorepeat, so don't check them
				if ((is_up)||(nThisChar != nLastChar)) {
					signed char row1,col1=-1;
					signed char row2,col2=-1;
					
					nLastChar=nThisChar;

					row1=pgm_read_byte_near(pDat);
					if (-1 != row1) {
						col1=pgm_read_byte_near(pDat+1);
					}

					pDat+=2;

					row2=pgm_read_byte_near(pDat);
					if (-1 != row2) {
						col2=pgm_read_byte_near(pDat+1);
					}

					// fLastMeta tracks whether the last keypress
					// added a shift-style key that the user did not
					// explicitly press it, so we can turn it off it
					// we don't need it now. It can't help a string
					// of three keypresses. ;) The extra up event won't
					// cause a problem as the refcounting code can cope
					// with that.
					if (!is_up) {
						if ((METANONE != fLastMeta) && (col1 != SHIFT_COL) && (col2 != SHIFT_COL)) {
							switch (fLastMeta) {
								case METAFCTN:	// FCTN was added last
									if ((row1 != FCTN_ROW)&&(row2 != FCTN_ROW)) {
										// turn off last key (prevents errors on up event)
										ParseRowCol(FCTN_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break;

								case METACTRL:	// CTRL was added last
									if ((row1 != CTRL_ROW)&&(row2 != CTRL_ROW)) {
										// turn off last key (prevents errors on up event)
										ParseRowCol(CTRL_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break;

								case METASHIFT:	// SHIFT was added last
									if ((row1 != SHIFT_ROW) && (row2 != SHIFT_ROW)) {
										// turn off last key (prevents errors on up event)
										ParseRowCol(SHIFT_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break; 
							}
						}
					
						fLastMeta=METANONE;
					}

					if (-1 != row1) {
						ParseRowCol(row1,col1,is_up);
					}
					if (-1 != row2) {
						ParseRowCol(row2,col2,is_up);

						if (!is_up) {
							// Update meta - it may be either one, but it only counts when both were used!
							if (col1 == SHIFT_COL) {
								switch (row1) {
									case FCTN_ROW: fLastMeta=METAFCTN; break;
									case SHIFT_ROW: fLastMeta=METASHIFT; break;
									case CTRL_ROW: fLastMeta=METACTRL; break;
								}
								if (METANONE != fLastMeta) {
									putserialchar(';');
									nLastRow=row2;
									nLastCol=col2;
								}
							} else if (col2 == 7) {
								switch (row2) {
									case FCTN_ROW: fLastMeta=METAFCTN; break;
									case SHIFT_ROW: fLastMeta=METASHIFT; break;
									case CTRL_ROW: fLastMeta=METACTRL; break;
								}
								if (METANONE != fLastMeta) {
									putserialchar(';');
									nLastRow=row1;
									nLastCol=col1;
								}
							}
						}
					}
				}

				// check cheat codes - this is just to catch
				// people who dump the binary and copy it to resell,
				// and also for a bit of ego boost. Please do not
				// remove it. :)
				if (0 == pCheat) {
					if (!is_up) {
						if (0x5a != sc) {	// not Enter down
							cheatcode[cheatidx++]=sc;
							if (cheatidx>9) cheatidx=0;
						}
					} else {
						if (0x5a == sc) {	// is Enter up
							int x;
							int y;
							x=cheatidx;
							y=0;
							#ifdef DEBUGMODE
								putserialchar('\r');
								putserialchar('\n');
								putserialchar('>');
							#endif
							while (y < 10) {
								#ifdef DEBUGMODE
									print_hexbyte(cheatcode[x]);
									putserialchar(' ');
									putserialchar('=');
									putserialchar(' ');
									print_hexbyte(cheatmatch[y]);
									putserialchar('\r');
									putserialchar('\n');
								#endif

								if (cheatcode[x] != cheatmatch[y]) {
									break;
								}

								x++;
								if (x>9) x=0;
								y++;
							}
							
							if (y > 9) {
								// this is it - start the playback
								#ifdef DEBUGMODE
									putserialchar('!');
									putserialchar('!');
								#endif
								memset(cheatcode, 0, 10);
								pCheat=copyright;
								abortCheat=0;
							}
							#ifdef DEBUGMODE
								putserialchar('\r');
								putserialchar('\n');
							#endif

						}
					}
				} else {
					if ((abortCheat)&&(is_up)&&(0x5a == sc)) {
						pCheat=0;
						abortCheat=0;
					}
				}

				CheckTIPolling();
			}
			if (is_up) {
				lockedshiftstate=0;
				nLastChar=0x00ff;	// clear last char - now that we've released we can press again ;)
			}
			isextended=0;
			is_up=0;
			break;
	}

#ifdef DEBUGMODE
	// debug on LEDs
	PORTB&=(unsigned char)0xf0;
	PORTB|=(unsigned char)((unsigned char)~(is_up|(isextended<<1)|(shiftstate<<2)|(capslock<<3)))&0x0f;
#endif
}

void put_kbbuff(unsigned char c)
{
	if (buffcnt<BUFF_SIZE)						// If buffer not full
	{
		*inpt = c;								// Put character into buffer
		inpt++; 								// Increment pointer
		buffcnt++;
		if (inpt >= kb_buffer + BUFF_SIZE) {	// Pointer wrapping
			inpt = kb_buffer;
		}
	}
}

int getps2char(void)
{
	int byte;

	while(buffcnt == 0) {					// Wait for data
		CheckTIPolling();
		delay(0);							// just to handle the timeout
	}

	byte = *outpt;							// Get byte
	outpt++; 								// Increment pointer
	
	if (outpt >= kb_buffer + BUFF_SIZE) {	// Pointer wrapping
		outpt = kb_buffer;
	}

	buffcnt--; 								// Decrement buffer count
	return byte;
}

void UpdateKbLEDs() {
	char nCnt=0;
	unsigned char nCmd=0;

	// This function may take a while, and it is unlikely the user is
	// also pushing any relevant keys. Therefore, we clear our output
	// to the TI in case there's old data there
	PORTA=0xff;

	// As a workaround for ref counting errors, toggling any LED
	// will also reset all shift meta keys
	if (fctnrefcount) {
		fctnrefcount=0;
		ticols[7]&=(unsigned char)~(1<<4);
	}
	if (shiftrefcount) {
		shiftrefcount=0;
		ticols[7]&=(unsigned char)~(1<<5);
	}
	if (ctrlrefcount) {
		ctrlrefcount=0;
		ticols[7]&=(unsigned char)~(1<<6);
	}

	//delay(1);

	// now work out which lights are active
	if (capslock) {
		nCnt++;
		nCmd|=LED_CAPS;
	}
	if (scrolllock) {
		nCnt++;
		nCmd|=LED_SCROLL;
	}
	if (numlock) {
		nCnt++;
		nCmd|=LED_NUM;
	}
	if (nCnt&0x01) {
		ack_parity=0;
	} else {
		ack_parity=1;
	}
	ack_cmd=nCmd;
	ack_valid=5;	// valid for up to 5 timeout periods

	// Although this will weaken the TI response time, the user
	// is most likely waiting for the lights to change anyway
	while (0 == send_cmd(KEY_LEDS)) {
		delay(1);
	}
#ifdef DEBUG_MODE
	putserialchar('~');
#endif
	while (ack_valid != 0) {
		delay(1);
	}
}

// when numlock is off, the numeric keypad has alternate functions
unsigned char remapnumlock(unsigned char in) {
	// this is nice - the scan codes map the same as the real keys,
	// only they become extended
	if (isextended) {
		return in;
	}

	switch (in) {//Numpad digit
	case 0x69:	// 1
	case 0x6b:	// 4
	case 0x6c:	// 7
	case 0x70:	// 0
	case 0x71:	// Period
	case 0x72:	// 2
	case 0x74:	// 6
	case 0x75:	// 8
	case 0x7a:	// 3
	case 0x7d:	// 9
		isextended=1;	// lie and make it extended!
	}

	return in;
}

// when scroll lock is on, we remap the arrow keys to ESDX
unsigned char remapscrolllock(unsigned char in) {
	if (isextended) {
		switch (in) {
		case 0x75:
			isextended=0;
			return 0x24;	// E
		case 0x6b:
			isextended=0;
			return 0x1b;	// S
		case 0x72:
			isextended=0;
			return 0x22;	// X
		case 0x74:
			isextended=0;
			return 0x23;	// D
		}
	}
	return in;
}

// just embed some copyright info in the binary - please do not remove
const char szCTable[] = {
"REM (C) 2009 by M.Brent aka\n"
"REM Tursi- harmlesslion.com\n"
"REM Licensed for personal\n"
"REM use only.\n"
"REM This software may not\n"
"REM be re-distributed or\n"
"REM sold without written\n"
"REM permission from the\n"
"REM author.  V1.5a\n"
};

// Please do not remove
void InjectCheatKey() {
	// assuming pCheat is running, read the next scancode and feed it into the system
	unsigned char dat;

	if (0 != pCheat) {
		dat=pgm_read_byte_near(pCheat++);
		if (0 == dat) {
			pCheat=0;
		} else {
			decode(dat);
		}
		if (dat == 0xf0) {		// release code
			InjectCheatKey();	// inject the next one too
		}
	}
}
