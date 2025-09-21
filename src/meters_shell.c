#include "meters_private.h"
#include "meters_spm90.h"
#include "meters_ce318.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void get_meter_address(uint8_t * addr_str, uint32_t buffSize, meter_parameters_t *param);
static void shell_values(const struct shell * shell, meters_values_t *values, bool horizontal);

#if CONFIG_STRIM_METERS_BUS485_ENABLE

  typedef struct {
    const uint8_t *name;
    int32_t (*func)(const struct shell *shell, uint32_t address, uint32_t baudrate);
  }meters_query_table_t;

  static int32_t query_energy(const struct shell *shell, uint32_t address, uint32_t baudrate)
  {
    
    meters_context_t * context = &meters_context;
    uint64_t energy;
    
    int32_t ret = meters_ce318_get_energy_active(context, baudrate, address, &energy);
    if(ret == 0){
      uint64_t energy_Wh = energy / 3600;
      uint32_t energy_kWh_fractional = energy_Wh % 1000;
      uint32_t energy_kWh_integer = energy_Wh / 1000;
      shell_print(shell, "ce318 energy = %6u.%03u  kWh", energy_kWh_integer, energy_kWh_fractional);
    }
    else
      shell_warn(shell, "ce318 error = %d", ret);
    
    return 0;
  }

  static int32_t query_voltage(const struct shell *shell, uint32_t address, uint32_t baudrate)
  {
    meters_context_t * context = &meters_context;
    float voltage[3];

    int32_t ret = meters_ce318_get_voltage(context, baudrate, address, voltage);
    if(ret == 0)
      shell_print(shell, "ce318 voltage = %5.3f/%5.3f/%5.3f", (double)voltage[0], (double)voltage[1], (double)voltage[2]);
    else
      shell_warn(shell, "ce318 error = %d", ret);

    return 0;
  }

  static meters_query_table_t meters_query_table[] = {
    {"energy", query_energy},
    {"voltage", query_voltage},
  };

  static int32_t ce318_query_cmd(const struct shell *shell,
                                size_t argc, uint8_t **argv)
  {
    if(argc < 3){
      shell_warn(shell, "incorrect arguments, enter <address> and <baudrate>");
      return 0;  
    }

    for(uint32_t i = 0; i < ARRAY_SIZE(meters_query_table); i++) {
      if(0 == strcmp(argv[1], meters_query_table[i].name)){
        uint32_t address = strtol(argv[2], NULL, 10);
        
        uint32_t baudrate = 4800;
        if(argc == 4)
          baudrate = strtol(argv[3], NULL, 10);
        shell_print(shell, "set baudrate to  %u", baudrate);
        
        int32_t ret = meters_query_table[i].func(shell, address, baudrate);
        if(ret != 0)
          shell_warn(shell, "query error: %d", ret);
        break;
      }
    }
    return 0;
  }

  static int32_t ce318_sample_cmd(const struct shell *shell,
                                  size_t argc, uint8_t **argv)
  {
    shell_warn(shell, "ce318 sample query");
    return 0;
  }                                

  static void cmd_query_get(size_t idx, struct shell_static_entry *entry)
  {
    if(idx < ARRAY_SIZE(meters_query_table)) {
      entry->syntax = meters_query_table[idx].name;
      entry->handler = NULL;
      entry->subcmd = NULL;
      entry->help = NULL;
    }
    else {
      entry->syntax = NULL;
    }
  }

  SHELL_DYNAMIC_CMD_CREATE(sub_ce318_query, cmd_query_get);

  SHELL_STATIC_SUBCMD_SET_CREATE(sub_ce318,
    SHELL_CMD_ARG(query, &sub_ce318_query,  "Query parameters", ce318_query_cmd, 4, 0),
    SHELL_CMD(sample,     NULL,             "Query sample battery", ce318_sample_cmd),
    SHELL_SUBCMD_SET_END
  );

  static int32_t spm90_read_cmd(const struct shell * shell, size_t argc, uint8_t ** argv)
  {
    meters_context_t * context = &meters_context;
    meters_values_dc_t value;

    uint16_t id = (uint16_t)atoi(argv[1]);

    uint32_t baudrate = 9600;
    if(argc > 2){
      baudrate = (uint32_t)atoi(argv[2]);
    }
    shell_print(shell, "set baudrate to  %u", baudrate);

    int32_t ret = meters_spm90_get_values(context, id, baudrate, &value, 0);
    if(ret == -ETIMEDOUT){
      shell_warn(shell, "meter %u no response", id);
      return 0;
    }
    else if(ret != 0){
      shell_error(shell, "meter %u error: %d", id, ret);
      return 0;
    }

    shell_print(shell, "Voltage:  %8.1f V", (double)value.voltage);
    shell_print(shell, "Current:  %8.2f A", (double)value.current);
    shell_print(shell, "Power:    %8.0f W", (double)value.power);
    uint64_t energy_Wh = value.energy / 3600;
    uint32_t energy_kWh_int = energy_Wh / 1000;
    uint32_t energy_kWh_fract = energy_Wh % 1000;
    shell_print(shell, "Energy:   %6u.%03u kWh", energy_kWh_int, energy_kWh_fract);

    return 0;
  }
