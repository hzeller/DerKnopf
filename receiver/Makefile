# <h.zeller@acm.org>
##

DEFINES=-DF_CPU=8000000UL -DSERIAL_BAUDRATE=38400

TARGET_ARCH=-mmcu=atmega8
CC=avr-gcc
CXX=avr-g++
CXXFLAGS=-Os -g -W -Wall -ffunction-sections -fdata-sections -mcall-prologues $(DEFINES)
CFLAGS=$(CXXFLAGS) -std=c99
AVRDUDE_DEVICE ?= /dev/ttyUSB0
AVRDUDE     = avrdude -p m8 -c stk500v2 -P $(AVRDUDE_DEVICE)
FLASH_CMD   = $(AVRDUDE) -e -U flash:w:main.hex
LINK=avr-g++ -g $(TARGET_ARCH) -Wl,-gc-sections
OBJECTS=receiver.o quad.o serial-com.o i2c_master.o

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
# http://www.atmel.com/images/atmel-2486-8-bit-avr-microcontroller-atmega8_l_datasheet.pdf / pp 216-217 are interesting.
# internal oscillator.
### Fuse high byte:
# 7 rstdisbl	1   disable external reset: disabled (i.e.: reset enabled).
# 6 wdton	1   watchdog.
# - spien	0   serial programming: enabled.
# 4 chkopt	1   clock options (doesn't matter, we do internal RC)
#
# 3 eesave      0   save eeprom on chip erase: enabled
# 2 bootsz1	1
# 1 bootsz0	1
# 0 bootrst     1   starting at 0x0000

### low byte:
# 7 bodlevel	1   brown out detect at 2.7v
# 6 boden	0   brown out enable.
# 5 sut1	1-+ slow rising power (page 30)
# 4 sut0	0/
#
# 3 cksel3	0\
# 2 cksel2	1 + internal RC oscillator  (page 30)  (8Mhz)
# 1 cksel1	0/
# 0 cksel0	0
fuse:
	$(AVRDUDE) -U hfuse:w:0xd7:m -U lfuse:w:0xe4:m
