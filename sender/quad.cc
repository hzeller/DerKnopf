/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
#include "quad.h"

enum {
  STATE_00   = 0b00,
  STATE_01   = 0b01,
  STATE_10   = 0b10,
  STATE_11   = 0b11,
  INIT_STATE = 4
};

QuadDecoder::QuadDecoder() : current_state_(INIT_STATE) {}

// Given the current external state of the quadratur encoder (two bits),
// returns if it increased or decreased (+1, -1 or 0).
// Jumping states (too fast turning or some other illegal transitions) or
// no transitions will return 0.
// Note, that switch bouncing will not result in runnaway increase or decrease
// sequence but emits self-zeoring +1/-1.
int QuadDecoder::UpdateEnoderState(int value) {
  // assert((value & STATE_11) == value);
  // self transitions, skipping states or transition from init always dir=0
  int direction = 0;
  switch (current_state_) {  // essentially difference of gray encoding.
  case STATE_00:
    if (value == STATE_01) direction = +1;       // ^ -> 01
    else if (value == STATE_10) direction = -1;  // ^ -> 10
    break;
  case STATE_01:
    if (value == STATE_11) direction = +1;       // ^ -> 10
    else if (value == STATE_00) direction = -1;  // ^ -> 01
    break;
  case STATE_11:
    if (value == STATE_10) direction = +1;       // ^ -> 01
    else if (value == STATE_01) direction = -1;  // ^ -> 10
    break;
  case STATE_10:
    if (value == STATE_00) direction = +1;       // ^ -> 10
    else if (value == STATE_11) direction = -1;  // ^ -> 01
    break;
  }
  current_state_ = value;
  return direction;
}
