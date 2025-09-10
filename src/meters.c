#include "meters_private.h"
#include <stdbool.h>
#include "bus485.h"

#include "meters_spm90.h"
#include "meters_ce318.h"
#include "meters_poll485.h"

LOG_MODULE_REGISTER(meters, CONFIG_STRIM_METERS_LOG_LEVEL);


meters_context_t meters_context;

typedef struct {
    const char* name;
    meters_current_type_t values_type;
    meters_init_t init;
    meters_read_t read;
}meters_description_type_t;

static const meters_description_type_t meters_description_type[meters_type_lastIndex] = {
    [meters_type_extern_dc] = {.name = "EXTERNAL.DC",
                            .values_type = meters_current_type_dc,
                            .init = NULL,
                            .read = NULL},
    [meters_type_extern_ac] = {.name = "EXTERNAL.AC",
                            .values_type = meters_current_type_ac,
                            .init = NULL,
                            .read = NULL},
#if CONFIG_STRIM_METERS_BUS485_ENABLE
    [meters_type_SPM90]   = {.name = "SPM90",
                            .values_type = meters_current_type_dc,
                            .init = meters_spm90_init,
                            .read = meters_spm90_read},
    [meters_type_CE318]   = {.name = "CE318",
                            .values_type = meters_current_type_ac,
                            .init = meters_ce318_init,
                            .read = meters_ce318_read},
#endif                
};

meters_current_type_t meters_get_values_type(meters_type_t type){
  if (type < meters_type_lastIndex)
    return meters_description_type[type].values_type;
  
  // для неизвестных пусть будет DC
  return meters_current_type_dc;
}

meters_read_t meters_get_read_func(meters_type_t type)
{
  if (type < meters_type_lastIndex)
    return meters_description_type[type].read;
  
  return NULL;
}

meters_init_t meters_get_init_func(meters_type_t type)
{
  if (type < meters_type_lastIndex)
    return meters_description_type[type].init;
  
  return NULL;
}

static int32_t meters_initialize_context(meters_context_t *context, 
                                        meter_parameters_t *params, 
                                        uint8_t count)
{
    int32_t ret;
    //default properties
    for(uint32_t i = 0; i < CONFIG_STRIM_METERS_ITEMS_MAX_COUNT; i++){
        context->parameters[i].current_factor = 1;
        context->parameters[i].address = 0;
        context->parameters[i].type = meters_type_lastIndex;
    }

    if(params == NULL){
        return -EINVAL;
    }

	if(count > CONFIG_STRIM_METERS_ITEMS_MAX_COUNT)
		return -E2BIG;

	memcpy(context->parameters, params, count * sizeof(meter_parameters_t));

    context->item_count = count;

    for(uint32_t i = 0; i < context->item_count; i++){
        meters_type_t type = context->parameters[i].type;
        if(type >= meters_type_lastIndex){
            LOG_ERR("meter %u have unknown type %u\r\n", i, context->parameters[i].type);
            context->item_count = 0;
            return -ENOMSG;
        }
        context->items[i].values.type = meters_get_values_type(type);
        context->items[i].is_valid_values = false;
        meters_init_t init_func = meters_get_init_func(type);
        if (init_func != NULL)
        {
          ret = init_func(context, i);
          if (ret != 0)
          {
            LOG_ERR("init meter %d error: %d", i, ret);
            return ret;
          }
        }
    }
    return 0;
}


int32_t meters_set_values(uint32_t idx, const meters_values_t *buffer){
    meters_context_t *context = &meters_context;

    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->item_count)
        return -ERANGE;
    
    if(buffer->type != meters_description_type[context->parameters[idx].type].values_type)
        return -EINVAL; 
    
    k_mutex_lock(&context->data_access_mutex, K_FOREVER);
    {
        memcpy(&context->items[idx].values, buffer, sizeof(meters_values_t));
        context->items[idx].timemark = k_uptime_get_32();
        context->items[idx].is_valid_values = true;
    }
    k_mutex_unlock(&context->data_access_mutex);
    return 0;
}

int32_t meters_get_values(uint32_t idx, meters_values_t *buffer){
    meters_context_t *context = &meters_context;
    meters_item_t *item = &context->items[idx];
    
    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->item_count)
        return -ERANGE;

    int32_t ret = -ENXIO;

    k_mutex_lock(&context->data_access_mutex, K_FOREVER);
    {
        uint32_t curTimemark = k_uptime_get_32();
        if((curTimemark - item->timemark) > (CONFIG_STRIM_METERS_VALID_DATA_TIMEOUT))
            item->is_valid_values = false;
        if(item->is_valid_values){
            memcpy(buffer, &item->values, sizeof(meters_values_t));
            ret = 0;
        }
    }
    k_mutex_unlock(&context->data_access_mutex);

    return ret;
}

const uint8_t * meters_get_typename(meters_type_t type){
    if(type < meters_type_lastIndex)
        return meters_description_type[type].name;
    
    return "unknown";
}

int32_t meters_get_all(meters_values_collection_t *buffer){
    meters_context_t *context = &meters_context;

    if(buffer == NULL)
        return -EINVAL;
    buffer->count = 0;

    k_mutex_lock(&context->data_access_mutex, K_FOREVER);
    {
        for(uint32_t i = 0; i < context->item_count; i++){
            if((i < ARRAY_SIZE(context->items)) &&
                (i < ARRAY_SIZE(context->parameters)) && 
                (i < ARRAY_SIZE(buffer->items))){
                    uint32_t timemark = k_uptime_get_32();
                    if((timemark - context->items[i].timemark) > (CONFIG_STRIM_METERS_VALID_DATA_TIMEOUT))
                        buffer->items[i].is_valid = context->items[i].is_valid_values = false;
                    else
                        buffer->items[i].is_valid = context->items[i].is_valid_values;
                    buffer->items[i].timemark = context->items[i].timemark;
                    memcpy(&buffer->items[i].values, &context->items[i].values, sizeof(meters_values_t));
                    memcpy(&buffer->items[i].parameters, &context->parameters[i], sizeof(meter_parameters_t));
                    buffer->count++;
            }
        }
    }
    k_mutex_unlock(&context->data_access_mutex);

    return 0;
}

//TODO реализовать остановку всех подчиненных потоков, заблокировать чтение мьютексом 
// при задании новых параметров
int32_t meters_reinit(void){
#if 0
    meters_context_t *context = &meters_context;
    k_sem_give(&context->reinitSem);
#endif
    return 0;
}

int32_t meters_init(meter_parameters_t *parameters, uint8_t count){
    meters_context_t * context = &meters_context;
    int32_t ret;
    
    if(parameters == NULL)
        return -EINVAL;

    k_mutex_init(&context->data_access_mutex);
    k_sem_init(&context->reinitSem, 0 ,1);

    meters_initialize_context(context, parameters, count);
    
    (void)ret;

#ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    context->bus485 = DEVICE_DT_GET(DT_CHOSEN(strim_meter_bus485));
    meters_poll485_thread_run(context);
#endif

    return 0;
}
