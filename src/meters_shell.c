#include "meters_private.h"

#include <stdio.h>
#include <math.h>

static int32_t meters_view_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv)
{

  return 0;
}

static int32_t meters_reinit_cmd(const struct shell * shell, 
                                 size_t argc, uint8_t ** argv)
{
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