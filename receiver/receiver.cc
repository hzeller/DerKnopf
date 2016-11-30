/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Copyright (c) h.zeller@acm.org. GNU public License.
 *
 * Receiver for 'DerKnopf'.
 *   - Input something like TSOP38238 for infrared.
 *   - Also, we have a local rotational quadrature encoder.
 *   - Output digipot is a DS1882
 *   - A charlie-plexed LED ring shows the current pos.
 */

#define DO_SERIAL_COM 0

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include "quad.h"
#include "clock.h"
#include "i2c_master.h"

#if DO_SERIAL_COM
#  include "serial-com.h"
#else
typedef int SerialCom;  // dummy type.
// TODO: a version that sends via SPI
#endif

#define IR_PORT_IN  PIND
#define IR_PORT_OUT PORTD
#define IR_IN       (1<<3)

#define QUAD_PORT_IN  PINB
#define QUAD_PORT_OUT PORTB
#define QUAD_SHIFT    6
#define QUAD_IN       (3<<QUAD_SHIFT)

#define BUTTON_PORT_IN  PINA
#define BUTTON_PORT_OUT PORTA
#define BUTTON_IN       (1<<2)

#define DIGIPOT_READ  0x51
#define DIGIPOT_WRITE 0x50

// If histogram shift is defined, we spit out a histogram. And it only makes
// sense if we do serial.
#if DO_SERIAL_COM
#  define HISTOGRAM_SHIFT 3
#endif

static inline uint8_t quad_in() {
    // Flipping one bit as we get the signal in the wrong sequence.
    return ((QUAD_PORT_IN & QUAD_IN) >> QUAD_SHIFT) ^ 0b01;
}
static inline bool infrared_in() { return (IR_PORT_IN & IR_IN) != 0; }
static inline bool button_in() { return (BUTTON_PORT_IN & BUTTON_IN) == 0; }

struct EepromLayout {
    // The first character sometimes seems to be wiped out in power-glitch
    // situations; so let's not store anything of interest here.
    uint8_t dummy;

    uint8_t value;
    uint8_t is_muted;
};

// EEPROM layout with some defaults in case we'd want to prepare eeprom flash.
struct EepromLayout EEMEM ee_data = { 0, 0, 0 };

#if DO_SERIAL_COM
static uint8_t histogram[255];
static char to_hex(unsigned char c) { return c < 0x0a ? c + '0' : c + 'a' - 10; }
static void printHexByte(SerialCom *out, unsigned char c) {
    out->write(to_hex((c >> 4) & 0xf));
    out->write(to_hex((c >> 0) & 0xf));

}
static void PrintString(SerialCom *out, const char *str) {
    while (*str)
        out->write(*str++);
}
#endif

// (lifted from my other project, rc-screen)
static uint8_t read_infrared(uint8_t *buffer, SerialCom *com) {
    // The infrared input is default high.
    // A transmission starts with a long low phase (which triggered us to
    // be in this routine in the first place), followed by a sequence of bits
    // that are encoded in the duration of the high-phases. We interpret that as
    // long == 1, short == 0. The end of the signal is reached once we see the
    // high phase to be overly long (or: when 4 bytes are read).
    // The timings were determined empirically.
    uint8_t read = 0;
    uint8_t current_bit = 0x80;
    *buffer = 0;

    // manual measurment.
    //const unsigned short lo_hi_bit_threshold = 0x01CE;
    const unsigned short lo_hi_bit_threshold = 0x0318;
    const unsigned short end_of_signal = 10 * lo_hi_bit_threshold;
    unsigned short count = 0;

#ifdef HISTOGRAM_SHIFT
    const uint8_t* print_buffer = buffer;
    uint8_t divider = lo_hi_bit_threshold >> HISTOGRAM_SHIFT;
#endif

    while (read < 4) {
        while (!infrared_in()) {}  // skip low phase, wait for high.
        for (count = 0; infrared_in() && count < end_of_signal; ++count)
            ;
        if (count >= end_of_signal)
            break; // we're done - final high state.
        if (count > lo_hi_bit_threshold) {
            *buffer |= current_bit;
        }

#ifdef HISTOGRAM_SHIFT
        count >>= HISTOGRAM_SHIFT;
        if (count > 255) count = 255;
        histogram[count]++;
#endif

        current_bit >>= 1;
        if (!current_bit) {
            current_bit = 0x80;
            ++read;
            if (read == 4)
                break;
            ++buffer;
            *buffer = 0;
        }
    }
#ifdef HISTOGRAM_SHIFT
    PrintString(com, "hist: [");
    for (int i = 0; i < 255; ++i) {
        if (i == divider) {
            com->write('|');
        }
        if (histogram[i]) {
            printHexByte(com, i);
            com->write(':');
            printHexByte(com, histogram[i]);
            com->write(' ');
        }
    }
    PrintString(com, "]");
    printHexByte(com, print_buffer[0]);
    printHexByte(com, print_buffer[1]);
    printHexByte(com, print_buffer[2]);
    printHexByte(com, print_buffer[3]);
    PrintString(com, "\r\n");
#endif
    return read;
}

class DebouncedButton {
public:
    DebouncedButton() : is_on_(false), debounce_count_(0) {}

    bool DetectEdge(bool input) {
        if (!input) {
            is_on_ = false;
            debounce_count_ = 0;
            return false;
        }
        else if (is_on_) {
            return false;
        }
        else if (++debounce_count_ < 100) {
            return false;
        }
        is_on_ = true;
        debounce_count_ = 0;
        return true;
    }

private:
    bool is_on_;
    uint16_t debounce_count_;
};

