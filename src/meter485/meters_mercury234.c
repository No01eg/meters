#include "meters_private.h"
#include "bus485.h"
#include <zephyr/sys/crc.h>

enum{MERCURY_ERROR_THRESHOLD = 3};
enum{MERCURY_REQUEST_PAUSE = 30};

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

static int32_t meters_mercury_send(meters_context_t *context, uint8_t address, uint32_t baudrate,
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

static int32_t meters_mercury_request(meters_context_t *context, uint8_t address, uint32_t baudrate,
                                    const uint8_t *req_data, uint32_t req_length,
                                    uint8_t *rcv_buffer, uint32_t rcv_length )
{
    int32_t ret;
    ret = meters_mercury_send(context, address, baudrate, req_data, req_length);
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

int32_t meters_mercury_ping(meters_context_t *context, uint8_t address, uint32_t baudrate)
{
    int32_t ret;

    uint8_t req[1] = {0x00}; //проверка связи

    uint8_t rcv[1];

    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;
    
    if(rcv[0]!= 0x00){
        LOG_WRN("mercury ping error: %d", rcv[0]);
        return -EPROTO;
    }

    return 0;
}

static int32_t meters_mercury_connect(meters_context_t *context, uint8_t address, uint32_t baudrate)
{
    int32_t ret;

    uint8_t req[8] = {
        0x01, //команда открытия сессии связи
        0x01, // Первый уровень доступа (чтение)
    };
    const uint8_t * const password = "111111";//Пароль по умолчанию, согласно документации
    strncpy(&req[2], password, 6);

    uint8_t rcv[1];

    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;
    
    if(rcv[0]!= 0x00){
        LOG_WRN("mercury open session error: %d", rcv[0]);
        return -EPROTO;
    }

    return 0;
}

static int32_t meters_mercury_get_energy(meters_context_t *context, uint8_t address, 
                                        uint32_t baudrate, meters_values_ac_t *value)
{
    int32_t ret;

    uint8_t req[3] = {
        0x05, // чтение активной и реактивной энергии 
        0x00, // за все время со сброса
        0x00, // по сумме тарифов
    };

    uint8_t rcv[16];

    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;
    
    uint32_t energy_wh = (rcv[1] << 24) |
                         (rcv[0] << 16) |
                         (rcv[3] << 8) |
                         (rcv[2]);

    value->energy_active = energy_wh * 3600; // в Вт*с

    return 0;
}

static int32_t meters_mercury_get_power(meters_context_t *context, uint8_t address, 
                                        uint32_t baudrate, meters_values_ac_t *value)
{
    int32_t ret;

    uint8_t req[] = {
        0x08, // чтение параметров
        0x11, // мгновенная мощность
        0x00  // мощность активная по сумме фаз
    };

    uint8_t rcv[3];
    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;

    uint32_t power_10mw = ((rcv[0] & 0x3F) << 16) |
                           ( rcv[2]         <<  8) |
                           ( rcv[1]              );

    value->power_active = power_10mw / 100.0; //в Вт
    
    return 0;
}

static int32_t meters_mercury_get_voltage(meters_context_t * context, uint8_t address,
                                            uint32_t baudrate, meters_values_ac_t *value)
{
    int32_t ret;

    uint8_t req[3] = {
        0x08, // чтение параметров
        0x16, // чтение мгновенных значений по всем фазам
        0x11  // напряжение с указанием первой фазы, согласно документации
    };

    uint8_t rcv[9];
    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;

    for(uint32_t i = 0; i < 3; i++){
        uint32_t voltage_10mv = (rcv[0 + (3 * i)] << 16) |
                                (rcv[2 + (3 * i)] << 8) |
                                (rcv[1 + (3 * i)]);
        value->voltage[i] = voltage_10mv / 100.0; //значение в вольт
    }
    return 0;
}

static int32_t meters_mercury_get_current(meters_context_t *context, uint8_t address,
                                    uint32_t baudrate, meters_values_ac_t *value)
{
    int32_t ret;

    uint8_t req[3] ={
        0x08, //чтение параметров
        0x16, //мгновенные значения по всем фазам
        0x21  // чтение значения тока
    };

    uint8_t rcv[9];
    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;

    for(uint32_t i = 0; i < 3; i++){
        uint32_t current_ma = (rcv[0 + (3 * i)] << 16) |
                                (rcv[2 + (3 * i)] << 8) |
                                (rcv[1 + (3 * i)]);
        value->current[i] = current_ma / 1000.0; //значение в вольт
    }
    return 0;
}            

static int32_t meters_mercury_disconnect(meters_context_t *context, uint8_t address, uint32_t baudrate)
{
    int32_t ret;
    
    uint8_t req[1] = {0x02}; //закрытие сессии

    uint8_t rcv[1];

    ret = meters_mercury_request(context, address, baudrate, req, sizeof(req), rcv, sizeof(rcv));
    if(ret < 0)
        return ret;
    
    if(rcv[0]!= 0x00){
        LOG_WRN("mercury close session error: %d", rcv[0]);
        return -EPROTO;
    }

    return 0;
}


int32_t meters_mercury_init(meters_context_t * context, uint32_t item_idx){
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    meters_item_t * item = &context->items[item_idx];

    item->bad_responce_count = MERCURY_ERROR_THRESHOLD;
}