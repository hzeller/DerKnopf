# <h.zeller@acm.org>
##

DEFINES=-DF_CPU=4000000UL
TARGET_ARCH=-mmcu=attiny44
CXX=avr-g++
CXXFLAGS=-O3 -g -W -Wall -ffunction-sections -fdata-sections -fshort-enums $(DEFINES)
AVRDUDE_DEVICE ?= /dev/ttyUSB0
AVRDUDE     = avrdude -p attiny44 -c stk500v2 -P $(AVRDUDE_DEVICE)
FLASH_CMD   = $(AVRDUDE) -e -U flash:w:main.hex
LINK=avr-g++ -g $(TARGET_ARCH) -Wl,-gc-sections
OBJECTS=transmitter.o quad.o

all : main.hex

main.elf: $(OBJECTS)
	$(LINK) -o $@ $(OBJECTS)
	avr-size $@

disasm: main.elf
	avr-objdump -C -S main.elf

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex

eeprom.hex: main.elf
	avr-objcopy -j .eeprom --change-section-lma .eeprom=0 -O ihex main.elf $@

read-eeprom:
	$(AVRDUDE) -qq -U eeprom:r:-:r | od -t x1zu2

flash: main.hex
	$(FLASH_CMD)

eeprom-flash: eeprom.hex
	$(AVRDUDE) -U eeprom:w:eeprom.hex

clean:
	rm -f $(OBJECTS) main.elf main.hex

# Documentation page references from
# attiny24/44 documentation, page 160
# internal oscillator.
### Fuse high byte:
# 7 rstdisbl	1   disable external reset: disabled (i.e.: reset enabled).
# 6 dwen        1   debug wire enable
# 5 spien	0   SPI programming: enabled.
# 4 wdton	1   don't need watchdog. Save power.
#
# 3 eesave      0   save eeprom on chip erase: disabled (we don't use eeprom)
# 2 bodlevel2	1   No brown-out
# 1 bodlevel1	1
# 0 bodlevel0   1

### low byte:
# 7 chkdiv8	0   Default division by 8 (needed as we op with 3V)
# 6 ckout       1   clock out (no)
# 5 sut1	0-+ fast startup to react on interrupts quickly (page 27)
# 4 sut0	0/
#
# 3 cksel3	0\
# 2 cksel2	0 + internal RC oscillator (8Mhz) page 27
# 1 cksel1	1 |
# 0 cksel0	0/
fuse:
	$(AVRDUDE) -U hfuse:w:0xd7:m -U lfuse:w:0x42:m
