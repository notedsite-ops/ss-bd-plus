/*
 * common/mt85xx_bootset.c
 *
 *
 * Copyright (C) 2010-2012 MediaTek Inc.
 * $Author: tiny.chen
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


#include <asm/arch/x_typedef.h>
#include "mt85xx_bootset.h"
#include "upg_flash.h"
#include "upg_usb.h"
#include "upg_util.h"
#include "upg_main.h"
#include "nand.h"
#include <asm/arch/mt85xx_part_tbl.h>
#include <upg_config.h>
#include <asm/arch/mt85xx_upg_status.h>
#include <linux/mtd/mt85xx_part_oper.h>
#include <asm/arch/args_to_uboot.h>
#include <asm/arch/x_bim.h>
#include <asm/u-boot-arm.h>	/* ARM version to be fixed! */
#include <asm/arch/nand_operation.h>
#include <configs/mt8530_base.h>
#ifdef CONFIG_EMMC_BOOT
#include <emmc.h>
#endif

extern uint32_t crc32 (uint32_t crc, const char *buf, unsigned int len);
extern 	ARGS_TO_UBOOT r_args_to_uboot;
extern PARTITION_INFO_ENTRY     partInfoTableFlash[MAX_PARTITION_INFO_ENTRY_NUM]; //PIT has stored to flash

struct {  
	uint32_t crc32;   
	uint32_t isize;
	} __attribute__((__packed__)) gzip;


//globale val , for store re-arranged memory layout
UINT32 g_puNewInitrafsAddr = 0;
UINT32 g_NewInitrafsLen    = 0;
UINT8* puKernelAddr        = 0;
UINT32 u4KernelSize        = 0;
UINT8* puInitrdAddr        = 0;
UINT32 u4InitrdSize        = 0;



#if UPG_SUPPORT_NEW_FE_UPG
CHAR* g_szCurrentWorkUsbPath;
#endif
PARTITION_INFO_ENTRY 	g_partInfoTable[MAX_PARTITION_INFO_ENTRY_NUM];
BINARY_INFO_PART_ENTRY		g_binaryInfoTable[MAX_PARTITION_INFO_ENTRY_NUM];
BOOL g_fgNewPitIndexFormat = FALSE;
UINT32 g_u4BITHeadSigAddr = 0;
UINT32 g_u4BITTailSigAddr = 0;
UINT32 g_u4PITStartAddr = 0;
UINT32 g_u4PITSize = 0;

//for ubi
#define S_IFLNK	 0120000

extern int gunzip (void *dst, int dstlen, unsigned char *src, unsigned long *lenp);

int cpio_trailer(char *dstbuffer)
{
	char s[256];
	const char name[] = "TRAILER!!!";
	UINT32 u4AddedLen = 0;
	unsigned int tmp_ofs;

	sprintf(s, "%s%08X%08X%08lX%08lX%08X%08lX"
	       "%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		0,			/* ino */
		0,			/* mode */
		(long) 0,		/* uid */
		(long) 0,		/* gid */
		1,			/* nlink */
		(long) 0,		/* mtime */
		0,			/* filesize */
		0,			/* major */
		0,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		(unsigned)strlen(name)+1, /* namesize */
		0);			/* chksum */
	memcpy(dstbuffer, s, strlen(s));
	u4AddedLen += 110;
	//push_hdr(s);
	//push_rest(name);
	memcpy(dstbuffer + u4AddedLen, name, strlen(name));
	u4AddedLen += strlen(name);
	*(dstbuffer + u4AddedLen) = 0;
	u4AddedLen++;
	tmp_ofs = strlen(name) + 1 + 110;
	while (tmp_ofs & 3) {
		*(dstbuffer + u4AddedLen) = 0;
		u4AddedLen++;
		tmp_ofs++;
	}

	while (u4AddedLen % 512) {
		*(dstbuffer + u4AddedLen) = 0;
		u4AddedLen++;
	}
	return u4AddedLen;
}

int cpio_mkslink(const char *name, const char *target, unsigned int targetlen,
			 unsigned int mode, uid_t uid, gid_t gid, UINT32 ino, char *dstbuffer)
{
	char s[256];
	time_t mtime = 0;
	UINT32 u4AddedLen = 0;

	sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
	       "%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		ino,			/* ino */
		S_IFLNK | mode,		/* mode */
		(long) uid,		/* uid */
		(long) gid,		/* gid */
		1,			/* nlink */
		(long) mtime,		/* mtime */
		targetlen, /* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		(unsigned)strlen(name) + 1,/* namesize */
		0);			/* chksum */
	u4AddedLen = strlen(s);
	memcpy(dstbuffer, s, u4AddedLen);
	memcpy(dstbuffer + u4AddedLen, name, strlen(name));
	u4AddedLen += strlen(name);
	*(dstbuffer + u4AddedLen) = 0;
	u4AddedLen++;
    while(u4AddedLen & 3)
    {
      *(dstbuffer + u4AddedLen) = 0;
      u4AddedLen++;
    }
    memcpy(dstbuffer + u4AddedLen, target, targetlen);
    u4AddedLen += targetlen;
    while(u4AddedLen & 3)
    {
      *(dstbuffer + u4AddedLen) = 0;
      u4AddedLen++;
    }
	//push_hdr(s);
	//push_string(name);
	//push_pad();
	//push_string(target);
	//push_pad();
	return u4AddedLen;
}


static void vdumpPIT() {
	UINT32 i = 0;
	UPG_LOG(UPG_LOG_DEBUG, "================PIT dump==================\n");
	for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && 
			(g_partInfoTable[i].u4PartID != INVALID_PART_ID); i++) {
		UPG_LOG(UPG_LOG_DEBUG, "g_partInfoTable[%2d].u4PartID  			= %d\n", i, g_partInfoTable[i].u4PartID);	
		UPG_LOG(UPG_LOG_DEBUG, "g_partInfoTable[%2d].szName    			= %s\n", i, g_partInfoTable[i].szName);
		UPG_LOG(UPG_LOG_DEBUG, "g_partInfoTable[%2d].u4OffsetFromFlash	= 0x%X\n", i, g_partInfoTable[i].u4OffsetFromFlash);
		UPG_LOG(UPG_LOG_DEBUG, "g_partInfoTable[%2d].u4PartSize    		= 0x%X\n", i, g_partInfoTable[i].u4PartSize);
		UPG_LOG(UPG_LOG_DEBUG, "g_partInfoTable[%2d].u4PartInfo     		= 0x%X\n\n", i, g_partInfoTable[i].u4PartInfo);
	}
}


static void vdumpBIT() {
	UINT32 i = 0;
	UPG_LOG(UPG_LOG_DEBUG, "================BIT dump==================\n");
	for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && 
			(g_binaryInfoTable[i].u4PartID != INVALID_PART_ID); i++) {
		UPG_LOG(UPG_LOG_DEBUG, "g_binaryInfoTable[%d].u4PartID  		= %d\n", i, g_binaryInfoTable[i].u4PartID);
		UPG_LOG(UPG_LOG_DEBUG, "g_binaryInfoTable[%d].u4HasMoreBin  	= %d\n", i, g_binaryInfoTable[i].u4HasMoreBin);
		int j = 0;
		for (j = 0; (j < MAX_BINARY_INFO_PART_BIN_ENTRY_NUM) 
			&& (g_binaryInfoTable[i].partBinEntries[j].u4PartID != INVALID_PART_ID); j++) {
			UPG_LOG(UPG_LOG_DEBUG, "\tg_binaryInfoTable[%d].partBinEntries[%d].u4PartID  = %d\n", 
				i, j, g_binaryInfoTable[i].partBinEntries[j].u4PartID);	
			UPG_LOG(UPG_LOG_DEBUG, "\tg_binaryInfoTable[%d].partBinEntries[%d].u4OffsetFromPart  = 0x%X\n", 
				i, j, g_binaryInfoTable[i].partBinEntries[j].u4OffsetFromPart);	
			UPG_LOG(UPG_LOG_DEBUG, "\tg_binaryInfoTable[%d].partBinEntries[%d].u4BinSize  = 0x%X\n", 
				i, j, g_binaryInfoTable[i].partBinEntries[j].u4BinSize);			
			UPG_LOG(UPG_LOG_DEBUG, "\tg_binaryInfoTable[%d].partBinEntries[%d].u4OffsetFromFinalImg  = 0x%X\n\n", 
				i, j, g_binaryInfoTable[i].partBinEntries[j].u4OffsetFromFinalImg);
		}
	}
}

