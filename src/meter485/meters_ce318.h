#pragma once

#include "meters_private.h"

int32_t meters_ce318_read(meters_context_t * context, uint32_t item_idx);
int32_t meters_ce318_init(meters_context_t * context, uint32_t item_idx);

int32_t meters_ce318_get_energy_active(meters_context_t * context, uint32_t baudrate,
                                uint32_t address, uint64_t * energy);

int32_t meters_ce318_get_voltage(meters_context_t *context, uint32_t baudrate, 
                                uint32_t address, float voltage[3]);

int32_t meters_ce318_get_battery(meters_context_t *context, uint32_t baudrate,
                                uint32_t address, uint8_t *hex);                                