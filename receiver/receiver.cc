/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Copyright (c) h.zeller@acm.org. GNU public License.
 *
 * Receiver for 'DerKnopf'
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "serial-com.h"

#define IN_IR (1<<5)

static void printCRLF(SerialCom *out) {
    out->write('\r');
    out->write('\n');
}

static inline bool infrared_in() { return (PINC & IN_IR) != 0; }

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

int main() {
    SerialCom comm;
    uint8_t buffer[4];

    for (;;) {
        if (!infrared_in() && read_infrared(buffer) == 4) {
            for (uint8_t i = 0; i < 4; ++i) {
                comm.write(buffer[i]);
            }
            printCRLF(&comm);
        }
    }
}