void vinitPIT(void) {
	INT32 i = 0;
	for (i = 0; i < MAX_PARTITION_INFO_ENTRY_NUM; i++) {
		g_partInfoTable[i].u4PartID = INVALID_PART_ID;
	}
}
void vinitBIT(void) {
	INT32 i = 0;
	for (i = 0; i < MAX_PARTITION_INFO_ENTRY_NUM; i++) {
		g_binaryInfoTable[i].u4PartID = INVALID_PART_ID;
		g_binaryInfoTable[i].u4HasMoreBin = 0;
		int j = 0;
		for (j = 0; j < MAX_BINARY_INFO_PART_BIN_ENTRY_NUM; j++) {
			g_binaryInfoTable[i].partBinEntries[j].u4PartID = INVALID_PART_ID;
		}
	}
}

CHAR * pgetPartName(UINT32 u4PartID) {
	CHAR * szName = NULL;
	INT32 i = 0;
	for (i = 0; i < MAX_PARTITION_INFO_ENTRY_NUM; i++) {
		if (g_partInfoTable[i].u4PartID == u4PartID) {
			szName = g_partInfoTable[i].szName;
			break;
		}
	}

	return szName;
}

/*-----------------------------------------------------------------------------
 * Name: findPITFromFinalBinaryImage
 *
 * Description: Find partition info table from final binary image
 *
 * Inputs:    	u4pImgStartAddr	Image start address from memory
 *				u4ImgSize		Image size loaded to memory
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Find PIT success
 *				UPGR_FAIL		Find PIT failed
 ----------------------------------------------------------------------------*/

INT32 i4findPITFromFinalBinaryImage(UINT8 *u4pImgStartAddr, UINT32 u4ImgSize) {
     vinitPIT();
     INT32 i = 0;
     UINT32 u4PITHeadSigAddr = 0;
     UINT32 u4PITTailSigAddr = 0;
     BOOL fgFindPITFromBE = FALSE;
     #if UPG_SUPPORT_NEW_PIT_FORMAT
     UINT32 u4PitHeaderLen = 0;
     UINT32 u4PitItemLen = 0;
     UINT32 u4PitItemNum = 0;
     #endif
     UINT32 u4PitIdxItemLen = 16;
	 //INT32 i4Ret = 0;

#if !UPG_SUPPORT_UBI
	for (i = 0; i < u4ImgSize; i += PIT_SEARCH_STEP) {
		if ( (*(UINT32 *) (u4pImgStartAddr + i) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 4) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 8) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 12) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 16) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 20) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 24) == PITHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 28) == PITHEADSIG) ) {			
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]get pit head addr:0x%X\n", (u4pImgStartAddr + i)); 
			u4PITHeadSigAddr = u4pImgStartAddr + i;
			break;
		}
	}


	for (i = 0; i < u4ImgSize; i += PIT_SEARCH_STEP) {
		if ( (*(UINT32 *) (u4pImgStartAddr + i) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 4) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 8) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 12) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 16) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 20) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 24) == PITTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 28) == PITTAILSIG) ) {		
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]get pit tail addr:0x%X\n", (u4pImgStartAddr + i));	
			u4PITTailSigAddr = u4pImgStartAddr + i;
			break;
		}
	}
	UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage u4PITHeadSigAddr=0x%X u4PITTailSigAddr=0x%X\n",
		u4PITHeadSigAddr, u4PITTailSigAddr);

	if (u4PITHeadSigAddr == NULL || u4PITTailSigAddr == NULL) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]findPITFromFinalBinaryImage not found PIT head and tail sig\n");
		return ERR_ELF_FIND_PIT_ERR;
	}
#endif
#if UPG_SUPPORT_UBI
	for (i = 0; i < u4ImgSize; i += PIT_SEARCH_STEP) {
		if ( (*(UINT32 *) (u4pImgStartAddr + i) == PITINDEXHEADSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 4) == PITINDEXHEADSIG)  
			) {	

                 if(*(UINT32 *) (u4pImgStartAddr + i + 8) == 1)
                 {
                    g_fgNewPitIndexFormat = TRUE;
                 }
                 else if((*(UINT32 *) (u4pImgStartAddr + i + 8) == PITINDEXHEADSIG) &&
                         (*(UINT32 *) (u4pImgStartAddr + i + 12) == PITINDEXHEADSIG))
                 {
                    g_fgNewPitIndexFormat = FALSE;
                 }
                 else
                 {
                    continue;
                 }
                 UPG_LOG(UPG_LOG_DEBUG, "[UPG]get pit head addr:0x%X\n", (u4pImgStartAddr + i)); 
                 u4PITHeadSigAddr = u4pImgStartAddr + i;
                 break;
		}
	}


	for (i = 0; i < u4ImgSize; i += PIT_SEARCH_STEP) {
		if ( (*(UINT32 *) (u4pImgStartAddr + i) == PITINDEXTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 4) == PITINDEXTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 8) == PITINDEXTAILSIG) &&
			(*(UINT32 *) (u4pImgStartAddr + i + 12) == PITINDEXTAILSIG) 
			) {		
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]get pit tail addr:0x%X\n", (u4pImgStartAddr + i));	
			u4PITTailSigAddr = u4pImgStartAddr + i;
			break;
		}
	}
	UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage u4PITHeadSigAddr=0x%X u4PITTailSigAddr=0x%X\n",
		u4PITHeadSigAddr, u4PITTailSigAddr);

	if (u4PITHeadSigAddr == NULL || u4PITTailSigAddr == NULL) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]findPITFromFinalBinaryImage not found PIT head and tail sig\n");
		return ERR_ELF_FIND_PIT_ERR;
	}
    //FOR PITIndex 
    u4PITHeadSigAddr += 16;
    if(g_fgNewPitIndexFormat)
       u4PitIdxItemLen = 32;
	
    for (i = 0; ((u4PITHeadSigAddr + i * u4PitIdxItemLen) < u4PITTailSigAddr); i++) {
         UINT32 u4Nansize = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen) );
         UINT32 u4OffsetFromImag = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen)+ 4);
         UINT32 u4TableSize = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen) + 8);
         //UINT32 u4ParTableTagId = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen) + 12);
		 UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_main.c current nand size = 0x%X, PITI u4Nansize = 0x%X \n", nand_info[0].size, u4Nansize);
       #ifdef CONFIG_EMMC_BOOT       
         if(u4Nansize == 0x80000000 )
       #else
         if(u4Nansize == nand_info[0].size )
       #endif     
         {
            UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_main.c findPITFromFinalBinaryImage got 1024 PIT \n");
			if(g_fgNewPitIndexFormat)
            {
               g_u4BITHeadSigAddr = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen) + 16) + u4pImgStartAddr;
               g_u4BITTailSigAddr = *(UINT32 *) (u4PITHeadSigAddr + (i * u4PitIdxItemLen) + 20) + g_u4BITHeadSigAddr - 20;
            }
            u4PITHeadSigAddr = u4pImgStartAddr + u4OffsetFromImag;
            u4PITTailSigAddr = u4PITHeadSigAddr + u4TableSize - 32;
            UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage u4PITHeadSigAddr=0x%X u4PITTailSigAddr=0x%X\n",
            u4PITHeadSigAddr, u4PITTailSigAddr);
            g_u4PITStartAddr = u4PITHeadSigAddr;
            g_u4PITSize      = u4TableSize;
            fgFindPITFromBE = TRUE;
            break;
         }
    
    }
    
    //
    if(!fgFindPITFromBE)
    {
      UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]findPITFromFinalBinaryImage error, Cannot find PIT table from BE which match with nand size =0x%X\n",
              nand_info[0].size);
      return ERR_ELF_FIND_PIT_ERR;
    }
    //
    #if UPG_SUPPORT_NEW_PIT_FORMAT
    _u4PitTableVersion = *((UINT32*)(u4PITHeadSigAddr + 8));
    #endif 
    #if UPG_SUPPORT_NEW_PIT_FORMAT
    if(_u4PitTableVersion == PITHEADSIG)
    {
    #endif
    for (i = 0; i < u4ImgSize; i += PIT_SEARCH_STEP) {
       if ( (*(UINT32 *) (u4PITHeadSigAddr + i) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 4) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 8) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 12) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 16) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 20) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 24) == PITTAILSIG) &&
       (*(UINT32 *) (u4PITHeadSigAddr + i + 28) == PITTAILSIG) ) {		
          UPG_LOG(UPG_LOG_DEBUG, "[UPG]get pit tail addr:0x%X\n", (u4PITHeadSigAddr + i));	
          u4PITTailSigAddr = u4PITHeadSigAddr + i;
          break;
       }
    }
    #if UPG_SUPPORT_NEW_PIT_FORMAT
    }
    #endif
