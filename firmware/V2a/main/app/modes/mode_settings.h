#pragma once
#include "state.h"
#include <stdint.h>
void mode_settings_render(uint8_t *fb, t1e_state_t *s);
void mode_settings_action(t1e_state_t *s);
void mode_settings_confirm(t1e_state_t *s);
