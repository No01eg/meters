#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>

typedef enum {
    meters_type_extern_ac,
    meters_type_extern_dc,
#ifdef CONFIG_STRIM_METERS2_BUS485_ENABLE
    meters_type_CE318,
    meters_type_Mercury234,
    meters_type_SPM90,
#endif
    meters_type_lastIndex
}meters_type_t;

typedef enum{
    meters_current_type_dc,
    meters_current_type_ac
}meters_current_type_t;

typedef struct{
    uint64_t energy_active;
    float current[3];
    float voltage[3];
    float power_active;
    float frequency;
}meters_values_ac_t;
    

typedef struct{
    uint64_t energy;
    float current;
    float voltage;
    float power;
}meters_values_dc_t;

typedef struct{
    union{
        meters_values_ac_t AC;
        meters_values_dc_t DC;
    };
    meters_current_type_t type;
}meters_values_t;

typedef struct{
    meters_type_t type;
    uint32_t address;
    uint32_t baudrate;
    uint32_t current_factor;
}meter_parameters_t;

typedef struct{
    meter_parameters_t parameters;
    meters_values_t values;
    int32_t is_valid;
    uint32_t timemark;
}meter_item_info_t;

typedef struct{
    meter_item_info_t items[CONFIG_STRIM_METERS2_ITEMS_MAX_COUNT];
    uint32_t count;
}meters_values_collection_t;

int32_t meters_init(meter_parameters_t *parameters, uint8_t count);
int32_t meters_reinit(void);
__syscall int32_t meters_set_values(uint32_t idx, const meters_values_t *buffer);
__syscall int32_t meters_get_values(uint32_t idx, meters_values_t *buffer);
__syscall int32_t meters_get_all(meters_values_collection_t *buffer);
const uint8_t * meters_get_typename(meters_type_t type);

#include <zephyr/syscalls/meters.h>
