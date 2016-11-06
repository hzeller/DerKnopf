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

#if 1
// An encoder that creates one quad step per detent
int8_t QuadDecoder::UpdateEnoderState(uint8_t encoder_bits) {
  // assert((encoder_bits & STATE_11) == encoder_bits);
  // self transitions, skipping states or transition from init always dir=0
  int8_t direction = 0;
  switch (current_state_) {  // essentially difference of gray encoding.
  case STATE_00:
    if (encoder_bits == STATE_01) direction = +1;       // ^ -> 01
    else if (encoder_bits == STATE_10) direction = -1;  // ^ -> 10
    break;
  case STATE_01:
    if (encoder_bits == STATE_11) direction = +1;       // ^ -> 10
    else if (encoder_bits == STATE_00) direction = -1;  // ^ -> 01
    break;
  case STATE_11:
    if (encoder_bits == STATE_10) direction = +1;       // ^ -> 01
    else if (encoder_bits == STATE_01) direction = -1;  // ^ -> 10
    break;
  case STATE_10:
    if (encoder_bits == STATE_00) direction = +1;       // ^ -> 10
    else if (encoder_bits == STATE_11) direction = -1;  // ^ -> 01
    break;
  }
  current_state_ = encoder_bits;
  return direction;
}
#else
// An encoder that does goes through one full cycle
int8_t QuadDecoder::UpdateEnoderState(uint8_t encoder_bits) {
  // assert((encoder_bits & STATE_11) == encoder_bits);
  // self transitions, skipping states or transition from init always dir=0
  int8_t direction = 0;
  switch (current_state_) {  // essentially difference of gray encoding.
  case STATE_01:
    if (encoder_bits == STATE_11) direction = +1;       // ^ -> 10
    break;
  case STATE_10:
    if (encoder_bits == STATE_11) direction = -1;  // ^ -> 01
    break;
  }
  current_state_ = encoder_bits;
  return direction;
}
#endif
