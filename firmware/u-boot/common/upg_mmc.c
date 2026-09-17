/*
 * common/upg_mmc.c
 *
 *
 * Copyright (C) 2010-2012 MediaTek Inc.
 * $Author: bing.hong
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


#include <common.h>
//#include "upg_typedef.h"
#include <asm/arch/x_typedef.h>
#include "upg_main.h"
#include "upg_mmc.h"
#include "upg_util.h"
#include "fat.h"

//#ifdef CONFIG_MTK_SD
extern int mmc_card_exist(void);
//#endif

INT32 MMC_card_exist(void) 
{
    char * detectmmc = "mmc insert";
    INT32 u4Ret = run_command(detectmmc, 0);

    return u4Ret;
}

INT32 startMMC(void) 
{
    char * startmmc = "mmc rescan 0";
    INT32 u4Ret = run_command(startmmc, 0);
//#ifdef CONFIG_MTK_SD
    if (!mmc_card_exist())
    {
        printf("[UPG] SD/SDHC/MMC Card not inserted !!\n");
        return UPGR_FAIL;
    }
//#endif   
    UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_mmc.c call cmd: mmc start\n");
    UPG_LOG(UPG_LOG_DEBUG, "[UPG]startMMC run_command:%s  ret:%d\n", startmmc, u4Ret);
    
    if (-1 == u4Ret) 
    {
        UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", startmmc);	
        return UPGR_FAIL;
    }
    return UPGR_OK;
}

INT32 mmcLoadFile(CHAR * szFullPath, UINT32 *u4ImgStartAddr, UINT32 *u4ImgSize) 
{
	CHAR szCmd[1024];
	INT32 u4Ret = 0;
	UINT32 i = 0;
//#ifdef CONFIG_MTK_SD
    if (!mmc_card_exist())
    {
        printf("[UPG] SD/SDHC/MMC Card not inserted !!\n");
        return UPGR_FAIL;
    }
//#endif    
    u4Ret = sprintf(szCmd, "fatload mmc %d 0x%X %s", i, (UINT32)u4ImgStartAddr, szFullPath);
    if (-1 == u4Ret) 
    {
    	UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_mmc.c loadFile failed\n");	
    	return UPGR_FAIL;
    }

    u4Ret = run_command(szCmd, 0);
    UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile run_command:%s ret:%d\n", szCmd, u4Ret);

	if (u4Ret == -1) 
	{
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile failed\n");	
		return UPGR_FAIL;
	}
	else
	{
		//Get loaded file info
		CHAR * tmp;
		UINT32 u4FileSize = 0;
		
		if ((tmp = getenv ("filesize")) == NULL) 
		{
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==0\n");
			return UPGR_FAIL;
		}
		else 
		{
			u4FileSize = simple_strtoul (tmp, NULL, 16);
			if (u4FileSize <= 0) 
			{
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==0\n");
				return UPGR_FAIL;	
			}
			else 
			{
				UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile filesize=0x%X\n", u4FileSize);
				*u4ImgSize = u4FileSize;
				return UPGR_OK;
			}
		}				
	}
}

/*-----------------------------------------------------------------------------
 * Name: mmcGetImage
 *
 * Description: Get  image (such as DVD.bin, 8530_linux_demo_dbg.bin) from USB
 *
 * Inputs:    	None
 * Outputs: -	u4ImgSize		Image size
 *				u4ImgStartAddr	Image start address from memory
 *
 * Returns: 	UPGR_OK			Get image success
 *				UPGR_FAIL		Get image failed
 ----------------------------------------------------------------------------*/
INT32 mmcGetImage(CHAR *szFullPath, UINT32 *u4pImgStartAddr, UINT32 *u4pImgSize) {
	INT32 i4Ret = UPGR_FAIL;
	/*i4Ret = startUSB();
	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}*/

	i4Ret = mmcLoadFile(szFullPath, u4pImgStartAddr, u4pImgSize);

	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}
	
	return i4Ret;
}

/*-----------------------------------------------------------------------------
 * Name: mmcFindFile
 *
 * Description: find file by name(such as DVD.bin, 8530_linux_demo_dbg.bin) from MMC/directory
 *
 * Inputs:    	None
 * Outputs: -	szDirectory		Which directory to search 
 *				szFileName	    The searched file name
 *
 * Returns: 	UPGR_OK			Find the matched file success
 *				UPGR_FAIL		Find the matched file failed
 ----------------------------------------------------------------------------*/
INT32 mmcFindFile(CHAR *szDirectory, CHAR *szFileName, CHAR *szMatchedfilename) {
    INT32 i4Ret = UPGR_FAIL;
    CHAR* szFullDirectory = (CHAR*)malloc(256 * sizeof(CHAR));
    CHAR* szPart = (CHAR*)malloc(256 * sizeof(CHAR));
    UINT32 i = 0;
    
    memset((void*)szPart, 0, 256);
    memset((void*)szFullDirectory, 0, 256);
    strcat(szFullDirectory, "/");
    strcat(szFullDirectory, szDirectory);

    sprintf(szPart,"%d",i);
    i4Ret = x_match_file_from_dir("mmc", szPart,szFullDirectory, szFileName, szMatchedfilename);

    if (i4Ret == UPGR_FAIL) {
        UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]cannot find file %s from mmc \n",szFileName);	
        return UPGR_FAIL;
    }
    
    free(szFullDirectory);
    free(szPart);
    return i4Ret;
}

INT32 debugMMC(void) 
{
    char * debugmmc = "mmc debug";
    INT32 u4Ret = run_command(debugmmc, 0);

    return u4Ret;
}
