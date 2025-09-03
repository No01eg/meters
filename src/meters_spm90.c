#include "meters_private.h"
#include "protocol_utils.h"
#include "bus485.h"
#include <zephyr/sys/crc.h>

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

enum {SPM90_ERROR_THRESHOLD = 3};

static int32_t i32_Meters_Spm90GetResponce(meters_context_t * context, uint8_t id, 
                                    uint16_t * buf, uint16_t count){
    int32_t ret;
    uint8_t resp[17];
    enum {MODBUS_WRAP_SIZE = 5};    
    uint16_t expected = (sizeof(uint16_t) * count) + MODBUS_WRAP_SIZE;
    if(expected > sizeof(resp))
        return -E2BIG;
                                        
    ret = bus485_recv(context->bus485, resp, ARRAY_SIZE(resp), 3000);
    if(ret < 0)
        return ret;

    uint16_t crc = crc16_reflect(0xA001, 0xFFFF, resp, expected - sizeof(uint16_t));
    uint16_t crc1 = (((uint16_t)resp[ret-1]) << 8) | resp[ret-2];
    if(crc != crc1)
        return -EBADMSG;
    
    if(resp[0] != id)
        return -EADDRNOTAVAIL;
    
    if (resp[1] != 0x03)
        return -ENODATA;
    
    if ((count * sizeof(uint16_t)) != resp[2])
        return -EMSGSIZE;
    
    for (uint16_t i = 0; i < count; i++){
        buf[i] = resp[4 + (i * 2)] | (resp[3 + (i * 2)] << 8);
    }
    
    return 0;
}

int32_t i32_Meters_Spm90GetValues(meters_context_t * context, uint16_t id, 
                                uint16_t baudrate, meters_valuesDC_t *shadow){
    int32_t ret;
    uint8_t req[8] = {id, 0x03, 0x00, 0x00, 0x00, 0x06};
    uint16_t registers[6] = {0};
    uint16_t crc = crc16_reflect(0xA001, 0xFFFF, req, 6);//u16_calculateCRC(req, 6);
    req[6] = crc & 0xff;
    req[7] = (crc >> 8) & 0xff;
    
    bus485_lock(context->bus485);
    ret = bus485_set_baudrate(context->bus485, baudrate);
    bus485_flush(context->bus485);
    ret = bus485_send(context->bus485, req, 8);
    if(ret < 0){
        bus485_release(context->bus485);
        return ret;
    }
    ret = i32_Meters_Spm90GetResponce(context, id, registers, ARRAY_SIZE(registers));
    
    if(ret == 0){
        //TODO write valid data to shadow
        shadow->voltage = registers[0] / 10.0f;
        shadow->current = registers[1] / 100.0f;
        uint32_t power_100mW = (registers[2] << 16) | registers[3];
        shadow->power = power_100mW / 10.0f;
        uint32_t energy_10Wh = (registers[4] << 16) | registers[5];
        shadow->energy = (uint64_t)energy_10Wh * 10 * 3600;
    }
    bus485_release(context->bus485);
    return ret;
}

int32_t i32_Meters_ReadSpm90(meters_context_t * context, uint32_t item_idx){
    int32_t ret;
    meters_item_t * item = &context->items[item_idx];
    meter_parameters_t *param = &context->parameters[item_idx];
    meters_valuesDC_t * shadow = &item->data.exDC.shadow;
    ret = i32_Meters_Spm90GetValues(context, param->address, param->baudrate, shadow);
    if(ret == 0){
        k_mutex_lock(&context->dataAccessMutex, K_FOREVER);
        {
            item->isValidValues = true;
            item->timemark = k_uptime_get_32();
            memcpy(&item->values.DC, shadow, sizeof(meters_valuesDC_t));
        }
        k_mutex_unlock(&context->dataAccessMutex);
    }
    return 0;
}

int32_t i32_Meters_InitSpm90(meters_context_t * context, uint32_t item_idx){
    if(item_idx >= context->itemCount)
        return -ERANGE;
    
    context->items[item_idx].badResponceCount = SPM90_ERROR_THRESHOLD;

    return 0;
}