#include "meters_ce318.h"

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

enum{CE318_ERROR_THRESHOLD = 3};
#define DFF_FLAG (0x80)
#define DFF_FIELD_MAX_SIZE (sizeof(int64_t) + 1)

#define SMP_END (0xC0)
#define SMP_PROTOCOL_ID (6)

#define SMP_COMMAND_DATA (6)
#define SMP_COMMAND_ERROR (7)
 
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

static uint16_t ce318_get_crc16(uint16_t crc, const uint8_t * buffer, uint32_t lenght)
{
    static const uint16_t poly = 0x8005;
    uint32_t i;
    const uint8_t * pcBlock = buffer;
    
    while(lenght--){
        crc ^= *pcBlock++ << 8;

        for(i = 0; i < 8; i++)
            crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
    }

    return crc;
}

static int32_t ce318_dff_parce(const uint8_t * buffer, uint32_t remaining,
                                int64_t *field, int32_t signed_field)
{
    if(buffer == NULL)
        return -1;
    
    if(field == NULL)
        return -2;
    
    const uint8_t *ch = buffer;
    int32_t field_size = 0;
    uint64_t result = 0;
    uint8_t minus = false;

    if(signed_field){
        minus = (*ch & 0x40) != 0;
    }

    if(remaining == 0)
        return -4;

    do{
        if(field_size > DFF_FIELD_MAX_SIZE)
            return -3;
        
        result |= (*ch & 0x7F) << (field_size * 7);
        field_size++;
        
    }while((*ch++ & DFF_FLAG) != 0);

    if(minus){
       result |= 0xffffffffffffffff << (field_size * 7);
    }

    *field = (int64_t)result;

    return field_size;
}

int32_t meters_ce318_send_packet(meters_context_t * context, 
                                uint32_t baudrate, uint32_t address, 
                                const uint8_t * data, uint32_t length)
{
    if((data == NULL) || (length == 0))
        return -1;
    
    int32_t ret;
    uint8_t pack[64] = {SMP_END};
    uint8_t count = 1;
    uint8_t header_buf[] = {SMP_PROTOCOL_ID, 0, 0, 0, 0, 0, SMP_COMMAND_DATA};
    
    for(uint32_t i = 0; i < 4; i++)
        *(header_buf + i + 1) = (uint8_t)(address >> (i * 8));

    uint16_t crc = 0;

    crc = ce318_get_crc16(crc, header_buf, sizeof(header_buf));
    crc = ce318_get_crc16(crc, data, length);
    
    uint8_t crc_buf[] = {(crc >> 8) & 0xff, crc & 0xff};

    count += ce318_set_escape(header_buf, &pack[count], sizeof(header_buf));
    count += ce318_set_escape(data, &pack[count], length);
    count += ce318_set_escape(crc_buf, &pack[count], 2);

    pack[count] = SMP_END;
    count++;

    bus485_lock(context->bus485);
    ret = bus485_set_baudrate(context->bus485, baudrate);
    bus485_flush(context->bus485);
    ret = bus485_send(context->bus485, pack, count);
    if(ret < 0){
        bus485_release(context->bus485);
    }
    return ret;
}

int32_t meters_ce318_get_response(meters_context_t * context, uint8_t * data)
{

}

int32_t meters_ce318_read(meters_context_t * context, uint32_t item_idx)
{

}


int32_t meters_ce318_init(meters_context_t * context, uint32_t item_idx)
{
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    meters_item_t * item = &context->items[item_idx];

    item->bad_responce_count = CE318_ERROR_THRESHOLD;
}