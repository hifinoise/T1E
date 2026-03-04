#pragma once
#include <stdint.h>
#include "state.h"

void mode_clock_render(uint8_t *fb, t1e_state_t *s);
void mode_clock_action(t1e_state_t *s);
