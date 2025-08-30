#include "meters_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void get_meter_address(uint8_t * addr_str, uint32_t buffSize, meter_parameters_t *param);
static void shell_values(const struct shell * shell, meters_values_t *values, bool horizontal);

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

    if(!data.items[i].isValid){
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "        --- |     --- |         --- |            ---");
    }
    else{
      shell_values(shell, &data.items[i].values, true);
    }
    shell_print(shell, " | %2u", data.items[i].parameters.currentFactor);
  }
  
  shell_print(shell, "");

  return 0;
}

static void get_meter_address(uint8_t * addr_str, uint32_t buffSize, meter_parameters_t *param){
  switch(param->type){
    case meters_type_externAC:
      snprintf(addr_str, buffSize, "  %3u", param->address);
      break;
    case meters_type_externDC:
      snprintf(addr_str, buffSize, "  %3u", param->address);
      break;
#if STRIM_METERS_BUS485_ENABLE
    case meters_type_CE318:
      snprintf(addr_str, buffSize, " %u", param->address);
      break;
    case meters_type_Mercury234:
      snprintf(addr_str, buffSize, "  %4u", param->address);
      break;
    case meters_type_SPM90:
      snprintf(addr_str, buffSize, "  %03u", param->address);
      break;
#endif
    default: 
      addr_str[0] = '\0';
  }
}

static void shell_values(const struct shell * shell, meters_values_t *values, bool horizontal){
  uint64_t energy_Ws = (values->type ==     meters_currentType_DC) 
                              ? values->DC.energy : values->AC.energyActive;
  uint64_t energy_Wh = energy_Ws / 3600;
  uint32_t energy_kWh_fractional = energy_Wh % 1000;
  uint32_t energy_kWh_integer = energy_Wh / 1000;

  uint32_t power = (values->type == meters_currentType_DC) 
                      ? lroundf(values->DC.power) : lroundf(values->AC.powerActive);
  if(horizontal){
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " %6u.%03u |  %6u |",  
                      energy_kWh_integer, energy_kWh_fractional, power);
  }
  else{
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "Energy      : %u.%03u kWh\r\n", energy_kWh_integer, energy_kWh_fractional);
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "Power       : %u W\r\n", power);
  }
      
  if (values->type == meters_currentType_DC){
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

static int32_t meters_viewi_cmd(const struct shell * shell, 
                                size_t argc, uint8_t ** argv){

  meters_values_t values;

  
  int32_t ret = meters_get_values(0, &values);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " current = %.3lf\n", (double)values.DC.current);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " energy = %lld\n", values.DC.energy);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " power = %.3lf\n", (double)values.DC.power);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " voltage = %.3lf\n", (double)values.DC.voltage);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " --------------------\n");
  ret = meters_get_values(1, &values);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " current[0] = %.3lf\n", (double)values.AC.current[0]);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " current[1] = %.3lf\n", (double)values.AC.current[1]);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " current[2] = %.3lf\n", (double)values.AC.current[2]);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " energyActiv = %lld\n", values.AC.energyActive);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " powerActiv = %.3lf\n", (double)values.AC.powerActive);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " voltage[0] = %.3lf\n", (double)values.AC.voltage[0]);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " voltage[1] = %.3lf\n", (double)values.AC.voltage[1]);
  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " voltage[2] = %.3lf\n", (double)values.AC.voltage[2]);
  
  shell_print(shell, "");
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

  meter_itemInfo_t *item = &data.items[index];
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
  if(item->isValid)
    shell_values(shell, &item->values, false);

  shell_print(shell, "CT          : %2u", item->parameters.currentFactor);

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
    .type = meters_currentType_DC
  };

  meters_set_values(0, &tmp);
  return 0;
}

static int32_t meters_testAC_cmd(const struct shell * shell,
                              size_t argc, uint8_t **argv){
  meters_values_t tmp = {
    .AC.current[0] = 24.0,
    .AC.current[1] = 39.0,
    .AC.current[2] = 57.0,
    .AC.energyActive = 30500,
    .AC.powerActive = 564.605,
    .AC.voltage[0] = 220.4,
    .AC.voltage[1] = 223.1,
    .AC.voltage[2] = 222.2,
    .type = meters_currentType_AC
  };

  meters_set_values(1, &tmp);
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_meters,
  SHELL_CMD(testDC, NULL, "test to write data", meters_testDC_cmd),
  SHELL_CMD(testAC, NULL, "test to write data", meters_testAC_cmd),
  SHELL_CMD(get, NULL, "view data for single meter by index", meters_view_single_cmd),
  SHELL_CMD(viewi, NULL, "view test", meters_viewi_cmd),
  SHELL_CMD(view, NULL,  "View all data", meters_view_cmd),
  SHELL_CMD(reinit, NULL, "Reinite invoke", meters_reinit_cmd),
  SHELL_SUBCMD_SET_END /* Array terminated */
);

SHELL_CMD_REGISTER(meters, &sub_meters, "Meters commands", NULL);