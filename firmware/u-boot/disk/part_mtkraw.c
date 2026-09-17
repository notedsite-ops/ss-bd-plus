/*
 * (C) Copyright 2001
 * Raymond Lo, lo@routefree.com
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Support for harddisk partitions.
 *
 * To be compatible with LinuxPPC and Apple we use the standard Apple
 * SCSI disk partitioning scheme. For more information see:
 * http://developer.apple.com/techpubs/mac/Devices/Devices-126.html#MARKER-14-92
 */

#include <common.h>
#include <command.h>
#include <ide.h>
#include "part_mtkraw.h"
#include <asm/arch/mt85xx_part_tbl.h>
#include <mtkfs.h>
#if defined(CONFIG_CMD_IDE) || \
    defined(CONFIG_CMD_MG_DISK) || \
    defined(CONFIG_CMD_SATA) || \
    defined(CONFIG_CMD_SCSI) || \
    defined(CONFIG_CMD_USB) || \
    defined(CONFIG_MMC) || \
    defined(CONFIG_SYSTEMACE)|| \
    defined(CONFIG_EMMC_BOOT)

/* Convert char[4] in little endian format to the host format integer
 */
int test_part_mtkraw (block_dev_desc_t *dev_desc)
{
	unsigned char buffer[DEFAULT_SECTOR_SIZE];

	if (mt85xx_part_tbl_init()) {
		printf("test mtk part error\n");
		return (-1);
	}
	return (0);
}

/*  Print a partition that is relative to its Extended partition table
 */
void print_part_mtkraw(block_dev_desc_t * dev_desc)
{
	return;
}


/*  Print a partition that is relative to its Extended partition table
 */
int get_partition_info_mtkraw (block_dev_desc_t *dev_desc, int part, disk_partition_t * info)
{
	PART_TBL_ITEM *pr_dst_part;
	pr_dst_part = mt85xx_get_part_info_by_id(part);
	info->blksz = dev_desc->blksz;
	strncpy(info->name,pr_dst_part->pc_name,PART_NAME_SIZE);
	info->size = (pr_dst_part->u4_size >> SECTOR_BITS);
	info->start = (pr_dst_part->u4_offset >> SECTOR_BITS);
	sprintf ((char *)info->type, "U-Boot");
	return 0;

}


#endif
