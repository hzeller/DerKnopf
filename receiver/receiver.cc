/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Copyright (c) h.zeller@acm.org. GNU public License.
 *
 * Receiver for 'DerKnopf'. Input something like TSOP38238.
 *
 * TODO: switch ISR use from output to input. Instead of using the ISR for motor pulses, we
 * should use it to receive and precisely measure infrared input.
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "quad.h"
#include "clock.h"

#define IR_PORT PINC
#define IR_IN (1<<3)

#define QUAD_PORT PINB
#define QUAD_SHIFT 6
#define QUAD_IN (3<<QUAD_SHIFT)

#define BUTTON_PORT PINC
#define BUTTON_IN (1<<2)

static inline uint8_t quad_in() { return (QUAD_PORT & QUAD_IN) >> QUAD_SHIFT; }
static inline bool infrared_in() { return (IR_PORT & IR_IN) != 0; }
static inline bool button_in() { return (BUTTON_PORT & BUTTON_IN) == 0; }

// (lifted from my other project, rc-screen)
static uint8_t read_infrared(uint8_t *buffer) {
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
    const unsigned short lo_hi_bit_threshold = 0x01CE;
    const unsigned short end_of_signal = 10 * lo_hi_bit_threshold;
    unsigned short count = 0;

    while (read < 4) {
        while (!infrared_in()) {}  // skip low phase, wait for high.
        for (count = 0; infrared_in() && count < end_of_signal; ++count)
            ;
        if (count >= end_of_signal)
            break; // we're done - final high state.
        if (count > lo_hi_bit_threshold) {
            *buffer |= current_bit;
        }
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

        static const int shifted = 2;
        row += shifted;
        col += shifted;
        ledData[i].row = row;
        ledData[i].col = col;
    }
}

void led_output(uint8_t value, bool on) {
    CharlieLookup &data = ledData[value];
    DDRD = (1 << data.row) | (1 << data.col);
    PORTD = (PORTD & 0b11) | ((on ? 1 : 0) << data.row);
}

int main() {
    InitLedData();
    Clock::init();
    QuadDecoder knob;
    DebouncedButton button;
    uint8_t buffer[4];
    int16_t pot_pos = 0;
    bool muted = false;

    PORTC = IR_IN | BUTTON_IN;
    PORTB = QUAD_IN;  // Pullup.

    for (;;) {
        if (button.DetectEdge(button_in())) {
            muted = !muted;
        }

        if (infrared_in() && read_infrared(buffer) == 4) {
            if (buffer[0] == 'm' && buffer[1] == 'o' && buffer[2] == 'r' && buffer[3] == 'e') {
                ++pot_pos;
            }
            else if (buffer[0] == 'l' && buffer[1] == 'e' && buffer[2] == 's' && buffer[3] == 's') {
                --pot_pos;
            }
        }

        pot_pos += knob.UpdateEnoderState(quad_in());

        if (pot_pos < 0) pot_pos = 0;
        if (pot_pos > 29) pot_pos = 29;
        bool is_on = !muted || ((Clock::now() & 0x1FFF) < 0xFFF);
        led_output(pot_pos, is_on);
    }
}
