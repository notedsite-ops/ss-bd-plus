/*
 * common/upg_main.h
 *
 *
 * Copyright (C) 2010-2012 MediaTek Inc.
 * $Author: zhifeng.zhao
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * http://www.gnu.org/licenses/old_licenses/gpl-2.0.html
 *
 */


#ifndef _UPG_MAIN_H_
#define _UPG_MAIN_H_

//#include <nand.h>
#include <upg_config.h>
#include <linux/mtd/mt85xx_nand_config.h>
#include <asm/config.h>
#if (CFG_BRANCH_NAME == 1)
#include "pana_metadata.h"
#endif

//#define DEBUG_MODE 0
#define UPG_LOG_ERROR 0
#define UPG_LOG_INFO  1
#define UPG_LOG_DEBUG 2

#define UPGR_OK 0
#define UPGR_FAIL -1

#define UPG_BE_NAME_TAG 	"upg_be_name"
#define UPG_FE_NAME_TAG 	"upg_fe_name"
#define UPG_USB_PATH_TAG 	"upg_usb_path"
#define UPG_USB_FW_PATH_TAG 	"upg_usb_fw_path"

#define UPG_BSM_BE_NAME_TAG 	"upg_bsm_be_name"
#define UPG_BSM_FE_NAME_TAG 	"upg_bsm_fe_name"

#define xstr(s) #s
#define str(s) xstr(s)



#define FE_PART_NAME		"fe_bin_1"
#define FE_BAK_PART_NAME	"fe_bin_2"
#define FE_FA_PART_NAME		"fe_test_data"
#define PANA_REGION_NAME    "region_code"
#define PANA_MODLE_NAME     "model_name_"

#ifndef CONFIG_CURRENT_REGION_CODE
#define CONFIG_CURRENT_REGION_CODE PE
#endif


#define ODD_SOC_FE_FW_CHECKSUM    0x37922226

#define UPG_DYNAMIC_IMG_BUF		0

#define UPG_IMAGE_BUFFER_SIZE	128*1024*1024 //128M
#if (CONFIG_MT85XX_HIGH_PHYS_OFFSET_EN == 1)
#define UPG_IMAGE_BUFFER_ADDR	0x88000000UL //128M
#else
#define UPG_IMAGE_BUFFER_ADDR	0x05000000UL //80M
#endif
#define UPG_LOADER_IMAGE_BUFFER_SIZE	8*1024*1024
#define UPG_MAX_SEARCH_SIZE         64*1024*1024

#define UPG_SUPPORT_UBI 1
#define UPG_SUPPORT_NEW_PIT_FORMAT 1
#if ADAPT_MTD_LINUX
#define UPG_SUPPORT_ADAPTIVE_PIT 1
#define UPG_SUPPORT_BACKUP_NAND_DATA 1
#endif

#if CFG_UPG_NO_SUPPORT_NEW_FE
#define UPG_SUPPORT_NEW_FE_UPG 0
#else
#define UPG_SUPPORT_NEW_FE_UPG 1
#endif 

#define UPG_SUPPORT_BIG_BINARY 0
#if 1//UPG_SUPPORT_BIG_BINARY
#if CFG_UPG_SUPPORT_128M_DRAM //for 128M dram
#define UPG_CHECK_TABLE_SIZE (0x1C00000)
#define UPG_BINARY_BIG_SIZE  (0x1E00000)
#define UPG_MAX_SUB_BINARY_SIZE  (0x1C00000)
#else
#define UPG_CHECK_TABLE_SIZE (0x2000000)
#define UPG_BINARY_BIG_SIZE  (0x8000000)
#define UPG_MAX_SUB_BINARY_SIZE  (0x7000000)
#endif
#endif

#define UPG_USB_NUM			3				//USB Number

#define MAX_PART_NAME_LEN				16	//Max length for Partition Name
#define MAX_PARTITION_INFO_ENTRY_NUM		100
#define MAX_BINARY_INFO_PART_BIN_ENTRY_NUM	10	//Max bin entries number of each partition entry

#define UPG_PART_LIST 		0
#define UPG_PART_WRITE 		1
#define UPG_PART_RELOAD		2


#define BITHEADSIG		0x8530ABCD
#define BITTAILSIG		0x8530EFEF
#define PITHEADSIG		0x8530EADC
#define PITTAILSIG		0xC0CAC01A

#define ADAPPITHEADSIG		0x8530AAAA
#define ADAPPITTAILSIG		0x5441494C


#define PITINDEXHEADSIG		0x50495469
#define PITINDEXTAILSIG		0x69544950

#define XORHEADSIG		0x53526F58
#define XORTAILSIG		0x586F5253

#define REGIONHEADSIG 0x6D657461
#define REGIONTAILSIG 0x6174656d

#define LOADER_HEAD_SIG		0x64616F4C //Load
#define LOADER_TAIL_SIG     0x57467265 //erFW
#define LOADER_FLAG_OFFSET	0xC000		


