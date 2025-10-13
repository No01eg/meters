#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/internal/syscall_handler.h>
#include <zephyr/app_memory/app_memdomain.h>
#include "meters_private.h"
#include "bus485.h"

#include "meters_spm90.h"
#include "meters_ce318.h"
#include "meters_mercury234.h"
#include "meters_poll485.h"

LOG_MODULE_REGISTER(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

struct k_mem_domain app0_domain;    
K_APPMEM_PARTITION_DEFINE(app_part0);
K_APP_BMEM(app_part0) meters_tools_context_t __aligned(32) tools_context;
K_APP_DMEM(app_part0) meters_context_t __aligned(32) meters_context;


struct k_mem_partition *app0_parts[] = {
    &app_part0,
    &k_log_partition
};                       

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
    [meters_type_Mercury234] = {.name = "MERCURY234",
                                .values_type = meters_current_type_ac,
                                .init = meters_mercury_init,
                                .read = meters_mercury_read},
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
            LOG_ERR("meter %u have unknown type %u", i, context->parameters[i].type);
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

int32_t z_impl_meters_set_values(uint32_t idx, const meters_values_t *buffer){
    meters_context_t *context = &meters_context;
    meters_tools_context_t *tool = context->tools;
    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->item_count)
        return -ERANGE;
    
    if(buffer->type != meters_description_type[context->parameters[idx].type].values_type)
        return -EINVAL; 
    
    k_mutex_lock(&tool->data_access_mutex, K_FOREVER);
    {
        memcpy(&context->items[idx].values, buffer, sizeof(meters_values_t));
        context->items[idx].timemark = k_uptime_get_32();
        context->items[idx].is_valid_values = true;
    }
    k_mutex_unlock(&tool->data_access_mutex);
    return 0;
}

#if CONFIG_USERSPACE
static int32_t z_vrfy_meters_set_values(uint32_t idx, const meters_values_t *buffer)
{
    int32_t ret;
    K_OOPS(K_SYSCALL_MEMORY_WRITE(buffer, sizeof(*buffer)));
    ret = z_impl_meters_set_values(idx, buffer);

    return ret;
}

#include <zephyr/syscalls/meters_set_values_mrsh.c>
#endif

int32_t z_impl_meters_get_values(uint32_t idx, meters_values_t *buffer){
    meters_context_t *context = &meters_context;
    meters_item_t *item = &context->items[idx];
    meters_tools_context_t *tool = context->tools;
    
    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->item_count)
        return -ERANGE;

    int32_t ret = -ENXIO;

    k_mutex_lock(&tool->data_access_mutex, K_FOREVER);
    {
        uint32_t curTimemark = k_uptime_get_32();
        if((curTimemark - item->timemark) > (CONFIG_STRIM_METERS_VALID_DATA_TIMEOUT))
            item->is_valid_values = false;
        if(item->is_valid_values){
            memcpy(buffer, &item->values, sizeof(meters_values_t));
            ret = 0;
        }
    }
    k_mutex_unlock(&tool->data_access_mutex);

    return ret;
}

#if CONFIG_USERSPACE
static int32_t z_vrfy_meters_get_values(uint32_t idx, meters_values_t *buffer)
{
    meters_values_t copy_values;
    int32_t ret;

    if(k_usermode_from_copy(&copy_values, buffer, sizeof(*buffer)) != 0){
        return -EPERM;
    }
    

    ret = z_impl_meters_get_values(idx, &copy_values);

    if(k_usermode_to_copy(buffer, &copy_values, sizeof(*buffer)) != 0){
        return -EPERM;
    }

    return ret;
}

#include <zephyr/syscalls/meters_get_values_mrsh.c>
#endif

const uint8_t * meters_get_typename(meters_type_t type){
    if(type < meters_type_lastIndex)
        return meters_description_type[type].name;
    
    return "unknown";
}

int32_t z_impl_meters_get_all(meters_values_collection_t *buffer){
    meters_context_t *context = &meters_context;
    meters_tools_context_t *tool = context->tools;

    if(buffer == NULL)
        return -EINVAL;
    buffer->count = 0;

    k_mutex_lock(&tool->data_access_mutex, K_FOREVER);
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
    k_mutex_unlock(&tool->data_access_mutex);

    return 0;
}

#if CONFIG_USERSPACE
static int32_t z_vrfy_meters_get_all(meters_values_collection_t *buffer)
{
    meters_values_collection_t copy_values;
    int32_t ret;

    if(k_usermode_from_copy(&copy_values, buffer, sizeof(*buffer)) != 0){
        return -EPERM;
    }

    ret = z_impl_meters_get_all(&copy_values);

    if(k_usermode_to_copy(buffer, &copy_values, sizeof(*buffer)) != 0){
        return -EPERM;
    }

    return ret;
}

#include <zephyr/syscalls/meters_get_all_mrsh.c>
#endif

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
    
    context->tools = &tools_context;
    meters_tools_context_t *tool = context->tools;
    
    if(parameters == NULL)
        return -EINVAL;

    k_mutex_init(&tool->data_access_mutex);
    k_sem_init(&tool->reinitSem, 0 ,1);

    meters_initialize_context(context, parameters, count);
    
    (void)ret;
    k_mem_domain_init(&app0_domain, ARRAY_SIZE(app0_parts), app0_parts);

#ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    tool->bus485 = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(strim_meter_bus485));
    if(tool->bus485 == NULL){
        LOG_ERR("bus485 init error nullpoint");
        return -ENXIO;
    }


    k_tid_t thread_id = meters_poll485_thread_run(context);

    k_object_access_grant(&tool->data_access_mutex, &tool->poll485_thread);
    k_object_access_grant(tool->bus485, &tool->poll485_thread);
    k_mem_domain_add_thread(&app0_domain, thread_id);
#endif

    return 0;
}
