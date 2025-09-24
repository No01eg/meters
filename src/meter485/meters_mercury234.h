#pragma once

#include "meters_private.h"

int32_t meters_mercury_read(meters_context_t * context, uint32_t item_idx);
int32_t meters_mercury_init(meters_context_t * context, uint32_t item_idx);

int32_t meters_mercury_ping(meters_context_t *context, uint8_t address, uint32_t baudrate);

