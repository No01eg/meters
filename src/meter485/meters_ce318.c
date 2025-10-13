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
  smp_command_get_data_single     =  1,
  smp_command_get_data_multiple   =  2,
  smp_command_get_data_singleEx   = 10,
  smp_command_get_data_multipleEx = 11,

} smp_command_t;

typedef enum
{
  smp_data_single_energy_reg_active_plus     =   1,
  smp_data_single_energy_reg_active_minus    =   2,
  smp_data_single_energy_reg_reactive_plus   =   3,
  smp_data_single_energy_reg_reactive_minus  =   4,

  smp_data_single_total_power                =  13,
  smp_data_single_power_active_plus          =  14,
  smp_data_single_power_active_minus         =  15,
  smp_data_single_power_reactive_plus        =  16,
  smp_data_single_power_reactive_minus       =  17,

  smp_data_single_power_factor               =  25,
  smp_data_single_frequency                  =  26,

  smp_data_single_temperature                =  31,
  smp_data_single_battery                    =  32,

  smp_data_single_time                       =  49,

  smp_data_single_total_power_mVA            = 105,
  smp_data_single_total_power_mW             = 106,
  smp_data_single_total_power_mvar           = 107,
  

} smp_data_single_t;

typedef enum
{
  smp_data_singleEx_power           =  13,
  smp_data_singleEx_power_active    =  14,
  smp_data_singleEx_power_reactive  =  16,
  smp_data_singleEx_current         =  22,
  smp_data_singleEx_voltage         =  24,
  smp_data_singleEx_power_factor    =  25,
  smp_data_singleEx_frequency       =  26,

} smp_data_singleEx_t;


typedef enum
{
  smp_data_singleEx_flag_O = BIT(1),
  smp_data_singleEx_flag_a = BIT(2),
  smp_data_singleEx_flag_b = BIT(3),
  smp_data_singleEx_flag_c = BIT(4),

} smp_data_singleEx_flags_t;

typedef enum
{
  smp_phase_a   = 0,
  smp_phase_b   = 1,
  smp_phase_c   = 2,
  smp_phase_abc = 3,

} smp_phase_t;


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

    
    if(remaining == 0)
    return -4;
    
    do{
        if(field_size > DFF_FIELD_MAX_SIZE)
        return -3;
        
        result |= (*ch & 0x7F) << (field_size * 7);
        field_size++;
        
        if(signed_field){
            minus = (*ch & 0x40) != 0;
        }
        
    }while((*ch++ & DFF_FLAG) != 0);

    if(signed_field && minus){
       result |= 0xffffffffffffffff << (field_size * 7);
    }

    *field = (int64_t)result;

    return field_size;
}

static uint8_t meters_ce318_get_phase(smp_phase_t phase)
{
  uint8_t flags = 0;

  switch (phase)
  {
    case smp_phase_a:
      flags = smp_data_singleEx_flag_a;
      break;

    case smp_phase_b:
      flags = smp_data_singleEx_flag_b;
      break;

    case smp_phase_c:
      flags = smp_data_singleEx_flag_c;
      break;

    case smp_phase_abc:
    default:
      flags = smp_data_singleEx_flag_O;
      break;
  }

  return flags;
}

static int32_t meters_ce318_send_packet(meters_context_t * context, 
                                uint32_t baudrate, uint32_t address, 
                                const uint8_t * data, uint32_t length)
{
    meters_tools_context_t *tool = context->tools;
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

    bus485_lock(tool->bus485);
    ret = bus485_set_baudrate(tool->bus485, baudrate);
    if(ret < 0){
        bus485_release(tool->bus485);
        LOG_ERR("set baudrate error: %d", ret);
        return ret;
    }

    bus485_flush(tool->bus485);
    ret = bus485_send(tool->bus485, pack, count);
    if(ret < 0){
        bus485_release(tool->bus485);
        LOG_ERR("ce318 send error: %d", ret);
        return ret;
    }
    return 0;
}