#endif //CONFIG_STRIM_METERS_BUS485_ENABLE

static int32_t meters_view_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv)
{
  meters_values_collection_t data;
  shell_print(shell, "    |    Type      |   Address  | Energy,kWh | Power,W |  Voltage,V  |    Current,A   | CT");
  shell_print(shell, "----|--------------|------------|------------|---------|-------------|----------------|----");

  int32_t ret = meters_get_all(&data);
  if(ret < 0){
    shell_warn(shell, "collect data error: %d\r\n", ret);
    return 0;
  }

  for(uint32_t i = 0; i < data.count; i++){
    uint8_t addr_str[12] = {0};
    get_meter_address(addr_str, sizeof(addr_str), &data.items[i].parameters);

    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT,
                  " %2u | %-12s | %-10s |", i, meters_get_typename(data.items[i].parameters.type), addr_str);

    if(!data.items[i].is_valid){
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "        --- |     --- |         --- |            ---");
    }
    else{
      shell_values(shell, &data.items[i].values, true);
    }
    shell_print(shell, " | %2u", data.items[i].parameters.current_factor);
  }
  
  shell_print(shell, "");

  return 0;
}

static void get_meter_address(uint8_t * addr_str, uint32_t buffSize, meter_parameters_t *param){
  switch(param->type){
    case meters_type_extern_ac:
      snprintf(addr_str, buffSize, "  %3u", param->address);
      break;
    case meters_type_extern_dc:
      snprintf(addr_str, buffSize, "  %3u", param->address);
      break;
#if CONFIG_STRIM_METERS_BUS485_ENABLE
    case meters_type_CE318:
      snprintf(addr_str, buffSize, " %u", param->address);
      break;
    case meters_type_Mercury234:
      snprintf(addr_str, buffSize, "  %4u", param->address);
      break;
    case meters_type_SPM90:
      snprintf(addr_str, buffSize, "  %3u", param->address);
      break;
#endif
    default: 
      addr_str[0] = '\0';
  }
}

static void shell_values(const struct shell * shell, meters_values_t *values, bool horizontal){
  uint64_t energy_Ws = (values->type ==     meters_current_type_dc) 
                              ? values->DC.energy : values->AC.energy_active;
  uint64_t energy_Wh = energy_Ws / 3600;
  uint32_t energy_kWh_fractional = energy_Wh % 1000;
  uint32_t energy_kWh_integer = energy_Wh / 1000;

  uint32_t power = (values->type == meters_current_type_dc) 
                      ? lroundf(values->DC.power) : lroundf(values->AC.power_active);
  if(horizontal){
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " %6u.%03u |  %6u |",  
                      energy_kWh_integer, energy_kWh_fractional, power);
  }
  else{
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "Energy      : %u.%03u kWh\r\n", energy_kWh_integer, energy_kWh_fractional);
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "Power       : %u W\r\n", power);
  }
      
  if (values->type == meters_current_type_dc){
    if(horizontal){
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, 
                  "       %5ld |         %6.2lf", lroundf(values->DC.voltage), 
                                                (double)values->DC.current);
    }              
    else{
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "voltage     : %ld V\r\n", lroundf(values->DC.voltage));
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "current     : %.2lf A\r\n", (double)values->DC.current);
    }                      
  }
  else{
    uint8_t voltage[16];
    snprintf(voltage, sizeof(voltage), "%3ld/%3ld/%3ld", lroundf(values->AC.voltage[0]), 
                                                      lroundf(values->AC.voltage[1]), 
                                                      lroundf(values->AC.voltage[2]));
    uint8_t current[20];
    snprintf(current, sizeof(current), "%3.1lf/%3.1lf/%3.1lf", (double)values->AC.current[0],
                                                            (double)values->AC.current[1],
                                                            (double)values->AC.current[2]);
    if(horizontal){
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " %11s | %14s", voltage, current);
    }
    else{
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "voltage     : %3ld/%3ld/%3ld V\r\n", 
                    lroundf(values->AC.voltage[0]), lroundf(values->AC.voltage[1]), lroundf(values->AC.voltage[2]));
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "current     : %3.1lf/%3.1lf/%3.1lf A\r\n", 
                    (double)values->AC.current[0], (double)values->AC.current[1], (double)values->AC.current[2]);
    }
  }
}