#endif

 //#if !UPG_SUPPORT_ADAPTIVE_PIT
    #if UPG_SUPPORT_NEW_PIT_FORMAT
    if(_u4PitTableVersion == PITHEADSIG)
    {
    #endif
	for (i = 1; ((u4PITHeadSigAddr + i * PIT_ENTRY_SIZE) < u4PITTailSigAddr); i++) {
		CHAR * szName = u4PITHeadSigAddr + (i * PIT_ENTRY_SIZE);
		UINT32 u4PartID = *(UINT32 *) (u4PITHeadSigAddr + (i * PIT_ENTRY_SIZE) + 16);
		UINT32 u4PartInfo = *(UINT32 *) (u4PITHeadSigAddr + (i * PIT_ENTRY_SIZE) + 20);
		UINT32 u4OffsetFromFlash = *(UINT32 *) (u4PITHeadSigAddr + (i * PIT_ENTRY_SIZE) + 24);
		UINT32 u4PartSize = *(UINT32 *) (u4PITHeadSigAddr + (i * PIT_ENTRY_SIZE) + 28);
		UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage[i=%2d,name=%16s,partid=%2d,partinfo=%2d,offset=0x%8X,size=0x%8X\n",
			i, szName, u4PartID, u4PartInfo, u4OffsetFromFlash, u4PartSize);
					
		strncpy(g_partInfoTable[i-1].szName, szName, 16);		
		g_partInfoTable[i-1].u4PartID = u4PartID;
		g_partInfoTable[i-1].u4PartInfo = u4PartInfo;
		g_partInfoTable[i-1].u4OffsetFromFlash = u4OffsetFromFlash;
		g_partInfoTable[i-1].u4PartSize = u4PartSize;
	}
    #if UPG_SUPPORT_NEW_PIT_FORMAT
    }
    else if(_u4PitTableVersion == 1)
    {
       u4PitHeaderLen = *((UINT32*)(u4PITHeadSigAddr + 12));
       u4PitItemLen = *((UINT32*)(u4PITHeadSigAddr + 16));
       u4PitItemNum = *((UINT32*)(u4PITHeadSigAddr + 20));
       for (i = 0; i< u4PitItemNum; i++) {
           CHAR * szName = u4PITHeadSigAddr + u4PitHeaderLen + (i * u4PitItemLen);
           UINT32 u4PartID = *(UINT32 *) (u4PITHeadSigAddr + u4PitHeaderLen + (i * u4PitItemLen) + 16);
           UINT32 u4PartInfo = *(UINT32 *) (u4PITHeadSigAddr + u4PitHeaderLen + (i * u4PitItemLen) + 20);
           UINT32 u4OffsetFromFlash = *(UINT32 *) (u4PITHeadSigAddr + u4PitHeaderLen + (i * u4PitItemLen) + 24);
           UINT32 u4PartSize = *(UINT32 *) (u4PITHeadSigAddr + u4PitHeaderLen + (i * u4PitItemLen) + 28);
           UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage[i=%2d,name=%16s,partid=%2d,partinfo=%2d,offset=0x%8X,size=0x%8X\n",
                     i, szName, u4PartID, u4PartInfo, u4OffsetFromFlash, u4PartSize);
           
           strncpy(g_partInfoTable[i].szName, szName, 16);		
           g_partInfoTable[i].u4PartID = u4PartID;
           g_partInfoTable[i].u4PartInfo = u4PartInfo;
           g_partInfoTable[i].u4OffsetFromFlash = u4OffsetFromFlash;
           g_partInfoTable[i].u4PartSize = u4PartSize;
       }  
    }
    else
    {
        UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]findPITFromFinalBinaryImage error, unknown pit version = 0x%08x \n",
                               _u4PitTableVersion);
        return ERR_ELF_FIND_PIT_ERR; 
    }
    #endif
//#endif


	UPG_LOG(UPG_LOG_DEBUG, "[UPG]findPITFromFinalBinaryImage PIT data dump:\n");
	vdumpPIT();

    #if 0
	for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && 
				(g_partInfoTable[i].u4PartID != INVALID_PART_ID); i++) {
			INT32		u4PartID = g_partInfoTable[i].u4PartID;
			INT32		u4PartInfo = g_partInfoTable[i].u4PartInfo;					//u4PartInfo[0:0]---Nor flag(1 Nor, 0 Nand)
			CHAR *		szPartName = pgetPartName(u4PartID);;
			UINT32		u4OffsetFromFlash = g_partInfoTable[i].u4OffsetFromFlash;
			UINT32		u4PartSize = g_partInfoTable[i].u4PartSize;
			UINT32 u4IsPrimary = ISPRIMARY(u4PartInfo);
			UINT32 u4IsUBIVolume = ISUBIVOLUME(u4PartInfo);
			UINT32 u4IsPIT = strstr(szPartName, "part_info_") ? 1:0;
			UINT32 u4IsUpgStats = strstr(szPartName, "upg_status") ? 1:0;
			UPG_LOG(UPG_LOG_DEBUG, "upgBEImage(search PIT) u4PartID=%d, szPartName=%s, u4OffsetFromFlash=0x%x, u4PartSize=%d, u4IsPrimary=%d, u4IsUBIVolume=%d, u4IsPIT=%d \n",
				   u4PartID, szPartName, u4OffsetFromFlash, u4PartSize, u4IsPrimary, u4IsUBIVolume, u4IsPIT);
			if((u4IsPIT == 1)&&(u4PITHeadSigAddr != 0)&&(g_u4PITSize != 0))
			{
			   UPG_LOG(UPG_LOG_DEBUG, "[UPG]write PIT data to nand partid=%d, u4OffsetFromPart=0x%x, u4PartOffsetFromFlash=0x%x\
					   u4PartSize=0x%x, memstartaddr=0x%x, size=0x%x\n",
					   u4PartID, 0, u4OffsetFromFlash, 
					   u4PartSize, (u4PITHeadSigAddr), g_u4PITSize);
			   UPG_LOG(UPG_LOG_DEBUG, "[UPG] upgPITToNand eighth bytes is:0x%8x 0x%8x\n", 
					   *(UINT32 * ) ((UCHAR *)u4PITHeadSigAddr), 
					   *((UINT32 * ) ((UCHAR *)u4PITHeadSigAddr) + 1));
			   
			   i4Ret = writeDataToNand(((UCHAR *)u4PITHeadSigAddr),
			   g_u4PITSize, u4PartID, 0, u4OffsetFromFlash, u4PartSize);	
			   
			   if (i4Ret == UPGR_FAIL) {
				 UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upgPITToNand 1 write data to nand failed\n");
				 return UPGR_FAIL;
			   } 
			}
			
		}
	#endif

	return ERR_ELF_OK;
}



/*-----------------------------------------------------------------------------
 * Name: findBITFromFinalBinaryImage
 *
 * Description: Find binary info table from final binary image
 *
 * Inputs:    	u4pImgStartAddr	Image start address from memory
 *				u4ImgSize		Image size loaded to memory
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Find BIT success
 *				UPGR_FAIL		Find BIT failed
 ----------------------------------------------------------------------------*/

