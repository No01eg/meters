#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include "meters.h"

LOG_MODULE_REGISTER(app);

meter_parameters_t meters_parameters[] = {
	{.type = meters_type_extern_dc, .address = 1, .current_factor = 1},
	{.type = meters_type_extern_ac, .address = 3, .current_factor = 1},
	{.type = meters_type_Mercury234, .address = 47, .baudrate = 9600, .current_factor = 1},
	/*{.type = meters_type_SPM90,    .address = 1, .baudrate = 9600, .current_factor = 1},
	{.type = meters_type_CE318,    .address = 80114997, .baudrate = 4800, .current_factor = 1},*/
};

int main(void){
	LOG_INF("start sample");
	meters_init(meters_parameters, ARRAY_SIZE(meters_parameters));
	while(1){
		k_msleep(2000);
	}

    return 0;
}