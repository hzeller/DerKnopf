# <h.zeller@acm.org>
##

DEFINES=-DF_CPU=8000000UL -DSERIAL_BAUDRATE=38400

TARGET_ARCH=-mmcu=attiny48
CC=avr-gcc
CXX=avr-g++
CXXFLAGS=-Os -g -W -Wall -Wno-unused-parameter -ffunction-sections -fdata-sections -mcall-prologues $(DEFINES)
CFLAGS=$(CXXFLAGS) -std=c99
AVRDUDE_DEVICE ?= /dev/ttyUSB0
AVRDUDE     = avrdude -p t48 -c stk500v2 -P $(AVRDUDE_DEVICE)
FLASH_CMD   = $(AVRDUDE) -e -U flash:w:main.hex
LINK=avr-g++ -g $(TARGET_ARCH) -Wl,-gc-sections
#OBJECTS=receiver.o quad.o serial-com.o i2c_master.o
OBJECTS=receiver.o quad.o i2c_master.o

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
# Attiny 48 fuse. internal oscillator. 8Mhz
### low byte:
# 7 chkdiv	1   division by 8: no.
# 6 chkout	1   clock output. No.
# 5 sut1	1-+ slow rising power (page 31)
# 4 sut0	0/
#
# 3 nc   	1
# 2 nc	        1
# 1 cksel1	1-+ internal RC 8Mhz
# 0 cksel0	0/
fuse:
	$(AVRDUDE) -U lfuse:w:0xee:m