INT32 i4findBITFromFinalBinaryImage(UINT8 *u4pImgStartAddr, UINT32 u4ImgSize) {
	vinitBIT();
	INT32 i = 0;
	

	INT32 u4PrePartID = INVALID_PART_ID;
	INT32 u4PartIdx = -1;
	INT32 u4BinIdx = -1;
	if(!g_fgNewPitIndexFormat)
    {
    	for (i = 0; i < u4ImgSize; i += BIT_SEARCH_STEP) {
    		if ( (*(UINT32 *) (u4pImgStartAddr + i) == BITHEADSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 4) == BITHEADSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 8) == BITHEADSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 12) == BITHEADSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 16) == BITHEADSIG)) {			
    			UPG_LOG(UPG_LOG_DEBUG, "[UPG]get bit head addr:0x%X\n", (u4pImgStartAddr + i));
    			g_u4BITHeadSigAddr = u4pImgStartAddr + i;
    			break;
    		}
    	}
    
    
    	for (; i < u4ImgSize; i += 4) {
    		if ( (*(UINT32 *) (u4pImgStartAddr + i) == BITTAILSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 4) == BITTAILSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 8) == BITTAILSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 12) == BITTAILSIG) &&
    			(*(UINT32 *) (u4pImgStartAddr + i + 16) == BITTAILSIG)) {			
    			UPG_LOG(UPG_LOG_DEBUG, "[UPG]get bit tail addr:0x%X\n", (u4pImgStartAddr + i));	
    			g_u4BITTailSigAddr = u4pImgStartAddr + i;
    			break;
    		}
    	}
    }
	UPG_LOG(UPG_LOG_DEBUG, "[UPG]findBITFromFinalBinaryImage g_u4BITHeadSigAddr=0x%X g_u4BITTailSigAddr=0x%X\n",
		g_u4BITHeadSigAddr, g_u4BITTailSigAddr);

	if (g_u4BITHeadSigAddr == NULL || g_u4BITTailSigAddr == NULL) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]findBITFromFinalBinaryImage not found BIT head and tail sig\n");
		return ERR_ELF_FIND_BIT_ERR;
	}

	for (i = 1; ((g_u4BITHeadSigAddr + i * BIT_ENTRY_SIZE) < g_u4BITTailSigAddr); i++) {
		UINT32 u4PartID = *(UINT32 *) (g_u4BITHeadSigAddr + (i * BIT_ENTRY_SIZE));
		UINT32 u4OffsetFromFinalImg = *(UINT32 *) (g_u4BITHeadSigAddr + (i * BIT_ENTRY_SIZE) + 4);
		UINT32 u4BinSize = *(UINT32 *) (g_u4BITHeadSigAddr + (i * BIT_ENTRY_SIZE) + 8);;
		UINT32 u4OffsetFromPart = *(UINT32 *) (g_u4BITHeadSigAddr + (i * BIT_ENTRY_SIZE) + 12);
		UINT32 u4BinInfo = *(UINT32 *) (g_u4BITHeadSigAddr + (i * BIT_ENTRY_SIZE) + 16);
				
		UPG_LOG(UPG_LOG_DEBUG, "[UPG]findBITFromFinalBinaryImage[i=%2d,partid=%2d,u4OffsetFromFinalImg=0x%8X,u4BinSize=0x%8X,u4OffsetFromPart=0x%8X\n",
			i, u4PartID, u4OffsetFromFinalImg, u4BinSize, u4OffsetFromPart);

		UPG_LOG(UPG_LOG_DEBUG, "u4PartID=%d, u4PrePartID=%x, u4PartIdx=%d, u4BinIdx=%d\n",
			u4PartID, u4PrePartID, u4PartIdx, u4BinIdx);		
		if (u4PartID != u4PrePartID) {//coming a new partid
			if (u4PartIdx > -1) {
				if (u4BinIdx > 1) {
					g_binaryInfoTable[u4PartIdx].u4HasMoreBin = 1;
				} else {
					g_binaryInfoTable[u4PartIdx].u4HasMoreBin = 0;
				}
			}
			u4PrePartID = u4PartID;
			u4PartIdx++;
			u4BinIdx = 0;//Reset
			g_binaryInfoTable[u4PartIdx].u4PartID = u4PartID;//todo
		}
		if (u4BinIdx > 0) {
           g_binaryInfoTable[u4PartIdx].u4HasMoreBin = 1;
        }
		g_binaryInfoTable[u4PartIdx].partBinEntries[u4BinIdx].u4PartID = u4PartID;
		g_binaryInfoTable[u4PartIdx].partBinEntries[u4BinIdx].u4BinSize = u4BinSize;
		g_binaryInfoTable[u4PartIdx].partBinEntries[u4BinIdx].u4OffsetFromFinalImg = u4OffsetFromFinalImg;
		g_binaryInfoTable[u4PartIdx].partBinEntries[u4BinIdx].u4OffsetFromPart = u4OffsetFromPart;
		g_binaryInfoTable[u4PartIdx].partBinEntries[u4BinIdx++].u4BinInfo = u4BinInfo;
	}

	UPG_LOG(UPG_LOG_DEBUG, "[UPG]findBITFromFinalBinaryImage BIT data dump:\n");
	vdumpBIT();	
	return ERR_ELF_OK;
}





INT32 i4LoadBootBin(UINT8 *puImgStartAddr, UINT32 u4pImgSize)
{
   UINT32 i = 0;
   UINT32 u4Temp = 0;
   UINT32 u4RawSecOffset = 0;
   UINT32 u4RawSecSize = 0;
   //1. start analysis elf file format
   if( *((UINT32*)puImgStartAddr) != ELF_FILE_HEADER)
   {
      UPG_LOG(UPG_LOG_ERROR,"Check elf file header fail, first 4 bytes in file is = 0x%08x! \n",*((UINT32*)puImgStartAddr));
      return ERR_ELF_NOT_ELF_FILE;
   }
   
   
   rELF_FILE_KEY_DATA.u4e_phoff      = *((UINT32*)(puImgStartAddr + 28));
   rELF_FILE_KEY_DATA.u4e_shoff      = *((UINT32*)(puImgStartAddr + 32));
   rELF_FILE_KEY_DATA.u2e_phentsize  = *((UINT16*)(puImgStartAddr + 42));
   rELF_FILE_KEY_DATA.u2e_phnum      = *((UINT16*)(puImgStartAddr + 44));
   rELF_FILE_KEY_DATA.u2e_shentsize  = *((UINT16*)(puImgStartAddr + 46));
   rELF_FILE_KEY_DATA.u2e_shnum      = *((UINT16*)(puImgStartAddr + 48));
   rELF_FILE_KEY_DATA.u2e_shstrindex = *((UINT16*)(puImgStartAddr + 50));
   
   //2. list section header for find raw data section
   for(i = 0; i < rELF_FILE_KEY_DATA.u2e_shnum;i++)
   {
     u4Temp = *((UINT32*)(puImgStartAddr + rELF_FILE_KEY_DATA.u4e_shoff + i*rELF_FILE_KEY_DATA.u2e_shentsize + 28));
     if(u4Temp == 0x10101010)//GOT SECYION INFO VALUE, IF IT EQUALS 0x10101010 MEANS IT IS A RAW DATA SECTION
     {
       u4RawSecOffset = *((UINT32*)(puImgStartAddr + rELF_FILE_KEY_DATA.u4e_shoff + i*rELF_FILE_KEY_DATA.u2e_shentsize + 16));
       u4RawSecSize   = *((UINT32*)(puImgStartAddr + rELF_FILE_KEY_DATA.u4e_shoff + i*rELF_FILE_KEY_DATA.u2e_shentsize + 20));
     }
   }
   //3. check if find the raw data section
   if((0 == u4RawSecOffset)&&(0 == u4RawSecSize)
)
   {
	 UPG_LOG(UPG_LOG_ERROR,"Raw image may be damage, can not find raw data from BE!! \n");
     return ERR_ELF_FIND_RAWSEC_ERR;
   }
   //4.    parsing raw data, and got kernel + initrd bin
   //4.1  got bit 
   if(ERR_ELF_OK != i4findPITFromFinalBinaryImage(puImgStartAddr + u4RawSecOffset, u4RawSecSize))
   {
     UPG_LOG(UPG_LOG_ERROR,"Raw image may be damage, can not find PIT  from BE!! \n");
     return ERR_ELF_FIND_RAWSEC_ERR;
   }
   if(ERR_ELF_OK != i4findBITFromFinalBinaryImage(puImgStartAddr + u4RawSecOffset, u4RawSecSize))
   {
     UPG_LOG(UPG_LOG_ERROR,"Raw image may be damage, can not find BIT  from BE!! \n");
     return ERR_ELF_FIND_RAWSEC_ERR;
   }
   //4.2 got kernel and initrd info
   for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && (g_binaryInfoTable[i].u4PartID != INVALID_PART_ID); i++)
   {
      UINT32 u4PartID	= g_binaryInfoTable[i].u4PartID;
      CHAR * szPartName = pgetPartName(u4PartID);
      UINT32 u4IsKernel = strstr(szPartName, "kernel_") ? 1:0;
      UINT32 u4IsInitrd = strstr(szPartName, "initrd_") ? 1:0;
      UPG_LOG(UPG_LOG_DEBUG, "upgBEImage u4PartID=%d, szPartName=%s\n",u4PartID, szPartName);
      if(u4IsKernel)
      {
         puKernelAddr = puImgStartAddr + (g_binaryInfoTable[i].partBinEntries[0].u4OffsetFromFinalImg + u4RawSecOffset);
         u4KernelSize = g_binaryInfoTable[i].partBinEntries[0].u4BinSize;
      }
      if(u4IsInitrd)
      {
         puInitrdAddr = puImgStartAddr + (g_binaryInfoTable[i].partBinEntries[0].u4OffsetFromFinalImg + u4RawSecOffset);
         u4InitrdSize = g_binaryInfoTable[i].partBinEntries[0].u4BinSize;
      }
   }
   //4.3 check if find kernel and initrd successful
   if((0 == puKernelAddr)&&(0 == u4KernelSize)
)
   {
      UPG_LOG(UPG_LOG_ERROR,"Raw image may be damage, can not find kernel from BE!! \n");
      return ERR_ELF_FIND_KERNEL_ERR;
   }
   if((0 == puInitrdAddr)&&(0 == u4InitrdSize)
)
   {
      UPG_LOG(UPG_LOG_ERROR,"Raw image may be damage, can not find Initrd from BE!! \n");
      return ERR_ELF_FIND_KERNEL_ERR;
   }
   //end of this function 

   return ERR_ELF_OK;
}



