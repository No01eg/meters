#include "meters_private.h"

#include <stdio.h>
#include <math.h>

static void get_meter_address(uint8_t * addr_str, uint32_t buffSize, meter_parameters_t *param);
static void shell_values(const struct shell * shell, meters_values_t *values);

static int32_t meters_view_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv)
{
  meters_context_t *context =&metersContext;
  meters_values_t values;

  shell_print(shell, "    |    Type      |   Address  | Energy,kWh | Power,W |  Voltage,V  |    Current,A   | CT");
  shell_print(shell, "----|--------------|------------|------------|---------|-------------|----------------|----");

  for(uint32_t i = 0; i < context->itemCount; i++){
    meter_parameters_t *param = &context->parameters[i];
    
    int32_t ret = meters_get_values(i, &values);

    if((ret != 0) && (ret != -ENXIO)){
      shell_warn(shell, "get values of item %d error: %d\r\n", i, ret);
      return 0;
    }

    uint8_t addr_str[12] = {0};
    get_meter_address(addr_str, sizeof(addr_str), param);

    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT,
                  " %2u | %-12s | %-10s |", i, meters_get_typename(param->type), addr_str);
          
    if(ret == -ENXIO){
      shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, "        --- |     --- |         --- |            ---");
    }
    else{
      shell_values(shell, &values);
    }
    shell_print(shell, " | %2u", param->currentFactor);
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

static void shell_values(const struct shell * shell, meters_values_t *values){
  uint64_t energy_Ws = (values->type ==     meters_currentType_DC) 
                              ? values->DC.energy : values->AC.energyActive;
  uint64_t energy_Wh = energy_Ws / 3600;
  uint32_t energy_kWh_fractional = energy_Wh % 1000;
  uint32_t energy_kWh_integer = energy_Wh / 1000;

  uint32_t power = (values->type == meters_currentType_DC) 
                      ? lroundf(values->DC.power) : lroundf(values->AC.powerActive);

  shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " %6u.%03u |  %6u |",  
                      energy_kWh_integer, energy_kWh_fractional, power);
      
  if (values->type == meters_currentType_DC){
    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, 
                  "       %5ld |         %6.2f", lroundf(values->DC.voltage), 
                                                (double)values->DC.current);
  }
  else{
    uint8_t voltage[16];
    snprintf(voltage, sizeof(voltage), "%ld/%ld/%ld", lroundf(values->AC.voltage[0]), 
                                                      lroundf(values->AC.voltage[1]), 
                                                      lroundf(values->AC.voltage[2]));
    uint8_t current[16];
    snprintf(current, sizeof(current), "%3.1f/%3.1f/%3.1f", (double)values->AC.current[0],
                                                            (double)values->AC.current[1],
                                                            (double)values->AC.current[2]);

    shell_fprintf(shell, SHELL_VT100_COLOR_DEFAULT, " %11s | %14s", voltage, current);
  }
}

static int32_t meters_reinit_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv){
  meters_reinit();
  return 0;
}

static int32_t meters_test_cmd(const struct shell * shell,
                              size_t argc, uint8_t **argv){
  meters_values_t tmp = {
    .DC.current = 2.000,
    .DC.energy = 35.000,
    .DC.power = 77.605,
    .DC.voltage = 120.3
  };

  meters_set_values(0, &tmp);
  return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_meters,
  SHELL_CMD(test, NULL, "test to write data", meters_test_cmd),
  SHELL_CMD(view, NULL,  "View all data", meters_view_cmd),
  SHELL_CMD(reinit, NULL, "Reinite invoke", meters_reinit_cmd),
  SHELL_SUBCMD_SET_END /* Array terminated */
);

SHELL_CMD_REGISTER(meters, &sub_meters, "Meters commands", NULL);