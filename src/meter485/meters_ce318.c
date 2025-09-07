#include "meters_ce318.h"

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

enum{CE318_ERROR_THRESHOLD = 3};

static int32_t ce318_set_escape(const uint8_t * src, uint8_t * dest, int32_t count)
{
    int32_t size = 0;
    
    while(count > 0){
        if(*src == 0xC0){
            *dest++ = 0xDB;
            *dest++ = 0xDC;
            size++;
        }
        else if(*src == 0xDB){
            *dest++ = 0xDB;
            *dest++ = 0xDD;
            size++;
        }
        else 
            *dest++ = *src;
        src++;
        count--;
        size++;
    }
    return size;
}

static int32_t ce318_remove_escape(const uint8_t *src, uint8_t * dest, int32_t count)
{
    int32_t size = 0;

    while(count > 0){
        if(*src == 0xDB){
            *dest = (*(src + 1) == 0xDC) ? 0xC0 : 0xDB;
            src += 2;
            size++;
            dest++;
            count -= 2;
        }
        else{
            *dest++ = *src++;
            size++;
            count--;
        }
    }
    return size;
}

static uint16_t ce318_get_crc16(const uint8_t * buffer, uint32_t lenght)
{
    static const uint16_t poly = 0x8005;
    uint32_t i;
    uint16_t crc = 0;
    const uint8_t * pcBlock = buffer;
    
    while(lenght--){
        crc ^= *pcBlock++ << 8;

        for(i = 0; i < 8; i++)
            crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
    }

    return crc;
}


int32_t meters_ce318_init(meters_context_t * context, uint32_t item_idx)
{
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    meters_item_t * item = &context->items[item_idx];

    item->bad_responce_count = CE318_ERROR_THRESHOLD;
}