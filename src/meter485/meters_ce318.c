#include "meters_ce318.h"
#include "bus485.h"
#include <zephyr/sys/util_macro.h>

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

enum{CE318_ERROR_THRESHOLD = 3};
#define DFF_FLAG (0x80)
#define DFF_FIELD_MAX_SIZE (sizeof(int64_t) + 1)

#define SMP_END (0xC0)
#define SMP_PROTOCOL_ID (6)

#define SMP_COMMAND_DATA (6)
#define SMP_COMMAND_ERROR (7)

typedef enum
{
  SMP_Command_GetDataSingle     =  1,
  SMP_Command_GetDataMultiple   =  2,
  SMP_Command_GetDataSingleEx   = 10,
  SMP_Command_GetDataMultipleEx = 11,

} SMP_Command_t;

typedef enum
{
  SMP_DataSingle_EnergyRegisteredActivePlus     =   1,
  SMP_DataSingle_EnergyRegisteredActiveMinus    =   2,
  SMP_DataSingle_EnergyRegisteredReactivePlus   =   3,
  SMP_DataSingle_EnergyRegisteredReactiveMinus  =   4,

  SMP_DataSingle_TotalPower                     =  13,
  SMP_DataSingle_PowerActivePlus                =  14,
  SMP_DataSingle_PowerActiveMinus               =  15,
  SMP_DataSingle_PowerReactivePlus              =  16,
  SMP_DataSingle_PowerReactiveMinus             =  17,

  SMP_DataSingle_PowerFactor                    =  25,
  SMP_DataSingle_Frequency                      =  26,

  SMP_DataSingle_Temperature                    =  31,
  SMP_DataSingle_Battery                        =  32,

  SMP_DataSingle_Time                           =  49,

  SMP_DataSingle_TotalPower_mVA                 = 105,
  SMP_DataSingle_TotalPower_mW                  = 106,
  SMP_DataSingle_TotalPower_mvar                = 107,
  

} SMP_DataSingle_t;

typedef enum
{
  SMP_DataSingleEx_Power          =  13,
  SMP_DataSingleEx_PowerActive    =  14,
  SMP_DataSingleEx_PowerReactive  =  16,
  SMP_DataSingleEx_Current        =  22,
  SMP_DataSingleEx_Voltage        =  24,
  SMP_DataSingleEx_PowerFactor    =  25,
  SMP_DataSingleEx_Frequency      =  26,

} SMP_DataSingleEx_t;


//#define BIT(_n_)  (1 << _n_)

typedef enum
{
  SMP_DataSingleExFlags_O = BIT(1),
  SMP_DataSingleExFlags_A = BIT(2),
  SMP_DataSingleExFlags_B = BIT(3),
  SMP_DataSingleExFlags_C = BIT(4),

} SMP_DataSingleExFlags_t;


#define SMP_NO_DFF        (0x00)
 
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
    return 0;
}

int32_t meters_ce318_get_response(meters_context_t * context, uint8_t * data, uint32_t length)
{   
    int32_t ret;
    uint8_t resp[256];
    uint8_t size = 0;   
                                           
    ret = bus485_recv(context->bus485, resp, ARRAY_SIZE(resp), CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
    if(ret < 0){
        LOG_WRN("get ce318 response error: %d", ret);
        return ret;
    }

    size += ret;

    //Пока под вопросом нужна ли дополнительное доскачивание, если он за раз всегда вычитывает
    if(resp[ret-1] != SMP_END){
        
        ret = bus485_recv(context->bus485, resp + ret, ARRAY_SIZE(resp) - ret, CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
        if(ret < 0){
            LOG_WRN("get ce318 second response error: %d", ret);
            return ret;
        }
        else 
            size += ret;
    }

    if(size > length){
        LOG_WRN("get ce318 check length error: %d", ret);
        return -EMSGSIZE;
    }

    if(resp[size - 1] != SMP_END || resp[0] != SMP_END){
        LOG_WRN("tail ce318 0xC0 error: %d", ret);
        return -EBADMSG;
    }
    
    uint8_t pack[256] = {0};
    
    size = ce318_remove_escape(&resp[1], pack, size - 2);
    
    uint16_t crc_test = (pack[size-2] << 8) | pack[size-1];

    uint16_t crc = 0;
    crc = ce318_get_crc16(crc, pack, size - 2);
    
    if(crc != crc_test){
        LOG_WRN("get ce318 crc error: %d", ret);
        return -EBADMSG;
    }
    memcpy(data, pack + 7, size - 9);

    return size - 9;
}

int32_t meters_ce318_get_voltage(meters_context_t *context, uint32_t baudrate, 
                                uint32_t address, float voltage[3])
{
  uint8_t query[] = {SMP_Command_GetDataSingleEx, SMP_NO_DFF, SMP_DataSingleEx_Voltage, 
                     SMP_DataSingleExFlags_A | SMP_DataSingleExFlags_B | SMP_DataSingleExFlags_C};


  int64_t value[3];

  int32_t ret = meters_ce318_send_packet(context, 4800, address, query, sizeof(query));

  if (ret != 0) {
    return ret;
  }

  uint8_t response[256];
  ret = meters_ce318_get_response(context, response, sizeof(response));
  if(ret < 0){
        bus485_release(context->bus485);
      return ret;
  }
  bus485_release(context->bus485);

  int32_t offset = ce318_dff_parce(response + 3, ret, &value[0], 1);
  offset += ce318_dff_parce(response+offset + 3, ret-offset, &value[1], 1);
  offset += ce318_dff_parce(response+offset + 3, ret-offset, &value[2], 1);

  if (voltage != NULL)
  {
    for (uint32_t i = 0; i < 3; i++)
    {
      voltage[i] = value[i] / 100.0;
    }
  }

  return 0;
}

int32_t meters_ce318_read(meters_context_t * context, uint32_t item_idx)
{
    int32_t ret;
    meters_item_t * item = &context->items[item_idx];
    meter_parameters_t *param = &context->parameters[item_idx];
    meters_values_ac_t * shadow = &item->data.ce318.shadow;

    ret = meters_ce318_get_voltage(context, param->baudrate, 
                                param->address, shadow->voltage);
                                
    if(ret == 0){
        item->bad_responce_count = 0;
        meters_values_t data = {.AC = *shadow, .type = meters_current_type_ac};
        meters_set_values(item_idx, &data);
    }
    else {
        if(item->is_valid_values && (++item->bad_responce_count > CE318_ERROR_THRESHOLD)){
            k_mutex_lock(&context->data_access_mutex, K_FOREVER);
            {
                item->is_valid_values = false;
            }
            k_mutex_unlock(&context->data_access_mutex);
        }
    }
    return 0;
}


int32_t meters_ce318_init(meters_context_t * context, uint32_t item_idx)
{
    if(item_idx >= context->item_count)
        return -ERANGE;
    
    meters_item_t * item = &context->items[item_idx];

    item->bad_responce_count = CE318_ERROR_THRESHOLD;
    return 0;
}