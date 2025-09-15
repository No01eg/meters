/*
 * Copyright (c) 2020 STRIM ALC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/arch/arm/mpu/arm_mpu.h>

#define REGION_RAM_EXEC_ATTR(size) \
{ \
	(NORMAL_OUTER_INNER_WRITE_BACK_WRITE_READ_ALLOCATE_NON_SHAREABLE | \
	 size | P_RW_U_NA_Msk) \
}

static const struct arm_mpu_region mpu_regions[] = {
	/* Region 1 */
	MPU_REGION_ENTRY("SDRAM_ALL",
			 DT_REG_ADDR(DT_CHOSEN(zephyr_sram)),
			 REGION_RAM_EXEC_ATTR(DT_REG_SIZE(DT_CHOSEN(zephyr_sram)))),
	/* Region 2 */
	MPU_REGION_ENTRY("FLASH_0",
			 DT_REG_ADDR(DT_CHOSEN(zephyr_sram)),
			 REGION_FLASH_ATTR(REGION_8M)),
	/* Region 3 */
	MPU_REGION_ENTRY("FLASH_1",
			 DT_REG_ADDR(DT_CHOSEN(zephyr_sram)) + MB(8),
			 REGION_FLASH_ATTR(REGION_4M)),
};

const struct arm_mpu_config mpu_config = {
	.num_regions = ARRAY_SIZE(mpu_regions),
	.mpu_regions = mpu_regions,
};
