/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * Copyright (c) h.zeller@acm.org. GNU public License.
 *
 * Transmitter for 'DerKnopf'
 *   - react on interrupt, waking up when reading the encoder
 *   - send stuff via infrared, 38kHz encoded.
 *   - Infrared receivers like a particular burst width, so we encode the bits in the spacing.
 *     (this gives a variable length encoding, but we don't care).
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "quad.h"

#define IR_OUT_PORT    PORTB
#define IR_OUT_DATADIR DDRB
#define IR_OUT_BIT     (1<<0)
#define IR_BURST_LEN   32
#define IR_BIT_0       32
#define IR_BIT_1       64
#define IR_FINAL_PAUSE 255
#define IR_DEBUG_BIT   (1<<1)  // Nice to trigger the scope on.

#define ROT_PORT_OUT PORTD
#define ROT_PORT_IN  PIND
#define ROT_BUTTON   (1<<2)   // Also INT0 to wakeup
#define ROT_A        (1<<3)   // Also INT1 to wakeup
#define ROT_B        (1<<4)

// The commands we send are just a 32 bit values. For easier debugging, make that some text :)
#define MK_COMMAND(a, b, c, d) ((uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | d)
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
// State used in the ISR. To save time and space, we assign these to global registers.
register enum SendState send_state asm("r13");
register uint8_t countdown asm("r12");

static uint32_t data_to_send;
static uint32_t current_bit;

// Send a 32Bit value.
// "value" is the 32 bit value to send, typically just letters for easier debugging :)
// If "alternate_channel" is set, the first letter is made uppdercase
// letter is
void Send(uint32_t value) {
    data_to_send = value;
    current_bit = 0x80000000;
    send_state = BIT_BURST;
    countdown = 2 * IR_BURST_LEN;  // We make the first burst double the length.
    OCR2 = F_CPU / (2*38000);      // Need double transmit frequency for one cylcle.
    IR_OUT_PORT |= IR_DEBUG_BIT;
    TCNT2 = 0;
    TIMSK |= (1<<OCIE2);  // Go
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
    // Note, we need to set the state _before_ setting the countdown. The interrupt is still
    // running and polling send_state - so this results in race-conditions.
    if (send_state == FINAL_PAUSE) {
        TIMSK &= ~(1<<OCIE2);       // Disable interrupt. We are done.
        IR_OUT_PORT &= ~(IR_DEBUG_BIT|IR_OUT_BIT);
        send_state = SENDER_IDLE;  // External observers might be interested.
    }
    else if (send_state == BIT_BURST) {  // We just sent a burst, now encode some data
        if (current_bit == 0) {
            send_state = FINAL_PAUSE;
            IR_OUT_PORT &= ~(IR_OUT_BIT|IR_DEBUG_BIT);  // Switch off debug scope trigger.
            countdown = IR_FINAL_PAUSE;   // We ran out of data. Send final pause between packets.
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
    }
}

ISR(TIMER2_COMP_vect) {
    if (!countdown) return;
    if (send_state == BIT_BURST) {
        IR_OUT_PORT ^= IR_OUT_BIT;
    }
    --countdown;
}

static bool is_button_pressed() { return (ROT_PORT_IN & ROT_BUTTON) == 0; }
static uint8_t rot_status() {
    return ((ROT_PORT_IN & ROT_B) ? 10 : 00) | ((ROT_PORT_IN & ROT_A) ? 1 : 0);
}

int main() {
    send_state = SENDER_IDLE;
    IR_OUT_DATADIR |= IR_OUT_BIT|IR_DEBUG_BIT;
    TCCR2= ((1<<CS20)      // no prescaling p.116
            | (1<<WGM21)   // OCR2 compare. p.115
            );
    ROT_PORT_OUT |= (ROT_BUTTON|ROT_B|ROT_A);  // Switch on pullups.
    sei();

    QuadDecoder rotary;
    int rot_pos = 0;
    bool last_button_sent_status = false;
    for (;;) {
        // We accumulate the state here, so that we can send it possibly slower than
        // they are generated.
        rot_pos += rotary.UpdateEnoderState(rot_status());
        const bool new_button_status = is_button_pressed();
    
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
            else {
                if (!last_button_sent_status && new_button_status) Send(COMMAND_B_ON);
                if (last_button_sent_status && !new_button_status) Send(COMMAND_BOFF);
                if (last_button_sent_status && new_button_status)  Send(COMMAND_BHLD);
                last_button_sent_status = new_button_status;
            }
        }
#if 0
        // Nothing to send ? Go back to sleep. Does not work yet properly.
        if (PollIsSendingDone()) {
            GICR |= (1<<INT1)|(1<<INT0);
            MCUCR = ((1<<SE)|    // Sleep enable
                     (1<<SM1)    // Power down
                     // 0 on ISCxx -> low level interrupt request.
                     );
            sleep_mode();
            sleep_disable();
            GICR &= ~(1<<INT1)|(1<<INT0);
        }
#endif
    }
}