static int32_t meters_reinit_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv){
  meters_reinit();
  return 0;
}

static int32_t meters_view_single_cmd(const struct shell * shell,
                              size_t argc, uint8_t **argv){
  if(argc != 2){
    shell_warn(shell, "arguments error: correct cmd - meters get <index>\r\n");
    return 0;
  }
  meters_values_collection_t data;
  uint32_t index = atoi(argv[1]);
  
  int32_t ret = meters_get_all(&data);
  if(ret < 0){
    shell_warn(shell, "collect data error: %d\r\n", ret);
    return 0;
  }
  
  if(index > data.count){
    shell_warn(shell, "wrong index\r\n");
    return 0;
  }

  meter_item_info_t *item = &data.items[index];
  uint8_t addr_str[12] = {0};

  get_meter_address(addr_str, sizeof(addr_str), &item->parameters);

  shell_print(shell, "parameter   : value");
  shell_print(shell, "------------:--------------");
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "type        : %s\r\n", meters_get_typename(item->parameters.type));
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "address     : %s\r\n", addr_str);
  if(item->parameters.baudrate == 0){
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "baudrate    : %s\r\n", "----");
  }
  else{
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "baudrate    : %d\r\n", item->parameters.baudrate);
  }
  uint32_t time = k_uptime_get_32();
  time -= item->timemark;
  time /= 100;
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "success req : %d.%d seconds ago\r\n", time/10, time%10);
  if(item->is_valid)
    shell_values(shell, &item->values, false);

  shell_print(shell, "CT          : %2u", item->parameters.current_factor);

  shell_print(shell, "");
  return 0;
}

static int32_t meters_testDC_cmd(const struct shell * shell,
                              size_t argc, uint8_t **argv){
  meters_values_t tmp = {
    .DC.current = 2.000,
    .DC.energy = 35000,
    .DC.power = 77.605,
    .DC.voltage = 120.3,
    .type = meters_current_type_dc
  };

  meters_set_values(0, &tmp);
  return 0;
}

static int32_t meters_testAC_cmd(const struct shell * shell,
                              size_t argc, uint8_t **argv)
{
  meters_values_t tmp = {
    .AC.current[0] = 24.0,
    .AC.current[1] = 39.0,
    .AC.current[2] = 57.0,
    .AC.energy_active = 30500,
    .AC.power_active = 564.605,
    .AC.voltage[0] = 220.4,
    .AC.voltage[1] = 223.1,
    .AC.voltage[2] = 222.2,
    .type = meters_current_type_ac,
  };

  meters_set_values(1, &tmp);
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_meters,
  #if CONFIG_STRIM_METERS_BUS485_ENABLE
    SHELL_CMD(ce318, &sub_ce318,  "Energomera CE318BY", NULL),
    SHELL_CMD_ARG(spm90, NULL,    "spm90 read",         spm90_read_cmd, 2, 1),
  #endif
  SHELL_CMD(testdc, NULL, "test to write data", meters_testDC_cmd),
  SHELL_CMD(testac, NULL, "test to write data", meters_testAC_cmd),
  SHELL_CMD_ARG(get, NULL, "view data for single meter by index", meters_view_single_cmd, 2, 1),
  SHELL_CMD(view, NULL,  "View all data", meters_view_cmd),
  SHELL_CMD(reinit, NULL, "Reinite invoke", meters_reinit_cmd),
  SHELL_SUBCMD_SET_END /* Array terminated */
);

SHELL_CMD_REGISTER(meters, &sub_meters, "Meters commands", NULL);