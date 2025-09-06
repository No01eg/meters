#pragma once

#include "meters_private.h"

int32_t meters_spm90_read(meters_context_t * context, uint32_t item_idx);
int32_t meters_spm90_init(meters_context_t * context, uint32_t item_idx);

int32_t meters_spm90_get_values(meters_context_t * context, uint16_t id, 
                                uint16_t baudrate, meters_values_dc_t *shadow);