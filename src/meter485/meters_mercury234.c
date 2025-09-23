#include "meters_private.h"
#include "bus485.h"
#include <zephyr/sys/crc.h>

enum{MERCURY_ERROR_THRESHOLD = 3};
enum{MERCURY_REQUEST_PAUSE = 30};

static int32_t meters_mercury_send(meters_context_t *context, uint8_t address, 
                            const uint8_t *data, size_t length)
{
    int32_t ret;
    uint8_t query[24] = {address};
    size_t count = length + 1;

    memcpy(&query[1], data, length);

    uint16_t crc = crc16_reflect(0xA001, 0xFFFF, query, count);

    query[count] = crc & 0xff;
    query[count + 1] = (crc >> 8) & 0xff;

    bus485_lock(context->bus485);
    
    ret = bus485_set_baudrate(context->bus485, baudrate);
    if(ret < 0){
        bus485_release(context->bus485);
        return ret;
    }

    bus485_flush(context->bus485);

    ret = bus485_send(context->bus485, query, count);
    if(ret < 0){
        bus485_release(context->bus485);
        return ret;
    }

    return 0;
}

static int32_t meters_mercury_receive(meters_context_t *context, uint8_t address, uint8_t *data, uint32_t length){
    int32_t ret;
    uint8_t resp[32];

    ret = bus485_recv(context->bus485, resp, sizeof(resp), CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
    if(ret < 0){
        bus485_release(context->bus485);
        return ret;
    }
    bus485_release(context->bus485);

    uint16_t crc = crc16_reflect(0xA001, 0xFFFF, resp, ret - sizeof(uint16_t));
    uint16_t crc1 = (((uint16_t)resp[ret-1]) << 8) | resp[ret-2];

    if(crc != crc1)
        return -EBADMSG;
    
    if(resp[0] != 0 || resp[0] != address)
        return -EXDEV;
    
    memcpy(data, &resp[1], ret - 3);

    return ret - 3;
}

static int32_t meters_mercury_request(meters_context_t *context, uint8_t address, 
                                    const uint8_t *req_data, uint32_t req_length
                                    uint8_t *rcv_buffer, uint32_t rcv_length )
{
    int32_t ret;
    ret = meters_mercury_send(context, address, req_data, req_length);
    if(ret < 0)
        return ret;
    
    ret = meters_mercury_receive(context, address, rcv_buffer, rcv_length);
    if(ret < 0){
        if(ret != -ETIMEDOUT){
            LOG_WRN("mercury %d request error: %d", address, ret);
            LOG_HEXDUMP_WRN(req_data, req_length, "request");
        }
        return ret;
    }
    
    return ret;

}



int32_t meters_mercury_init(meters_context_t * context, uint32_t item_idx){
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    meters_item_t * item = &context->items[item_idx];

    item->bad_responce_count = MERCURY_ERROR_THRESHOLD;
}