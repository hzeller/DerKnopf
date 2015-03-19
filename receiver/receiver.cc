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

#define IR_PORT PINC
#define IR_IN (1<<5)

#define MOT_PORT PORTD
#define MOT_DIRECTION DDRD
#define MOT_A0 (1<<4)
#define MOT_A1 (1<<5)
#define MOT_B0 (1<<6)
#define MOT_B1 (1<<7)

uint8_t motor_cycle[] = {
    (MOT_A0 | MOT_B0),
    (MOT_A0 | MOT_B1),
    (MOT_A1 | MOT_B1),
    (MOT_A1 | MOT_B0),
};

static inline bool infrared_in() { return (IR_PORT & IR_IN) != 0; }

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

volatile bool reverse;
volatile uint8_t remaining_steps;
volatile uint8_t global_steps;

inline bool motor_busy() { return remaining_steps != 0; }
void InitMotor() {
    TCCR2= ((1<<CS22)|(1<<CS21)|(1<<CS20) // prescaling 1024 p.116
            | (1<<WGM21)   // OCR2 compare. p.115
            );
    remaining_steps = 0;
    global_steps = 0;
    MOT_DIRECTION = MOT_A0|MOT_A1|MOT_B0|MOT_B1;
    sei();
}

// Turn motor the number of steps. Don't call when remaining_steps != 0
void TurnMotor(int8_t steps) {
    if (steps < 0) {
        remaining_steps = -4 * steps;
        reverse = true;
    } else {
        remaining_steps = 4 * steps;
        reverse = false;
    }
    OCR2 = (F_CPU / 1024) / 200;
    TCNT2 = 0;
    TIMSK |= (1<<OCIE2);  // Go
}

ISR(TIMER2_COMP_vect) {
    if (remaining_steps == 0) {
        TIMSK &= ~(1<<OCIE2);       // Disable interrupt. We are done.
        MOT_PORT = 0;
        return;
    }
    if (reverse) {
        --global_steps;
    } else {
        ++global_steps;
    }
    MOT_PORT = motor_cycle[global_steps % 4];
    --remaining_steps;
}

int main() {
    InitMotor();
    uint8_t buffer[4];

    int8_t motor_steps = 0;
    for (;;) {
        if (!infrared_in() && read_infrared(buffer) == 4) {
            if (buffer[0] == 'm' && buffer[1] == 'o' && buffer[2] == 'r' && buffer[3] == 'e') {
                ++motor_steps;
            }
            else if (buffer[0] == 'l' && buffer[1] == 'e' && buffer[2] == 's' && buffer[3] == 's') {
                --motor_steps;
            }
        }

        // Turn motor, but only if it is not still busy. While busy, we just collect new steps
        if (motor_steps != 0 && !motor_busy()) {
            TurnMotor(3 * motor_steps);
            motor_steps = 0;
        }
    }
}
