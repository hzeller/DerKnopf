/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#ifndef QUAD_DECOER_H_
#define QUAD_DECOER_H_

#include <stdint.h>

class QuadDecoder {
public:
  QuadDecoder();
  
  // Given the current external state of the quadratur encoder (two bits),
  // returns if it increased or decreased (+1, -1 or 0).
  //
  // Jumping states (too fast turning or some other illegal transitions) or
  // no transitions will return 0.
  //
  // Note, that switch bouncing will not result in runnaway increase or decrease
  // sequence but emits self-zeoring +1/-1.
  int UpdateEnoderState(int value);

private:
  int current_state_;
};

#endif  // QUAD_DECOER_H_