static int32_t meters_ce318_get_response(meters_context_t * context, uint8_t * data, uint32_t length)
{   
    meters_tools_context_t *tool = context->tools;
    int32_t ret;
    uint8_t resp[256];
    uint8_t size = 0;   
                                           
    ret = bus485_recv(tool->bus485, resp, ARRAY_SIZE(resp), CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
    if(ret < 0){
        return ret;
    }

    size += ret;

    //Пока под вопросом нужна ли дополнительное доскачивание, если он за раз всегда вычитывает
    if(resp[ret-1] != SMP_END){
        
        ret = bus485_recv(tool->bus485, resp + ret, ARRAY_SIZE(resp) - ret, CONFIG_STRIM_METERS_BUS485_RESPONSE_TIMEOUT);
        if(ret < 0){
            return ret;
        }
        else 
            size += ret;
    }

    if(size > length){
        return -EMSGSIZE;
    }

    if(resp[size - 1] != SMP_END || resp[0] != SMP_END){
        return -EBADMSG;
    }
    
    uint8_t pack[256] = {0};
    
    size = ce318_remove_escape(&resp[1], pack, size - 2);
    
    uint16_t crc_test = (pack[size-2] << 8) | pack[size-1];

    uint16_t crc = 0;
    crc = ce318_get_crc16(crc, pack, size - 2);
    
    if(crc != crc_test){
        return -EBADMSG;
    }
    memcpy(data, pack + 7, size - 9);

    return size - 9;
}

static int32_t meters_ce318_poll(meters_context_t *context, ce318_poll_data_t *poll_data, 
                        int64_t *values, uint32_t values_count)
{
    meters_tools_context_t *tool = context->tools;
    int32_t ret;

    ret = meters_ce318_send_packet(context, poll_data->baudrate, poll_data->address,
                                poll_data->query, poll_data->query_length);
    if(ret != 0){
        return ret;
    }

    uint8_t response[256];
    ret = meters_ce318_get_response(context, response, sizeof(response));
    if(ret < 0){
            bus485_release(tool->bus485);
        return ret;
    }
    bus485_release(tool->bus485);

    int32_t offset = 0;

    for(uint32_t i = 0; i < values_count; i++)
        offset += ce318_dff_parce(response + offset + 3, ret - offset, &values[i], poll_data->is_signed_values);

    return 0;
}

int32_t meters_ce318_get_battery(meters_context_t *context, uint32_t baudrate,
                                uint32_t address, uint8_t *hex)
{
    meters_tools_context_t *tool = context->tools;
    uint8_t query[] = {smp_command_get_data_single, SMP_NO_DFF, smp_data_single_battery};
    
    uint8_t data[8];

    int32_t ret;

    ret = meters_ce318_send_packet(context, baudrate, address,
                                query, sizeof(query));
    if(ret != 0){
        return ret;
    }


    ret = meters_ce318_get_response(context, data, sizeof(data));
    if(ret < 0){
            bus485_release(tool->bus485);
        return ret;
    }
    bus485_release(tool->bus485);
    
    memcpy(hex, data, ret);
    return ret;
}

int32_t meters_ce318_get_voltage(meters_context_t *context, uint32_t baudrate, 
                                uint32_t address, float voltage[3])
{
  uint8_t query[] = {smp_command_get_data_singleEx, SMP_NO_DFF, smp_data_singleEx_voltage, 
                     smp_data_singleEx_flag_a | smp_data_singleEx_flag_b | smp_data_singleEx_flag_c};

  int64_t value[3];

  ce318_poll_data_t poll_data = {
    .address = address,
    .baudrate = baudrate,
    .query = query,
    .query_length = sizeof(query),
    .is_signed_values = 1
  };
  int32_t ret = meters_ce318_poll(context, &poll_data, value, 3);
  if(ret < 0){
    return ret;
  }

  if (voltage != NULL)
  {
    for (uint32_t i = 0; i < 3; i++)
    {
      voltage[i] = value[i] / 100.0;
    }
  }

  return 0;
}

int32_t meters_ce318_get_current(meters_context_t *context, uint32_t baudrate, 
                                uint32_t address, float current[3])
{
  uint8_t query[] = {smp_command_get_data_singleEx, SMP_NO_DFF, smp_data_singleEx_current, 
                     smp_data_singleEx_flag_a | smp_data_singleEx_flag_b | smp_data_singleEx_flag_c};


  int64_t value[3];

  ce318_poll_data_t poll_data = {
    .address = address,
    .baudrate = baudrate,
    .query = query,
    .query_length = sizeof(query),
    .is_signed_values = 1
  };
  int32_t ret = meters_ce318_poll(context, &poll_data, value, 3);
  if (ret < 0)
    return ret;

  if (current != NULL)
  {
    for (uint32_t i = 0; i < 3; i++)
    {
      current[i] = value[i] / 1000.0;
    }
  }

  return 0;
}

int32_t meters_ce318_get_energy_active(meters_context_t * context, uint32_t baudrate,
                                uint32_t address, uint64_t * energy)
{
  uint8_t query[] = {smp_command_get_data_single, SMP_NO_DFF, 
                    smp_data_single_energy_reg_active_plus, 0};
  int64_t value;

  ce318_poll_data_t poll_data = {
    .address = address,
    .baudrate = baudrate,
    .query = query,
    .query_length = sizeof(query),
    .is_signed_values = 0
  };
  int32_t ret = meters_ce318_poll(context, &poll_data, &value, 1);
  if (ret < 0)
    return ret;

  if (energy != NULL)
    *energy = (uint64_t)(value * 360); // десятитысячные доли киловатт-часов в ватт-секунды.

  return 0;
}

int32_t meters_ce318_get_power_active(meters_context_t * context, uint32_t baudrate,
                                uint32_t address, float *power, smp_phase_t phase)
{
    uint8_t query[] = {smp_command_get_data_singleEx, SMP_NO_DFF, 
                    smp_data_singleEx_power_active, 0};
    query[3] = meters_ce318_get_phase(phase);
    
    int64_t value;  

    ce318_poll_data_t poll_data = {
        .address = address,
        .baudrate = baudrate,
        .query = query,
        .query_length = sizeof(query),
        .is_signed_values = 0
    };
    int32_t ret = meters_ce318_poll(context, &poll_data, &value, 1);
    if (ret < 0)
        return ret;

    if(power != NULL){
        *power = value;
    }
    return 0;
}                                

int32_t meters_ce318_read(meters_context_t * context, uint32_t item_idx)
{
    int32_t ret;
    meters_item_t * item = &context->items[item_idx];
    meters_tools_context_t *tool = context->tools;
    meter_parameters_t *param = &context->parameters[item_idx];
    meters_values_ac_t * shadow = &item->data.ce318.shadow;

    ret = meters_ce318_get_voltage(context, param->baudrate, 
                                param->address, shadow->voltage);
    if(ret < 0){
        goto ce_318_end_poll;
    }
    
    ret = meters_ce318_get_current(context, param->baudrate, 
                                param->address, shadow->current);
    if(ret < 0){
        goto ce_318_end_poll;
    }
    
    ret = meters_ce318_get_energy_active(context, param->baudrate, 
                                param->address, &shadow->energy_active);
    if(ret < 0){
        goto ce_318_end_poll;
    }

    ret = meters_ce318_get_power_active(context, param->baudrate,
                                param->address, &shadow->power_active, smp_phase_abc);
    if(ret < 0){
        goto ce_318_end_poll;
    }                                
    
    ce_318_end_poll:
    if(ret == 0){
        if(!item->is_valid_values)
          LOG_INF("ce318 poll recovered");
        item->bad_responce_count = 0;
        meters_values_t data = {.AC = *shadow, .type = meters_current_type_ac};
        meters_set_values(item_idx, &data);
    }
    else {
        if(item->is_valid_values && (++item->bad_responce_count > CE318_ERROR_THRESHOLD)){
            k_mutex_lock(&tool->data_access_mutex, K_FOREVER);
            {
                item->is_valid_values = false;
            }
            k_mutex_unlock(&tool->data_access_mutex);
        }

        if(item->is_valid_values){
            if((ret != -ETIMEDOUT) && (ret != -EILSEQ) && (ret != -EBADMSG) &&
            (ret != -EADDRNOTAVAIL) && (ret != -ENODATA) && (ret != -EMSGSIZE))
                LOG_ERR("read ce318 %d error: %d", param->address, ret);
            else {
                LOG_DBG("ce318 error: %d", ret);
            }
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