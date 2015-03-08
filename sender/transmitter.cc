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
#include <util/delay.h>
#include <avr/sleep.h>

#include "quad.h"

#define IR_OUT_PORT    PORTB
#define IR_OUT_DATADIR DDRB
#define IR_OUT_BIT     (1<<0)
#define IR_BURST_LEN   32
#define IR_BIT_0       32
#define IR_BIT_1       64
#define IR_FINAL_PAUSE 30 * IR_BIT_1
#define IR_DEBUG_BIT   (1<<1)  // Nice to trigger the scope on.

#define ROT_PORT_OUT PORTD
#define ROT_PORT_IN  PIND
#define ROT_BUTTON   (1<<2)   // Also INT0 to wakeup
#define ROT_A        (1<<3)   // Also INT1 to wakeup
#define ROT_B        (1<<4)

// Sending. All happens in an interrupt set up to fire in 2*38kHz
enum SendState {
    BIT_BURST,   // the burst at the beginning of a bit (longer initially)
    BIT_PAUSE,   // the pause, whose length the bit encodes.
    FINAL_PAUSE, // time between data segments
    SENDER_IDLE,
};
static volatile enum SendState send_state = SENDER_IDLE;

static volatile uint32_t data_to_send;
static volatile uint32_t current_bit;
static volatile uint16_t countdown;

// Send a 32Bit value.
// - Start with a long burst cycle burst on.
// - 32 times { bit[i] ? pause32 : pause16 ; burst16 }
void Send(uint32_t value) {
    data_to_send = value;
    current_bit = 0x80000000;
    send_state = BIT_BURST;
    countdown = 2 * IR_BURST_LEN;  // We make the first burst double the length.
    OCR2 = F_CPU / (2*38000);      // Need double transmit frequency for one cylcle.
    TCNT2 = 0;
    IR_OUT_PORT |= IR_DEBUG_BIT;
    TIMSK |= (1<<OCIE2);  // Go
}

// Send a 4 letter string.
void SendArray(const char str[4]) {
    uint32_t value = 0;
    value |= (uint32_t)str[0] << 24;
    value |= (uint32_t)str[1] << 16;
    value |= (uint32_t)str[2] << 8;
    value |= (uint32_t)str[3];
    Send(value);
}

ISR(TIMER2_COMP_vect) {
    if (countdown != 0) {
        if (send_state == BIT_BURST) {
            IR_OUT_PORT ^= IR_OUT_BIT;
        }
        --countdown;
        return;
    }

    if (send_state == FINAL_PAUSE) {
        TIMSK &= ~(1<<OCIE2);       // Disable interrupt. We are done.
        IR_OUT_PORT &= ~(IR_DEBUG_BIT|IR_OUT_BIT);
        send_state = SENDER_IDLE;  // External observers might be interested.
    }
    else if (send_state == BIT_BURST) {  // We just sent a burst, now encode some data
        if (current_bit == 0) {
            countdown = IR_FINAL_PAUSE;   // We ran out of data. Send final pause between packets.
            IR_OUT_PORT &= ~(IR_DEBUG_BIT);
            send_state = FINAL_PAUSE;
        } else {
            countdown = (current_bit & data_to_send) ? IR_BIT_1 : IR_BIT_0;
            IR_OUT_PORT &= ~(IR_OUT_BIT);
            send_state = BIT_PAUSE;
        }
    }
    else {
        countdown = IR_BURST_LEN;
        send_state = BIT_BURST;
        current_bit >>= 1;
    }
}

static bool is_button_pressed() { return (ROT_PORT_IN & ROT_BUTTON) == 0; }
static uint8_t rot_status() {
    return ((ROT_PORT_IN & ROT_B) ? 10 : 00) | ((ROT_PORT_IN & ROT_A) ? 1 : 0);
}

int main() {
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
        if (send_state == SENDER_IDLE) {
            if (rot_pos > 0) {
                SendArray("more");
                --rot_pos;
            }
            else if (rot_pos < 0) {
                SendArray("less");
                ++rot_pos;
            }
            else {
                if (!last_button_sent_status && new_button_status) SendArray("b_on");
                if (last_button_sent_status && !new_button_status) SendArray("boff");
                if (last_button_sent_status && new_button_status)  SendArray("bhld");
                last_button_sent_status = new_button_status;
            }
        }
#if 0
        // Nothing to send ? Go back to sleep. Does not work yet properly.
        if (send_state == SENDER_IDLE) {
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
