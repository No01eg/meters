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
  return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_meters,
    SHELL_CMD(view, NULL,  "View all data", meters_view_cmd),
    SHELL_CMD(reinit, NULL, "Reinite invoke", meters_reinit_cmd),
    SHELL_SUBCMD_SET_END /* Array terminated */
);

SHELL_CMD_REGISTER(meters, &sub_meters, "Meters commands", NULL);