/*-----------------------------------------------------------------------------
 * Name: loadImage
 *
 * Description: Load image to memory
 *
 * Inputs:    	upgInterface	Image load interface such as usb, tftp
 *				imageType		Image type such as IMG_TYPE_BE, IMG_TYPE_FE
 *				u4pImgStartAddr	Image start address loaded to memory
 *				u4ImgSize		Image size
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Load image success
 *				UPGR_FAIL		Load image failed
 ----------------------------------------------------------------------------*/
    INT32 i4loadImage(upg_interface_t upgInterface, img_type_t imageType, UINT32 *u4pImgStartAddr, UINT32 *u4pImgSize, UINT32* u4ImageOff ) {
		INT32 i4Ret  = UPGR_OK;
        UINT32 i     = 0;
        *u4ImageOff  = 0;
		if (upgInterface == UPG_INTERFACE_USB) {//Load from USB
			CHAR* szUpgUSBPath;
			CHAR* szUpgImgName;
            #if CFG_UPG_SUPPORT_JIGMODE
            CHAR* szUpgUSBPathFW;
			CHAR* szUpgImgMatchName;
            szUpgUSBPath = getenv(UPG_USB_FW_PATH_TAG);			
			szUpgUSBPathFW = getenv(UPG_USB_PATH_TAG);
            
			#else			
			szUpgUSBPath = getenv(UPG_USB_PATH_TAG);
			#endif
			i4Ret = startUSB();
            if (i4Ret == UPGR_FAIL) {
                UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]start usb fail!! \n");
                return UPGR_FAIL;
            }
            
			if (IMG_TYPE_BE == imageType) {
				#if CFG_UPG_SUPPORT_JIGMODE
                szUpgImgName =  malloc(256 * sizeof(CHAR));
                memset((void*)szUpgImgName, 0, 256);
                if (szUpgImgName == NULL) {
                  UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]find matched file maloc memory failed.\n");
                  return UPGR_FAIL;
                }				
                *szUpgImgName = '\0';
				szUpgImgMatchName = getenv(UPG_BSM_BE_NAME_TAG); 
				i4Ret = usbFindFile(szUpgUSBPath, szUpgImgMatchName, szUpgImgName);
				if (i4Ret == UPGR_FAIL) {
                   UPG_LOG(UPG_LOG_DEBUG, "[UPG_ERROR]cannot find matched file %s in path s% \n",szUpgImgMatchName, szUpgUSBPath);
   				   *szUpgImgName = '\0';                   
   				   szUpgImgMatchName = getenv(UPG_BSM_BE_NAME_TAG); 
   				   i4Ret = usbFindFile(szUpgUSBPathFW, szUpgImgMatchName, szUpgImgName);
				   if (i4Ret == UPGR_FAIL) {	  	
					  UPG_LOG(UPG_LOG_DEBUG, "[UPG_ERROR]cannot find matched file %s in path s% \n",szUpgImgMatchName, szUpgUSBPathFW);
					  return UPGR_FAIL;
				   	}
                }
				#else
                szUpgImgName = getenv(UPG_BE_NAME_TAG);  
				#endif
			} else if (IMG_TYPE_FE == imageType) {
                #if CFG_UPG_SUPPORT_JIGMODE
                szUpgImgName = getenv(UPG_BSM_FE_NAME_TAG);  
                #else
                szUpgImgName = getenv(UPG_FE_NAME_TAG);   
                #endif
			} else {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage invalid image type:%d\n", imageType);
				return UPGR_FAIL;
			}
	
			CHAR* szFullPath = malloc(1024 * sizeof(CHAR));
			if (szFullPath == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage maloc memory failed.\n");
				return UPGR_FAIL;
			}
			*szFullPath = '\0';
			
			szFullPath = catFilePathName(szFullPath, szUpgUSBPath, szUpgImgName);
			if (szFullPath == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage full path is NULL\n");
				return UPGR_FAIL;
			}
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage full path:%s\n", szFullPath);
			
			
			i4Ret = usbGetImage(szFullPath, u4pImgStartAddr, u4pImgSize);
	
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage after usbGetImage u4pImgStartAddr:0x%x, u4pImgSize=0x%x\n",
				u4pImgStartAddr, *u4pImgSize);
			
			if(*u4pImgSize > 0x6400000)//image size > 100M, need print error messaeg
			{
				UPG_LOG(UPG_LOG_ERROR, "[UPG]loadImage after usbGetImage,image size > 100M,  u4pImgSize=0x%x\n",
										*u4pImgSize);				
			}

            #if UPG_SUPPORT_NEW_FE_UPG
            g_szCurrentWorkUsbPath = szUpgUSBPath;
            #endif
            #if CFG_UPG_SUPPORT_JIGMODE
            if(i4Ret == UPGR_FAIL)
            {
               
               UPG_LOG(UPG_LOG_DEBUG, "[UPG_DEBUG] loadImage from USB upgall path failed\n");
               memset((void*)(szFullPath),0,1024 * sizeof(CHAR));
               szFullPath = catFilePathName(szFullPath, szUpgUSBPathFW, szUpgImgName);
               if (szFullPath == NULL) {
                 UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage full path is NULL\n");
                 return UPGR_FAIL;
               }
               UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFWImage full path:%s\n", szFullPath);
                             
               i4Ret = usbGetImage(szFullPath, u4pImgStartAddr, u4pImgSize);
               #if UPG_SUPPORT_NEW_FE_UPG
               g_szCurrentWorkUsbPath = szUpgUSBPathFW;
               #endif
			   
               UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage after from USB upgfw path usbGetImage u4pImgStartAddr:0x%x, u4pImgSize=0x%x\n",
               u4pImgStartAddr, *u4pImgSize);
            }
            #endif
	
			if (i4Ret == UPGR_FAIL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] loadBEImage from USB failed\n");
				free(szFullPath);
				return UPGR_FAIL;
			}
	
			free(szFullPath);
			
		} 