#define BIT_SEARCH_STEP	16
#define	PIT_SEARCH_STEP	16
#define	PIT_ENTRY_SIZE	32	//Bytes
#define	BIT_ENTRY_SIZE	20	//Bytes


#define INVALID_PART_ID			-1
#define NAND_PAGE_SIZE	2048

//#define NAND_BLOCK_SIZE 	(&nand_info[nand_curr_device]->erasesize)

#define PART_FORMAT_RAW			00
#define PART_FORMAT_YAFFS		01

#define GETPARTFORMAT(partInfo)  ((0x00000006 & partInfo) >> 1) 	//bit[2:1]
#define ISBACKUP(binInfo)  (0x00000001 & binInfo) 	//bit[0]
#define ISPRIMARY(binInfo)  ((0x00000008 & binInfo) >> 3) 	//bit[3]
#define ISUBIVOLUME(binInfo) ((0x0000F000 & binInfo)>> 12) 	//bit[1512]
#define GETMTDID(binInfo) ((0x003F0000 & binInfo)>> 16) 	//bit[21:16]
#define GETVOLTYPE(binInfo) ((0x00C00000 & binInfo)>> 22) 	//bit[23:22]




typedef enum {
	IMG_TYPE_FE = 0,
	IMG_TYPE_BE = 1,
	IMG_TYPE_RAWDATA = 2,
	IMG_TYPE_UNKNOWN = 3,
} img_type_t;

typedef enum {
	UPG_INTERFACE_USB = 0,
	UPG_INTERFACE_TFTP = 1,
	UPG_INTERFACE_MMC = 2,	
	UPG_INTERFACE_UNKNOWN = 3,
} upg_interface_t;

//_PARTITION_INFO_ENTRY mapping to partition_infos.part (such as boot_1) entry
typedef struct _PARTITION_INFO_ENTRY
{
	CHAR		szName[MAX_PART_NAME_LEN];
	INT32		u4PartID;
	INT32		u4PartInfo;					//u4PartInfo[0:0]---Nor flag(1 Nor, 0 Nand)
	UINT32		u4OffsetFromFlash;
	UINT32		u4PartSize;
	
} PARTITION_INFO_ENTRY;


typedef struct _BINARY_INFO_PART_BIN_ENTRY
{
	UINT32		u4PartID;					//Identify whether the bin is active only
	UINT32		u4OffsetFromPart;
	UINT32		u4BinSize;
	INT32		u4OffsetFromFinalImg;		//Trace offset from final image info for each primary image
	INT32 		u4BinInfo;
	
} BINARY_INFO_PART_BIN_ENTRY;

//_BINARY_INFO_PART_ENTRY mapping to binary_infos.part(such as boot_1) entry
typedef struct _BINARY_INFO_PART_ENTRY
{
	UINT32		u4PartID;
	UINT32		u4HasMoreBin;				//0-one bin ,1- more bins
	BINARY_INFO_PART_BIN_ENTRY	partBinEntries[MAX_BINARY_INFO_PART_BIN_ENTRY_NUM];
} BINARY_INFO_PART_ENTRY;


typedef struct _BACKUP_INFO_ENTRY
{
    CHAR		szName[128];
	UINT32		u4PartID;
	UINT32		u4OffsetInBackupBuf;	
	UINT32		u4Length;
} BACKUP_INFO_ENTRY;

/*
typedef struct _BINARY_INFO_ENTRY
{
	INT32		u4PartID;
	INT32		u4OffsetFromFinalImg;		//Offset from the final image
	UINT32		u4OffsetFromFlashPart;		//Offset from the special flash partition 0 address
	UINT32		u4BinSize;
	
} BINARY_INFO_ENTRY;
*/
INT32 writePartBE(upg_interface_t upgInterface, INT32 i4PartID);
INT32 listPITForBE(upg_interface_t upgInterface);
INT32 reloadImage(upg_interface_t upgInterface, img_type_t imageType, INT32* upgBootPart, INT32* upgBackupPart);
INT32 upgBEImage(upg_interface_t upgInterface, INT32 upgBootPart, INT32 upgBackupPart);
INT32 upgFEImage(upg_interface_t upgInterface);
INT32 writeDataToFlash(upg_interface_t upgInterface,UINT8* puFilePath, CHAR* szPartName);
UINT32 getPartInfo(UINT32 u4PartID);
CHAR * getPartName(UINT32 u4PartID);


extern PARTITION_INFO_ENTRY 	partInfoTable[MAX_PARTITION_INFO_ENTRY_NUM];
extern PARTITION_INFO_ENTRY 	partInfoTableFlash[MAX_PARTITION_INFO_ENTRY_NUM]; //PIT has stored to flash
extern BINARY_INFO_PART_ENTRY	binaryInfoTable[MAX_PARTITION_INFO_ENTRY_NUM];

extern INT32 UPG_BOOT_PART;
extern INT32 UPG_BACKUP_PART;
extern INT32 UPG_LOG_LEVEL;
#if UPG_SUPPORT_NEW_PIT_FORMAT
extern UINT32 _u4PitTableVersion;
#endif


#endif

