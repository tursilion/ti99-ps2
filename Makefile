###############################################################################
# Makefile for the project KeyboardGCC
###############################################################################

## General Flags
PROJECT = KeyboardGCC
MCU = atmega16
TARGET = KeyboardGCC.elf
CC = avr-gcc.exe

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -funroll-loops -DF_CPU=16000000 -O3 -funsigned-char -fshort-enums  -g0
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=KeyboardGCC.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0


## Include Directories
INCLUDES = -I"C:\Program Files\WinAVR\avr\include" 

## Objects that must be built in order to link
OBJECTS = KeyboardGCC.o Serial.o kb.o gpr.o ti.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) KeyboardGCC.hex KeyboardGCC.eep KeyboardGCC.lss

## Compile
KeyboardGCC.o: ../KeyboardGCC.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

Serial.o: ../Serial.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

kb.o: ../kb.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

gpr.o: ../gpr.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

ti.o: ../ti.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) KeyboardGCC.elf dep/* KeyboardGCC.hex KeyboardGCC.eep KeyboardGCC.lss KeyboardGCC.map


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

