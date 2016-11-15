/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Copyright (c) h.zeller@acm.org. GNU public License.
 *
 * Transmitter for 'DerKnopf'
 *   - react on interrupt, waking up when reading the encoder
 *   - send stuff via infrared, 38kHz encoded.
 *   - Infrared receivers like a particular burst width, so we encode the bits
 *     in the spacing.
 *     (this gives a variable length encoding, but we don't care).
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

#include "quad.h"

#define DO_TIMER_COMPARE 0

// Do direct pullup for the quad encoder. However, these are relatively low
// values in the AVR, so if this is set to 0, then we use higher value 1MOhm
// pullups on the board.
#define ROT_DIRECT_PULLUP 1

// we want to sleep after transmit. Does not work yet, as we need to connect
// INT0
#define SLEEP_AFTER_TRANSMIT 0

// We need to fudge up the frequency a bit because the RC oscillator runs
// a little slower at 3V.
#define IR_FREQ         (38000 * 1.0387)  // Frequency of IR carrier

// Need double transmit frequency for one cycle
#define CLOCK_COUNTER   F_CPU / (2.0 * IR_FREQ)

#define IR_OUT_PORT      PORTA
#define IR_OUT_DATADIR   DDRA
#define IR_OUT_BIT       (1<<0)
#define IR_DEBUG_BIT     (1<<1)    // Nice to trigger the scope on.
#define IR_BURST_LEN     (2 * 22)  // 22 cycles. two edges.
#define IR_INITIAL_BURST (4 * IR_BURST_LEN)
#define IR_BIT_0_PAUSE    28
#define IR_BIT_1_PAUSE   105
#define IR_FINAL_PAUSE   255

#define ROT_PORT_OUT PORTA
#define ROT_PORT_IN  PINA
#define ROT_A        (1<<7)   // Also PCINT
#define ROT_B        (1<<3)   // Also PCINT
#define ROT_PULLUP_A (1<<5)
#define ROT_PULLUP_B (1<<6)
#define ROT_INTR1     PCINT7
#define ROT_INTR2     PCINT7

#define BUT_PORT_IN   PINB
#define BUT_PORT_OUT  PORTB
#define BUT_BIT       (1<<0)   // Also PCINT to wakeup
#define BUT_INTR      PCINT8

// The commands we send are just a 32 bit values. Make that some text :)
#define MK_COMMAND(a, b, c, d) \
        ((uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | d)
#define COMMAND_MORE MK_COMMAND('m', 'o', 'r', 'e')  // Knob turned right
#define COMMAND_LESS MK_COMMAND('l', 'e', 's', 's')  // Knob turned left
#define COMMAND_B_ON MK_COMMAND('b', '_', 'o', 'n')  // Button pressed
#define COMMAND_BOFF MK_COMMAND('b', 'o', 'f', 'f')  // Button released
#define COMMAND_BHLD MK_COMMAND('b', 'h', 'l', 'd')  // Button kept pressing

// Sending. All happens in an interrupt set up to fire in 2*38kHz
enum SendState {
    BIT_BURST,   // the burst at the beginning of a bit (longer initially)
    BIT_PAUSE,   // the pause, whose length the bit encodes.
    FINAL_PAUSE, // time between data segments
    SENDER_IDLE,
};
// State used in the ISR. To save time and space, we assign these to global
// registers.
register enum SendState send_state asm("r13");
register uint8_t countdown asm("r12");

static uint32_t data_to_send;
static uint32_t current_bit;

// Send a 32Bit value.
// "value" is the 32 bit value to send, typically just letters for easier
// debugging :)
void Send(uint32_t value) {
    data_to_send = value;
    current_bit = 0x80000000;
    send_state = BIT_BURST;
    countdown = IR_INITIAL_BURST;
    OCR0A = CLOCK_COUNTER;
    IR_OUT_PORT |= IR_DEBUG_BIT;
    TCNT0 = 0;
#if DO_TIMER_COMPARE
    TCCR0A = ((1<<WGM01)   // OCRA compare. p.83
              | (1<<COM0A0)); // Toggle OC0A on compare match
#else
    TCCR0A = (1<<WGM01);   // OCRA compare. p.83
#endif
    TIMSK0 |= (1<<OCIE0A);  // Go
}

void advanceStateBottomHalf();

// Check if sending is done. Needs to be called regularly after a Send()
// has been issued.
bool PollIsSendingDone() {
    if (send_state == SENDER_IDLE)
        return true;
    if (countdown == 0)
        advanceStateBottomHalf();
    return false;
}

void advanceStateBottomHalf() {
    // Note, we need to set the state _before_ setting the countdown.
    // The interrupt is still running and polling send_state - so this results
    // in race-conditions.
    if (send_state == FINAL_PAUSE) {
        TIMSK0 &= ~(1<<OCIE0A);       // Disable interrupt. We are done.
        IR_OUT_PORT &= ~(IR_DEBUG_BIT|IR_OUT_BIT);
        send_state = SENDER_IDLE;  // External observers might be interested.
    }
    else if (send_state == BIT_BURST) {  // Just sent burst, now encode data
        if (current_bit == 0) {
            send_state = FINAL_PAUSE;
            IR_OUT_PORT &= ~(IR_OUT_BIT|IR_DEBUG_BIT);
            countdown = IR_FINAL_PAUSE;   // Ran out of data. Final pause.
        } else {
            send_state = BIT_PAUSE;
            IR_OUT_PORT &= ~(IR_OUT_BIT);
            // Data is encoded in the pause between the bursts.
            countdown = (current_bit & data_to_send)
                ? IR_BIT_1_PAUSE
                : IR_BIT_0_PAUSE;
        }
    }
    else {
        send_state = BIT_BURST;
        countdown = IR_BURST_LEN;
        current_bit >>= 1;
#if DO_TIMER_COMPARE
        TCCR0A = ((1<<WGM01)   // OCRA compare. p.83
                  | (1<<COM0A0)); // Toggle OC0A on compare match
#endif

    }
}

ISR(TIM0_COMPA_vect) {
#if DO_TIMER_COMPARE
    if (countdown == 0) {
        TCCR0A = (1<<WGM01);   // OCRA compare. p.83
        IR_OUT_PORT &= ~(IR_OUT_BIT);
        return;
    }
#else
    if (countdown == 0)
        return;
    if (send_state == BIT_BURST) {
        IR_OUT_PORT ^= IR_OUT_BIT;
    } // else we're in a pause-phase.
#endif
    --countdown;
}

// Pin change interrupt. Dummy in the interrupt vector to wake up.
EMPTY_INTERRUPT(PCINT0_vect);

static bool is_button_pressed() { return (BUT_PORT_IN & BUT_BIT) == 0; }
static inline uint8_t rot_status() {
    const uint8_t rot_in = ROT_PORT_IN;
    return ((rot_in & ROT_B) ? 10 : 00) | ((rot_in & ROT_A) ? 01 : 00);
}

int main() {
    clock_prescale_set(clock_div_2);   // Default speed: 4Mhz
    send_state = SENDER_IDLE;
    countdown = 0;

    IR_OUT_DATADIR = (IR_OUT_BIT | IR_DEBUG_BIT);
    PORTB |= (1<<3);   // Add pullup to reset

    BUT_PORT_OUT |= BUT_BIT;          // Pullup.
#if ROT_DIRECT_PULLUP
    ROT_PORT_OUT |= (ROT_A | ROT_B);  // Direct pullup until we have reistors
#endif
    ROT_PORT_OUT |= (ROT_PULLUP_A | ROT_PULLUP_B);   // extra pullups.

    // The pins we are interested in when PCIE0 is on. Page 49.
    //PCMSK0 =(1<<ROT_INTR1)|(1<<ROT_INTR2)|(1<<BUT_INTR);

    TCCR0B = (1<<CS00);     // timer 0: no prescaling p.84

    PRR = (1<<PRADC);  // Don't need ADC. Power down.

    sei();

    QuadDecoder rotary;
    int rot_pos = 0;
    bool last_button_status = false;

    for (;;) {
        // We accumulate the state here, so that we can send it possibly slower
        // than they are generated.
        rot_pos += rotary.UpdateEnoderState(rot_status());
        const bool new_button_status = is_button_pressed();
        // If sender status is free, send our status.
        if (PollIsSendingDone()) {
            if (rot_pos > 0) {
                Send(COMMAND_MORE);
                rot_pos = 0;
            }
            else if (rot_pos < 0) {
                Send(COMMAND_LESS);
                rot_pos = 0;
            }
            else {
                if (!last_button_status && new_button_status) Send(COMMAND_B_ON);
                if (last_button_status && !new_button_status) Send(COMMAND_BOFF);
                //if (last_button_status && new_button_status)  Send(COMMAND_BHLD);
                last_button_status = new_button_status;
            }
        }

#if SLEEP_AFTER_TRANSMIT
        if (PollIsSendingDone()) {
            cli();
            GIMSK |= (1<<PCIE0);          // Interrupt on change anywhere 0:7
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);

            sleep_enable();
            sei();
            sleep_cpu();

            // Zzzz...

            // Waking up due to interrupt.
            sleep_disable();
            GIMSK = 0;
        }
#endif
    }
}
