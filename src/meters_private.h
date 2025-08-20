#pragma once

#include "meters.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/shell/shell.h>

typedef struct{
    meters_valuesDC_t shadow;
}meters_dataExternDC_t;

typedef struct{
    meters_valuesAC_t shadow;
}meters_dataExternAC_t;

typedef union{
    meters_dataExternAC_t exAC;
    meters_dataExternDC_t exDC;
#ifdef CONFIG_STRIM_BUS485_ENABLE
#endif    
}meters_data_t;

typedef struct{
    meters_values_t values;
    meters_data_t data;
    uint32_t isValidValues;
    uint64_t timemark;  
}meters_item_t;

typedef struct{
#ifdef CONFIG_STRIM_BUS485_ENABLE
    const struct device *bus485;
#endif
    meters_item_t items[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    meter_parameters_t parameters[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    uint32_t itemCount;
    struct k_mutex dataAccessMutex;
    struct k_sem reinitSem;
    struct k_thread baseThread;
    k_thread_stack_t *baseStack;
    size_t baseStackSize;

    meters_get_parameters_t getParameters;
    void * user_data;
}meters_context_t;

extern meters_context_t metersContext;