struct CharlieLookup {
    uint8_t row;
    uint8_t col;
};
CharlieLookup ledData[30];

void InitLedData() {
    for (int i = 0; i < 30; ++i) {
        uint8_t row = i / 5;
        uint8_t col = i % 5;
        if (col >= row) col += 1;

#if 1
        // If cables rotated.
        row = 5 - row;
        col = 5 - col;
#endif

        if (row > 2) row += 2;
        if (col > 2) col += 2;
        ledData[i].row = row;
        ledData[i].col = col;
    }
}

void led_output(uint8_t value, bool on) {
    CharlieLookup &data = ledData[value];
    DDRD = (1 << data.row) | (1 << data.col);
    PORTD = ((on ? 1 : 0) << data.row);
}

void ds1882_init() {
    if (i2c_start(DIGIPOT_WRITE) == 0) {
        i2c_write(0x86);  // set to 63 step mode.
        i2c_stop();
    }
}

uint8_t value_mapping[30] = {
    0,  1, 2, 3, 4, 5,   // 1 step.
    7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 31, 33, 35, 37, 39,  // 2 step.
    42, 45, 48, 51, 54, 57, 60, 63  // 3 step.
};
void ds1882_set_pot_value(uint8_t value, bool muted) {
    if (muted) value = 0;

    // Value can be 0..29. The DS1882 has a range 0..63 with the
    // highest value being the most attenuation.
    uint8_t wiper = 63 - value_mapping[value];

    if (i2c_start(DIGIPOT_WRITE) == 0) {
        i2c_write((0 << 6) | wiper);
        i2c_write((1 << 6) | wiper);
        i2c_stop();
    }
}

inline static uint8_t GetEEValue(uint8_t* which) { return eeprom_read_byte(which); }
inline static uint8_t SetEEValue(uint8_t* which, uint8_t value) {
  eeprom_write_byte(which, value);
  return value;
}

int main() {
    InitLedData();
    Clock::init();
    i2c_init();
    ds1882_init();

    // Set pullups.
    IR_PORT_OUT |= IR_IN;
    BUTTON_PORT_OUT |= BUTTON_IN;
    QUAD_PORT_OUT |= QUAD_IN;

    SerialCom com;
    QuadDecoder knob(quad_in());
    DebouncedButton button;
    uint8_t buffer[4];

    // Set initial values we have kept in EEPROM
    int16_t pot_pos = GetEEValue(&ee_data.value);
    bool muted = GetEEValue(&ee_data.is_muted);

    // Default EEPROM value is 0xff the first time we power up. Make sure
    // we're in range.
    if (pot_pos < 0 || pot_pos > 29)
        pot_pos = 0;

    ds1882_set_pot_value(pot_pos, muted);

    bool change_needs_writing = false;
    Clock::cycle_t change_needs_writing_start;

    // The optical encoder needs some settle-time it seems. Discard changes
    // until we see 100ms of no change.
    Clock::cycle_t last_encoder_change = Clock::now();
    while (Clock::now() - last_encoder_change < Clock::ms_to_cycles(100)) {
        if (knob.UpdateEnoderState(quad_in()) != 0)
            last_encoder_change = Clock::now();
    }

    for (;;) {
        int16_t old_pos = pot_pos;

        if (button.DetectEdge(button_in())) {
            muted = !muted;
            old_pos = -1;  // force redraw
        }

        if (!infrared_in() && read_infrared(buffer, &com) == 4) {
            // Our sender.
            if (buffer[0] == 'm' && buffer[1] == 'o' && buffer[2] == 'r' && buffer[3] == 'e') {
                ++pot_pos;
            }
            else if (buffer[0] == 'l' && buffer[1] == 'e' && buffer[2] == 's' && buffer[3] == 's') {
                --pot_pos;
            }
            else if (buffer[0] == 'b' && buffer[1] == '_' && buffer[2] == 'o' && buffer[3] == 'n') {
                muted = !muted;
                old_pos = -1;
            }
        }

#if DO_SERIAL_COM && defined(HISTOGRAM_SHIFT)
        for (int i = 0; i < 255; ++i) histogram[i] = 0;
#endif

        pot_pos += knob.UpdateEnoderState(quad_in());

        if (pot_pos < 0) pot_pos = 0;
        if (pot_pos > 29) pot_pos = 29;

        if (old_pos != pot_pos) {
            ds1882_set_pot_value(pot_pos, muted);
#if DO_SERIAL_COM
            com.write((pot_pos / 10) + '0');
            com.write((pot_pos % 10) + '0');
            if (muted) { com.write(' '); com.write('M'); }
            com.write(' ');
            com.write('\r');
            com.write('\n');
#endif
            change_needs_writing = true;
            change_needs_writing_start = Clock::now();
        }

        bool is_on = !muted || ((Clock::now() & 0x1FFF) < 0xFFF);
        led_output(pot_pos, is_on);

        // Write current setting to eeprom, but only after it has been settled
        // for a while not to wear out the eeprom.
        if (change_needs_writing &&
            (Clock::now() - change_needs_writing_start > Clock::ms_to_cycles(1000))) {
            SetEEValue(&ee_data.value, pot_pos);
            SetEEValue(&ee_data.is_muted, muted);
#if DO_SERIAL_COM
            com.write('w');
            com.write('\r');
            com.write('\n');
#endif
            change_needs_writing = false;
        }
    }
}
