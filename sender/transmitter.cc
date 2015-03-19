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
#include <util/delay.h>

#include "quad.h"

#define DO_TIMER_COMPARE 0
#define ALLOW_BUTTON 0

#define IR_FREQ        38000  // Frequency of IR carrier
#define IR_OUT_PORT    PORTB
#define IR_OUT_DATADIR DDRB
#define IR_OUT_BIT     (1<<2)
#define IR_DEBUG_BIT   (1<<1)  // Nice to trigger the scope on.
#define IR_BURST_LEN   32
#define IR_BIT_0       32
#define IR_BIT_1       64
#define IR_FINAL_PAUSE 255

#define ROT_PORT_OUT PORTA
#define ROT_PORT_IN  PINA
#define ROT_BUTTON   (1<<2)   // Also INT0 to wakeup
#define ROT_A        (1<<0)   // Also INT1 to wakeup
#define ROT_B        (1<<1)

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
    countdown = 2 * IR_BURST_LEN;  // We make the first burst double the length.
    OCR0A = F_CPU / (2*IR_FREQ);  // Need double transmit frequency for one cycle
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
            countdown = (current_bit & data_to_send) ? IR_BIT_1 : IR_BIT_0;
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
    if (!countdown) {
        TCCR0A = (1<<WGM01);   // OCRA compare. p.83
        IR_OUT_PORT &= ~(IR_OUT_BIT);
        return;
    }
#else
    if (!countdown) return;
    if (send_state == BIT_BURST) {
        IR_OUT_PORT ^= IR_OUT_BIT;
    }
#endif
    --countdown;
}

EMPTY_INTERRUPT(PCINT0_vect);   // Wake up.

#if ALLOW_BUTTON
static bool is_button_pressed() { return (ROT_PORT_IN & ROT_BUTTON) == 0; }
#endif
static inline uint8_t rot_status() {
    return ((ROT_PORT_IN & ROT_B) ? 10 : 00) | ((ROT_PORT_IN & ROT_A) ? 1 : 0);
}

int main() {
    clock_prescale_set(clock_div_2);   // Default speed: 4Mhz
    send_state = SENDER_IDLE;
    
    IR_OUT_DATADIR |= (IR_OUT_BIT | IR_DEBUG_BIT);
    PCMSK0 =(1<<PCINT1)|(1<<PCINT0);  // The pins we are interested in when PCIE0 is on.
    
    TCCR0B = (1<<CS00);     // timer 0: no prescaling p.84
    
    ROT_PORT_OUT |= (ROT_BUTTON|ROT_B|ROT_A);  // Switch on pullups.
    
    PRR = (1<<PRADC);  // Don't need ADC. Power down.
    
    sei();
    
    QuadDecoder rotary;
    int rot_pos = 0;
#if ALLOW_BUTTON
    bool last_button_status = false;
#endif
    for (;;) {
        // We accumulate the state here, so that we can send it possibly slower
        // than they are generated.
        rot_pos += rotary.UpdateEnoderState(rot_status());
#if ALLOW_BUTTON
        const bool new_button_status = is_button_pressed();
#endif
        // If sender status is free, send our status.
        if (PollIsSendingDone()) {
            if (rot_pos > 0) {
                Send(COMMAND_MORE);
                --rot_pos;
            }
            else if (rot_pos < 0) {
                Send(COMMAND_LESS);
                ++rot_pos;
            }
#if ALLOW_BUTTON
            else {
                if (!last_button_status && new_button_status) Send(COMMAND_B_ON);
                if (last_button_status && !new_button_status) Send(COMMAND_BOFF);
                if (last_button_status && new_button_status)  Send(COMMAND_BHLD);
                last_button_status = new_button_status;
            }
#endif
        }

        if (PollIsSendingDone()) {
            cli();
            GIMSK |= (1<<PCIE0);              // Interrupt on change anywhere 0:7
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_enable();
                
            sei();
            sleep_cpu();
            sleep_disable();
            GIMSK = 0;
        }
    }
}
