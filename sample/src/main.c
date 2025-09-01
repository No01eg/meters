#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include "meters.h"

LOG_MODULE_REGISTER(app);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

meter_parameters_t meters_parameters[] = {
	{.type = meters_type_externDC, .address = 1, .currentFactor = 1},
	{.type = meters_type_externAC, .address = 3, .currentFactor = 1},
	{.type = meters_type_SPM90,    .address = 2, .baudrate = 9600, .currentFactor = 1},
};

int32_t meters_getParameters(meter_parameters_t *table, uint32_t table_size, void *user_data){
	(void)user_data;
	
	uint32_t count = ARRAY_SIZE(meters_parameters);

	if(count > table_size)
		return -E2BIG;
	memcpy(table, meters_parameters, sizeof(meters_parameters));
	return count;
}

int main(void){
	meters_init(meters_getParameters, NULL);
	while(1){
		k_msleep(2000);
	}

    return 0;
}