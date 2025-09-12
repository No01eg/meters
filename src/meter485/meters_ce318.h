#pragma once

#include "meters_private.h"

typedef struct {
    uint32_t baudrate;
    uint32_t address;
    uint8_t *query;
    uint32_t query_length;
    uint32_t is_signed_values;
}ce318_poll_data_t;

int32_t meters_ce318_read(meters_context_t * context, uint32_t item_idx);
int32_t meters_ce318_init(meters_context_t * context, uint32_t item_idx);

/*int32_t meters_ce318_send_packet(meters_context_t * context, 
                                uint32_t baudrate, uint32_t address, 
                                const uint8_t * data, uint32_t length);*/