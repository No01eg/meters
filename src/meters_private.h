#pragma once

#include "meters.h"


#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>

typedef struct{
    meters_values_dc_t shadow;
}meters_data_extern_dc_t;

typedef struct{
    meters_values_ac_t shadow;
}meters_data_extern_ac_t;

typedef struct {
    meters_values_dc_t shadow;
}meters_data_spm90_t;

typedef struct {
    meters_values_ac_t shadow;
}meters_data_ce318_t;

typedef union{
    meters_data_extern_ac_t extern_ac;
    meters_data_extern_dc_t extern_dc;
#ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    meters_data_spm90_t spm90;
    meters_data_ce318_t ce318;
#endif    
}meters_data_t;

typedef struct{
    meters_values_t values;
    meters_data_t data;
    uint32_t is_valid_values;
    uint32_t timemark;  
    uint32_t error_timemark;
    uint32_t bad_responce_count;
}meters_item_t;

typedef struct{
#ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    const struct device *bus485;
#endif
    meters_item_t items[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    meter_parameters_t parameters[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    uint32_t item_count;
    struct k_mutex data_access_mutex;
    struct k_sem reinitSem;
    struct k_thread poll485_thread;
    k_thread_stack_t *poll485_stack;
    size_t poll485_stack_size;

    //void * user_data;
}meters_context_t;

extern meters_context_t meters_context;

typedef int32_t (*meters_init_t)(meters_context_t *context, uint32_t itemIndex);
typedef int32_t (*meters_read_t)(meters_context_t *context, uint32_t itemIndex);

meters_read_t meters_get_read_func(meters_type_t type);