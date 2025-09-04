#pragma once

#include <stdint.h>

typedef enum {
    meters_type_externAC,
    meters_type_externDC,
#ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    meters_type_CE318,
    meters_type_Mercury234,
    meters_type_SPM90,
#endif
    meters_type_lastIndex
}meters_type_t;

typedef enum{
    meters_current_type_DC,
    meters_current_type_AC
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
    meter_item_info_t items[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    uint32_t count;
}meters_values_collection_t;

int32_t meters_init(meter_parameters_t *parameters, uint8_t count);
int32_t meters_reinit(void);
int32_t meters_set_values(uint32_t idx, const meters_values_t *buffer);
int32_t meters_get_values(uint32_t idx, meters_values_t *buffer);
int32_t meters_get_all(meters_values_collection_t *buffer);
const uint8_t * meters_get_typename(meters_type_t type);
