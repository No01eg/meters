#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include "meters.h"

LOG_MODULE_REGISTER(app);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */



int main(void){
	
	while(1){
		k_msleep(2000);
	}

    return 0;
}