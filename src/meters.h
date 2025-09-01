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
    meters_currentType_DC,
    meters_currentType_AC
}meters_currentType_t;

typedef struct{
    uint64_t energyActive;
    float current[3];
    float voltage[3];
    float powerActive;
    float frequency;
}meters_valuesAC_t;
    

typedef struct{
    uint64_t energy;
    float current;
    float voltage;
    float power;
}meters_valuesDC_t;

typedef struct{
    union{
        meters_valuesAC_t AC;
        meters_valuesDC_t DC;
    };
    meters_currentType_t type;
    //uint64_t timemark;
}meters_values_t;

typedef struct{
    meters_type_t type;
    uint32_t address;
    uint32_t baudrate;
    uint32_t currentFactor;
}meter_parameters_t;

typedef struct{
    meter_parameters_t parameters;
    meters_values_t values;
    int32_t isValid;
    uint32_t timemark;
}meter_itemInfo_t;

typedef struct{
    meter_itemInfo_t items[CONFIG_STRIM_METERS_ITEMS_MAX_COUNT];
    uint32_t count;
}meters_values_collection_t;

typedef int32_t (*meters_get_parameters_t)(meter_parameters_t *table, 
                                        uint32_t table_size, 
                                        void *user_data);

int32_t meters_init(meters_get_parameters_t cb, void *user_data);
int32_t meters_reinit(void);
int32_t meters_set_values(uint32_t idx, const meters_values_t *buffer);
int32_t meters_get_values(uint32_t idx, meters_values_t *buffer);
int32_t meters_get_all(meters_values_collection_t *buffer);
const uint8_t * meters_get_typename(meters_type_t type);