#ifdef CONFIG_MMC 
		else if (upgInterface == UPG_INTERFACE_MMC) {//Load from MMC
			CHAR* szUpgMMCPath;
			CHAR* szUpgImgName;
            #if CFG_UPG_SUPPORT_JIGMODE
            CHAR* szUpgMMCPathFW;
			CHAR* szUpgImgMatchName;
            szUpgMMCPath = getenv(UPG_USB_FW_PATH_TAG);			
			szUpgMMCPathFW = getenv(UPG_USB_PATH_TAG);
            
			#else			
			szUpgMMCPath = getenv(UPG_USB_PATH_TAG);
			#endif
			i4Ret = startMMC();
            if (i4Ret == UPGR_FAIL) {
                UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]start mmc fail!! \n");
                return UPGR_FAIL;
            }
            
			if (IMG_TYPE_BE == imageType) {
				#if CFG_UPG_SUPPORT_JIGMODE
                szUpgImgName =  malloc(256 * sizeof(CHAR));
                memset((void*)szUpgImgName, 0, 256);
                if (szUpgImgName == NULL) {
                  UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]find matched file maloc memory failed.\n");
                  return UPGR_FAIL;
                }				
                *szUpgImgName = '\0';
				szUpgImgMatchName = getenv(UPG_BSM_BE_NAME_TAG); 
				i4Ret = mmcFindFile(szUpgMMCPath, szUpgImgMatchName, szUpgImgName);
				if (i4Ret == UPGR_FAIL) {
                   UPG_LOG(UPG_LOG_DEBUG, "[UPG_ERROR]cannot find matched file %s in path s% \n",szUpgImgMatchName, szUpgUSBPath);
   				   *szUpgImgName = '\0';                   
   				   szUpgImgMatchName = getenv(UPG_BSM_BE_NAME_TAG); 
   				   i4Ret = mmcFindFile(szUpgMMCPathFW, szUpgImgMatchName, szUpgImgName);
				   if (i4Ret == UPGR_FAIL) {	  	
					  UPG_LOG(UPG_LOG_DEBUG, "[UPG_ERROR]cannot find matched file %s in path s% \n",szUpgImgMatchName, szUpgMMCPathFW);
					  return UPGR_FAIL;
				   	}
                }
				#else
                szUpgImgName = getenv(UPG_BE_NAME_TAG);  
				#endif
			} else if (IMG_TYPE_FE == imageType) {
                #if CFG_UPG_SUPPORT_JIGMODE
                szUpgImgName = getenv(UPG_BSM_FE_NAME_TAG);  
                #else
                szUpgImgName = getenv(UPG_FE_NAME_TAG);   
                #endif
			} else {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage invalid image type:%d\n", imageType);
				return UPGR_FAIL;
			}
	
			CHAR* szFullPath = malloc(1024 * sizeof(CHAR));
			if (szFullPath == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage maloc memory failed.\n");
				return UPGR_FAIL;
			}
			*szFullPath = '\0';
			
			szFullPath = catFilePathName(szFullPath, szUpgMMCPath, szUpgImgName);
			if (szFullPath == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage full path is NULL\n");
				return UPGR_FAIL;
			}
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage full path:%s\n", szFullPath);
			
			
			i4Ret = mmcGetImage(szFullPath, u4pImgStartAddr, u4pImgSize);
	
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage after mmcGetImage u4pImgStartAddr:0x%x, u4pImgSize=0x%x\n",
				u4pImgStartAddr, *u4pImgSize);
			
               if(*u4pImgSize > 0x6400000)//image size > 100M, need print error messaeg
               {
				UPG_LOG(UPG_LOG_ERROR, "[UPG]loadImage after mmcGetImage,image size > 100M,  u4pImgSize=0x%x\n",
                                           *u4pImgSize);			   
               }
			   
            #if UPG_SUPPORT_NEW_FE_UPG
            g_szCurrentWorkUsbPath = szUpgMMCPath;
            #endif
            #if CFG_UPG_SUPPORT_JIGMODE
            if(i4Ret == UPGR_FAIL)
            {
              
               UPG_LOG(UPG_LOG_DEBUG, "[UPG_DEBUG] loadImage from MMC upgall path failed\n");
               memset((void*)(szFullPath),0,1024 * sizeof(CHAR));
               szFullPath = catFilePathName(szFullPath, szUpgMMCPathFW, szUpgImgName);
               if (szFullPath == NULL) {
                 UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage full path is NULL\n");
                 return UPGR_FAIL;
               }
               UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFWImage full path:%s\n", szFullPath);
                             
               i4Ret = mmcGetImage(szFullPath, u4pImgStartAddr, u4pImgSize);
               #if UPG_SUPPORT_NEW_FE_UPG
               g_szCurrentWorkUsbPath = szUpgMMCPathFW;
               #endif
			   
               UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadImage after from MMC upgfw path mmcGetImage u4pImgStartAddr:0x%x, u4pImgSize=0x%x\n",
               u4pImgStartAddr, *u4pImgSize);
            }
            #endif
	
			if (i4Ret == UPGR_FAIL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] loadBEImage from MMC failed\n");
				free(szFullPath);
				return UPGR_FAIL;
			}
	
			free(szFullPath);
			
		}
