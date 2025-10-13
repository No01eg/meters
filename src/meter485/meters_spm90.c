#include "meters_spm90.h"
#include "bus485.h"
#include <zephyr/sys/crc.h>

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

enum {SPM90_ERROR_THRESHOLD = 3};

static int32_t meters_spm90_get_responce(meters_context_t * context, uint8_t id, 
                                    uint16_t * buf, uint16_t count)
{
    meters_tools_context_t *tool = context->tools;
    int32_t ret;
    uint8_t resp[17];
    enum {MODBUS_WRAP_SIZE = 5};    
    uint16_t expected = (sizeof(uint16_t) * count) + MODBUS_WRAP_SIZE;

    if(expected > sizeof(resp))
        return -E2BIG;
                                        
    ret = bus485_recv(tool->bus485, resp, ARRAY_SIZE(resp), CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
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

int32_t meters_spm90_get_values(meters_context_t * context, uint16_t id, 
                                uint16_t baudrate, meters_values_dc_t *shadow, uint32_t is_wait_before_send)
{
    meters_tools_context_t *tool = context->tools;
    int32_t ret;
    uint8_t req[8] = {id, 0x03, 0x00, 0x00, 0x00, 0x06};
    uint16_t registers[6] = {0};
    uint16_t crc = crc16_reflect(0xA001, 0xFFFF, req, 6);
    req[6] = crc & 0xff;
    req[7] = (crc >> 8) & 0xff;
    
    bus485_lock(tool->bus485);
    
    ret = bus485_set_baudrate(tool->bus485, baudrate);
    if(ret < 0){
        LOG_ERR("set baudrate error: %d", ret);
        bus485_release(tool->bus485);
        return ret;
    }

    bus485_flush(tool->bus485);
    
    if(is_wait_before_send)
        k_sleep(K_MSEC(CONFIG_STRIM_SPM90_SILENSE_BEFORE_REQUEST));

    ret = bus485_send(tool->bus485, req, 8);
    if(ret < 0){
        LOG_ERR("send spm90 query error: %d", ret);
        bus485_release(tool->bus485);
        return ret;
    }

    ret = meters_spm90_get_responce(context, id, registers, ARRAY_SIZE(registers));
    
    if(ret == 0){
        shadow->voltage = registers[0] / 10.0f;
        shadow->current = registers[1] / 100.0f;
        uint32_t power_100mW = (registers[2] << 16) | registers[3];
        shadow->power = power_100mW / 10.0f;
        uint32_t energy_10Wh = (registers[4] << 16) | registers[5];
        shadow->energy = (uint64_t)energy_10Wh * 10 * 3600;
    }
    bus485_release(tool->bus485);
    return ret;
}



int32_t meters_spm90_read(meters_context_t * context, uint32_t item_idx)
{
    int32_t ret;
    meters_item_t * item = &context->items[item_idx];
    meter_parameters_t *param = &context->parameters[item_idx];
    meters_values_dc_t * shadow = &item->data.spm90.shadow;
    meters_tools_context_t *tool = context->tools;

    uint32_t is_wait_before_send = 0;
    
    if(!item->is_valid_values) {
        if(k_uptime_get_32() - item->error_timemark < CONFIG_STRIM_SPM90_WAIT_AFTER_ERROR)
            return 0;//пропускаем опрос, пока не вышло время
        
        item->error_timemark = 0;
        is_wait_before_send = 1;
    }

    ret = meters_spm90_get_values(context, param->address, param->baudrate, shadow, is_wait_before_send);
    if(ret == 0){
        if(!item->is_valid_values)
            LOG_INF("spm90 poll recovered");
        item->bad_responce_count = 0;
        meters_values_t data = {.DC = *shadow, .type = meters_current_type_dc};
        meters_set_values(item_idx, &data);
    } 
    else {
        if(item->is_valid_values && (++item->bad_responce_count > SPM90_ERROR_THRESHOLD)){
            if(item->is_valid_values)
                LOG_DBG("spm90 exclude of poll next %d ms", CONFIG_STRIM_SPM90_WAIT_AFTER_ERROR);
            
            k_mutex_lock(&tool->data_access_mutex, K_FOREVER);
            {
                item->is_valid_values = false;
            }
            k_mutex_unlock(&tool->data_access_mutex);
            item->error_timemark = k_uptime_get_32();

        }
        else if(!item->is_valid_values && item->error_timemark == 0){
            item->error_timemark = k_uptime_get_32();
        }
        if(item->is_valid_values){
            if((ret != -ETIMEDOUT) && (ret != -EILSEQ) && (ret != -EBADMSG) &&
            (ret != -EADDRNOTAVAIL) && (ret != -ENODATA) && (ret != -EMSGSIZE))
                LOG_ERR("read spm90 %d error: %d", param->address, ret);
            else {
                LOG_DBG("spm90 error: %d", ret);
            }
        }
    }
    is_wait_before_send = 0;
    
    return 0;
}

int32_t meters_spm90_init(meters_context_t * context, uint32_t item_idx)
{
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    context->items[item_idx].bad_responce_count = SPM90_ERROR_THRESHOLD;

    return 0;
}