#endif // #ifdef CONFIG_MMC 
		else if (upgInterface == UPG_INTERFACE_TFTP) {//Load from TFTP
			CHAR* szUpgImgName;
			INT32 u4ImageSize = 0;
			
			UPG_LOG(UPG_LOG_ERROR, "[UPG] loadImage from TFTP \n");
			//return UPGR_FAIL;
	
			if (IMG_TYPE_BE == imageType) {
				szUpgImgName = getenv(UPG_BE_NAME_TAG); 
			} else if (IMG_TYPE_FE == imageType) {
				szUpgImgName = getenv(UPG_FE_NAME_TAG); 
			} else {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadImage invalid image type:%d\n", imageType);
			return UPGR_FAIL;
		}
	
			//get file via tftp
			CHAR tftploadfilecmd[1024];
			sprintf(tftploadfilecmd, "tftpboot 0x%X %s", u4pImgStartAddr, szUpgImgName);
			UPG_LOG(UPG_LOG_ERROR, "[UPG]tftploadfilecmd run_command:%s\n", tftploadfilecmd);
			INT32 u4Ret = run_command(tftploadfilecmd, 0);
			UPG_LOG(UPG_LOG_DEBUG, "[UPG]tftploadfilecmd run_command:%s  ret:%d\n", tftploadfilecmd, u4Ret);
			if (-1 == u4Ret) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", tftploadfilecmd);	
				return UPGR_FAIL;
			}
	
			CHAR * tmp;
			UINT32 u4FileSize = 0;
			if ((tmp = getenv ("tftpfilesize")) == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile tftpfilesize==NULL\n");
				return UPGR_FAIL;
			} else {
				u4FileSize = simple_strtoul (tmp, NULL, 16);
				if (u4FileSize <= 0) {
					UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile tftpfilesize==0\n");
					return UPGR_FAIL;	
				} else {
					UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile filesize=0x%X\n", u4FileSize);
					*u4pImgSize = u4FileSize;
					return UPGR_OK;
				}
			}
		}
        else if (upgInterface == UPG_INTERFACE_NAND) 
        { 
          #ifdef CONFIG_EMMC_BOOT
           CHAR tNandloadfilecmd[1024];
           INT32		u4PartID;
           
           initPITFlash();
           
           i4Ret = findPITFromNand(partInfoTableFlash);
           if (i4Ret != UPGR_OK) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] Find PIT from nand failed, pls upgrade Backend image firtly\n");    
               return UPGR_FAIL;
           }

           for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && 
                (partInfoTableFlash[i].u4PartID != INVALID_PART_ID); i++) 
           {
              if( ISUBIVOLUME(partInfoTableFlash[i].u4PartInfo) == 3 )
              {
                 u4PartID = partInfoTableFlash[i].u4PartID;
                 break;
              }
           }
           
           sprintf(tNandloadfilecmd, "ext4load emmc 0:%x 0x%X /%s", u4PartID,u4pImgStartAddr, "upgbinary.bin");
           INT32 i4Ret = run_command(tNandloadfilecmd, 0);
           UPG_LOG(UPG_LOG_ERROR, "[UPG]tNandloadfilecmd run_command:%s  ret:%d\n", tNandloadfilecmd, i4Ret);
           if (i4Ret < 0) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", tNandloadfilecmd);   
               return UPGR_FAIL;
           }
          #else           
           CHAR tNandloadfilecmd[1024];
          
           initPITFlash();
           
           i4Ret = findPITFromNand(partInfoTableFlash);
           if (i4Ret != UPGR_OK) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] Find PIT from nand failed, pls upgrade Backend image firtly\n");    
               return UPGR_FAIL;
           }
 
           i4Ret =  i4SetUBIMountEnv(partInfoTableFlash);
           if(-1 == i4Ret)
           {
             UPG_LOG(UPG_LOG_DEBUG, "[UPG_ERROR]set partition info env fail,ret:%d\n", i4Ret);
             return UPGR_FAIL;
           }   
           
           for (i = 0; (i < MAX_PARTITION_INFO_ENTRY_NUM) && 
                (partInfoTableFlash[i].u4PartID != INVALID_PART_ID); i++) 
           {
              if( ISUBIVOLUME(partInfoTableFlash[i].u4PartInfo) != 4 )
              {
                if(mt85xx_init_mtd_table(partInfoTableFlash[i].szName) != 0)	
                {
                  UPG_LOG(UPG_LOG_ERROR, "[mtd_table init]%s not be added!\n", partInfoTableFlash[i].szName);	
                  return UPGR_FAIL;
                }
              }
           } 

           sprintf(tNandloadfilecmd, "ubi part ubi0");   
           i4Ret = run_command(tNandloadfilecmd, 0);
           UPG_LOG(UPG_LOG_ERROR, "[UPG]tNandloadfilecmd run_command:%s  ret:%d\n", tNandloadfilecmd, i4Ret);
           if (i4Ret < 0) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", tNandloadfilecmd);   
               return UPGR_FAIL;
           }
 
           sprintf(tNandloadfilecmd, "ubifsmount ubi_boot");   
           i4Ret = run_command(tNandloadfilecmd, 0);
           UPG_LOG(UPG_LOG_ERROR, "[UPG]tNandloadfilecmd run_command:%s  ret:%d\n", tNandloadfilecmd, i4Ret);
           if (i4Ret < 0) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", tNandloadfilecmd);   
               return UPGR_FAIL;
           }

           sprintf(tNandloadfilecmd, "ubifsload 0x%X /%s", u4pImgStartAddr, "upgbinary.bin");
           i4Ret = run_command(tNandloadfilecmd, 0);
           UPG_LOG(UPG_LOG_ERROR, "[UPG]tNandloadfilecmd run_command:%s  ret:%d\n", tNandloadfilecmd, i4Ret);
           if (i4Ret < 0) {
               UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", tNandloadfilecmd);   
               return UPGR_FAIL;
           }           
          #endif
			CHAR * tmp;
			UINT32 u4FileSize = 0;
			if ((tmp = getenv ("filesize")) == NULL) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==NULL\n");
				return UPGR_FAIL;
			} else {
				u4FileSize = simple_strtoul (tmp, NULL, 16);
				if (u4FileSize <= 0) {
					UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==0\n");
					return UPGR_FAIL;	
				} else {
					UPG_LOG(UPG_LOG_ERROR, "[UPG]loadFile filesize=0x%X\n", u4FileSize);
					*u4pImgSize = u4FileSize;
					return UPGR_OK;
				}
			}           
        }
	
		return i4Ret;
	}

INT32 i4AddImageToInitrd(UINT8* puNewInitrdAdd, UINT32 u4NewInirdSize, 
                            UINT8* puOldInitrdAdd,  UINT32 u4OldInirdSize,
                            UINT8* puImageAdd,      UINT32 u4ImageSize)
{
   INT32 i              = 0;
   UINT32 image_len     = u4OldInirdSize;
   UINT32 u4FoundNum    = 0;
   UINT32 u4LtCpioImIdx = 0;
   UINT32 u4NewCpioImLn = 0;
   UINT32 u4FinalCpioLn = 0;
   UINT8  gzipHead[10] = {0x1f,0x8b,0x08,0x08,0x8b,0x06,0x6b,0x4d,0x00,0x03};
   
   //1.1 gunzip the old initrd to newinitrd add
   if (gunzip ((void *)puNewInitrdAdd, u4NewInirdSize,(unsigned char *)puOldInitrdAdd, &image_len) != 0)
   {
       UPG_LOG(UPG_LOG_ERROR,"GUNZIP: uncompress, out-of-mem or overwrite error - must RESET board to recover\n");
       return ERR_ELF_UBZIP_INITRD_ERR;
   }
   //1.2 check the unzip result , if unzip size larger then u4NewInirdSize
   if(image_len > u4NewInirdSize)
   {
       UPG_LOG(UPG_LOG_ERROR,"unzip initrd fail, unzip size larger then available output memory size! \n");
       return ERR_ELF_UBZIP_OVERMEM_ERR;
   }
   //2. merge final image to initrd
   //2.1 search cpio_trailer
   for(i = image_len - 6; i > -1; i-- )
   {
      if( (*(puNewInitrdAdd + i    ) == 0x30) && 
          (*(puNewInitrdAdd + i + 1) == 0x37) && 
          (*(puNewInitrdAdd + i + 2) == 0x30) && 
          (*(puNewInitrdAdd + i + 3) == 0x37) && 
          (*(puNewInitrdAdd + i + 4) == 0x30) && 
          (*(puNewInitrdAdd + i + 5) == 0x31)    )
      {
        
          if(2 == (++u4FoundNum))
          {
      	  	 break; 
          }
          if(1 == u4FoundNum)
          {
             u4LtCpioImIdx = i;
          }
      }
   }
   if(u4FoundNum != 2)
   {
      UPG_LOG(UPG_LOG_ERROR,"find CPIO HEADER in ubziped initrd file fail \n");
      return ERR_FIND_CPIO_HEAD_ERR;
   }
   //2.2 GOT CPIO TRAILER, insert main image befor TRAILER
   //2.2.1 first check if unziped intrd size + main image size larger then available memory size(u4NewInirdSize)
   if(image_len + u4ImageSize > u4NewInirdSize)
   {
      UPG_LOG(UPG_LOG_ERROR,"no enought buffer for store main image and initrd! \n");
      return ERR_NEWIRD_SZEXCEED;
   }
   //2.2.2 add main image as a CPIO item
   u4NewCpioImLn = cpio_mkslink("main image", puImageAdd, u4ImageSize, 0x38314134, 0x323137, 0x374430, 0x5544, puNewInitrdAdd + i);

   //2.2.3 restore add trailer
   u4FinalCpioLn = cpio_trailer(puNewInitrdAdd + i + u4NewCpioImLn);
   u4FinalCpioLn += u4NewCpioImLn + i;
   
   if(u4FinalCpioLn > u4NewInirdSize)
   {
     UPG_LOG(UPG_LOG_ERROR,"new cpio exceed vaild buffer ! \n");
     return ERR_NEWIRD_SZEXCEED;
   }
   #if 0
   //3.0 end of new cpio file make, need gunzip this file -> no support now
   //3.1 add gzip head(10 bytes,)
   memcpy(puImageAdd, gzipHead, 10);
   //3.2 add name
   memcpy(puImageAdd + 10, "initrd.img", 10);
   *(puImageAdd + 20) = 0;
   //3.3 deflate the new initrd cpio file
  
   //3.4 add crc and isize, total 8 bytes 
   #endif

   return u4FinalCpioLn;
 
}


//below function used for generate a cpio+gzip init file
void output(const char *buf, size_t len)
{
    //fwrite(buf, len, 1, stdout);
    memcpy((void*)(g_puNewInitrafsAddr + g_NewInitrafsLen), buf, len);
    g_NewInitrafsLen += len;
}

size_t gzip_header(void)
{
    const char header[10] = { 0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03 };
    output(header, sizeof(header));
    return sizeof(header);
}

size_t gzip_data(const char *buf, size_t len, BOOL last)
{
    size_t ret = 0;
    gzip.crc32 = crc32(gzip.crc32, buf, len);
    gzip.isize += len;
    while (len)
    {
        size_t bsize = len < 32768 ? len : 32768;
        char b[5] = { last ? 0x01 : 0x00, bsize, bsize >> 8, ~bsize, ~bsize >> 8 };
        output(b, sizeof(b));
        output(buf, bsize);
        buf += bsize;
        len -= bsize;
        ret += sizeof(b) + bsize;
    }
    return ret;
}

size_t gzip_trailer(void)
{
    output((char *)&gzip, sizeof(gzip));
    return sizeof(gzip);
}

size_t cpio_header(unsigned long ino, unsigned short mode, const char *path, size_t datalen)
{
    char header[128] = {0};
    size_t pathlen = strlen(path) + 1;
    size_t len = sprintf(header,
        "%06x%08x%08x%08x%08x%08x%08x"
        "%08x%08x%08x%08x%08x%08x%08x%s%c",
        0x070701,
        ino, // s.st_ino
        mode, // s.st_mode
        0, // s.st_uid
        0, // s.st_gid
        1, // s.st_nlink
        0, // s.st_mtime
        datalen,
        0, // volmajor
        0, // volminor
        0, // devmajor
        0, // devminor
        pathlen,
        0,
        path,
        0);

    len = (len + 3) & ~3;
    return gzip_data(header, len, FALSE);
}

size_t cpio_data(const char *data, size_t datalen)
{
    const char padding[3] = {0};
    size_t len = (datalen + 3) & ~3;
    size_t ret = gzip_data(data, datalen, FALSE);
    if (len - datalen)
        ret += gzip_data(padding, len - datalen, FALSE);
    return ret;
}

size_t cpio_trailer_gzip(void)
{
    const char trailer[124] = "07070100000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000B00000000TRAILER!!!";
    return gzip_data(trailer, sizeof(trailer), TRUE);
}


void gen_init(char *data, size_t datalen)
{
    gzip_header();
    cpio_header(1, 0100755, "init", datalen);
    cpio_data(data, datalen);
    cpio_trailer_gzip();
    gzip_trailer();
}

//re-arrange the kernel and initrd address and data

UINT32 rearrange_ker_boots(UINT32 u4UpgFlag)
{
  UINT32 u4ImageSz              = 0;
  UINT32 u4FreeMemStAdd         = _bss_end + 0xE00000;//binary max size = 169m .if total memory = 256m
  upg_interface_t upgInterface  = u4UpgFlag;
  UINT32 itemp                  = 0;
  UINT32 u4ImageOff             = 0;
  INT32 i4Ret = 0;

  if (r_args_to_uboot.u4_dram_size_ch1 + r_args_to_uboot.u4_dram_size_ch2 > 0x20000000)
    u4FreeMemStAdd         = 0x10000000;

  UPG_LOG(UPG_LOG_INFO, "rearrange_ker_boots u4UpgFlag: %d, u4FreeMemStAdd: 0x%x\n",u4UpgFlag,u4FreeMemStAdd);

  r_args_to_uboot.u4_boot_type = 2;
	  
  i4loadImage(upgInterface, IMG_TYPE_BE, (UINT32*)(u4FreeMemStAdd), &u4ImageSz, &u4ImageOff);
  u4FreeMemStAdd += u4ImageOff;

  i4Ret = CheckXorValueFromFinalBinaryImage((UINT8 *)(u4FreeMemStAdd), u4ImageSz);
  if(i4Ret != 0)
  {
      UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] Upgrade BE image CheckXorValueFromFinalBinaryImage failed\n");
      while(1);  
  }
  
  i4LoadBootBin((UINT8*)(u4FreeMemStAdd), u4ImageSz);

  //move initrd and kernel to the end of memory
  
  //r_args_to_uboot.u4_initrd_addr = BIM_READ32(REG_RW_GPRDW2) - 0x400000;
  r_args_to_uboot.u4_kernel_addr = _bss_end;
  //r_args_to_uboot.u4_initrd_size = u4InitrdSize;
  //memcpy((void*)(r_args_to_uboot.u4_initrd_addr), puInitrdAddr, u4InitrdSize);
  memcpy((void*)(r_args_to_uboot.u4_kernel_addr), puKernelAddr, u4KernelSize);
  UPG_LOG(UPG_LOG_INFO, "kernel size: 0x%x\n", u4KernelSize);
  
    //make cpio+gpio format init
    if (r_args_to_uboot.u4_dram_size_ch1 + r_args_to_uboot.u4_dram_size_ch2 > 0x20000000)
    g_puNewInitrafsAddr   = 0xE000000;
    else
    {
        #if CONFIG_UPG_BIG_INITRD
        /* 
         * For td128wfd.
         * Cuz initrd size is too big, address moves forward 1M, 
         * kernel image size reduces to 3M.
         */
        g_puNewInitrafsAddr   = _bss_end + 0x300000;
        #else
        g_puNewInitrafsAddr   = _bss_end + 0x400000;  
        #endif
    }
  
  g_NewInitrafsLen      = 0;
  
  gzip_header();
  cpio_header(1, 0100755, "init", u4ImageSz);
  itemp  = 0;
  while(itemp < (u4ImageSz/32768))
  {
     cpio_data((char*)(u4FreeMemStAdd + (itemp*32768)) , 32768);
	 itemp++;
  }
  if( (u4ImageSz - itemp*32768) > 0)
  {
     cpio_data((char*)(u4FreeMemStAdd + (itemp*32768)), u4ImageSz - itemp*32768);
  }
  cpio_trailer_gzip();
  gzip_trailer();
  
  r_args_to_uboot.u4_initrd_addr = g_puNewInitrafsAddr;
  r_args_to_uboot.u4_initrd_size = g_NewInitrafsLen;

  UPG_LOG(UPG_LOG_INFO,"r_args_to_uboot:\n");
  UPG_LOG(UPG_LOG_INFO,"\thead sig\t: 0x%08x\n", r_args_to_uboot.u4_head_sig);
  UPG_LOG(UPG_LOG_INFO,"\tversion\t: %d\n", r_args_to_uboot.u4_version);
  UPG_LOG(UPG_LOG_INFO,"\tboot type\t: %d\n", r_args_to_uboot.u4_boot_type);
  UPG_LOG(UPG_LOG_INFO,"\tdram ch1\t: 0x%08x\n", r_args_to_uboot.u4_dram_size_ch1);
  UPG_LOG(UPG_LOG_INFO,"\tdram ch2\t: 0x%08x\n", r_args_to_uboot.u4_dram_size_ch2);
  UPG_LOG(UPG_LOG_INFO,"\tkern addr\t: 0x%08x\n", r_args_to_uboot.u4_kernel_addr);
  UPG_LOG(UPG_LOG_INFO,"\tinitrd addr\t: 0x%08x\n", r_args_to_uboot.u4_initrd_addr);
  UPG_LOG(UPG_LOG_INFO,"\tinitrd size\t: 0x%08x\n", r_args_to_uboot.u4_initrd_size);
  UPG_LOG(UPG_LOG_INFO,"\tu4_part_tbl_addr\t: 0x%08x\n", r_args_to_uboot.u4_part_tbl_addr);
  UPG_LOG(UPG_LOG_INFO,"\tu4_upg_status_addr\t: 0x%08x\n", r_args_to_uboot.u4_upg_status_addr);

  return 1;
  
}
