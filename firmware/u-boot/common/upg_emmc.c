/*
 * common/upg_emmc.c
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


#include "common.h"
//#include "upg_typedef.h"
#include <asm/arch/x_typedef.h>
#include "upg_main.h"
#include "upg_flash.h"
#include "upg_util.h"

#include <emmc.h>
#include <mt85xx_gpio.h>

#define ASSERT(e) \
        if(!(e)){ \
          while(1); \
        }



/*-----------------------------------------------------------------------------
 * Name: eraseDataFromEMMC
 *
 * Description: erase data from eMMC card
 *
 * Inputs:    	start_addr			Dest address to erase
 *				size			total size
 
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Upgrade to nand success
 *				UPGR_FAIL		Upgrade to nand failed
 ----------------------------------------------------------------------------*/
INT32 eraseDataFromEMMC(UINT32 start_addr, UINT32 size) 
{
	// Don't need the API
	return UPGR_OK;
}


/*-----------------------------------------------------------------------------
 * Name: readDataFromEMMC
 * Description: Read data from eMMC card
 *
 * Inputs:    	noffset			Offset from eMMC card (BYTE)
 *			size				Total size(Byte) to read	
 *			buffer			dram buffer pointer
 *
 * Returns: 	UPGR_OK			read from eMMC card success
 *			UPGR_FAIL		read from eMMC card failed
 ----------------------------------------------------------------------------*/
INT32 readDataFromEMMC(UINT32 noffset, UINT32 size, void* buffer) 
{
	INT32 i4Ret = 0;
	UINT32 u4MaxTransSize, u4CurAddr, u4TotalLen, u4CuLen;
	UINT8 *ucpCurBuffer, *ucpTempBuffer;
	struct emmc *eMMC=NULL;

	UPG_LOG(UPG_LOG_DEBUG, "[%s] noffset=0x%x, size=0x%x, buffer=0x%x \n", __func__, noffset, size, (UINT32)buffer); 

	eMMC= find_emmc_device(0);
	if(eMMC == NULL)
	{
		printf("[EMMC][%s] Emmc Device == NULL \n", __func__);
		return -1;
	}

	if((UINT32)buffer & 0x03){
		UPG_LOG(UPG_LOG_ERROR, "[%s] Read buffer pointer is not align \n", __func__); 
		
		u4MaxTransSize = 1024*1024; //EMMC_BLOCK_LEN*MSDC_MAX_BLOCK_NUM
		ucpTempBuffer = malloc(u4MaxTransSize);
		ASSERT(ucpTempBuffer != NULL);
		ASSERT(!((UINT32)ucpTempBuffer & 0x03));
				
		u4TotalLen = 0;
		if(u4MaxTransSize < size)
			u4CuLen = u4MaxTransSize;
		else
			u4CuLen = size;
		ucpCurBuffer = (UINT8*)buffer;
		u4CurAddr = noffset;
		while(u4TotalLen < size){	
			UPG_LOG(UPG_LOG_DEBUG, "[%s] u4CuLen=0x%x, u4TotalLen=0x%x\n", __func__, u4CuLen, u4TotalLen); 
											
			i4Ret = emmc_read(eMMC, u4CurAddr, u4CuLen, ucpTempBuffer);
			if(i4Ret){
				free(ucpTempBuffer);
				UPG_LOG(UPG_LOG_ERROR, "[%s] Read fail 1 \n", __func__);
				return i4Ret;
			}
			memcpy(ucpCurBuffer, ucpTempBuffer, u4CuLen);
			
			ucpCurBuffer += u4CuLen;
			u4CurAddr += u4CuLen;
			u4TotalLen += u4CuLen;				
			if((size-u4TotalLen) > u4MaxTransSize)
				u4CuLen = u4MaxTransSize;
			else
				u4CuLen = size-u4TotalLen;				
		}
	
		free(ucpTempBuffer);
	}
	else{
		i4Ret = emmc_read(eMMC, noffset, size, buffer);
	}
	
	UPG_LOG(UPG_LOG_ERROR, "[%s]--- Done !!! ---\n", __func__); 
	return i4Ret;
}


/*-----------------------------------------------------------------------------
 * Name: writeDataToEMMC
 * Description: Write data from eMMC card
 *
 * Inputs:    	noffset			Offset from eMMC card (BYTE)
 *			size				Total size(Byte) to write	
 *			buffer			dram buffer pointer
 *
 * Returns: 	UPGR_OK			write from eMMC card success
 *			UPGR_FAIL		write from eMMC card failed
 ----------------------------------------------------------------------------*/
INT32 writeDataToEMMC(UINT32 noffset, UINT32 size, void* buffer) 
{
	INT32 i4Ret = 0;
	UINT32 u4MaxTransSize, u4CurAddr, u4TotalLen, u4CuLen;
	UINT8 *ucpCurBuffer, *ucpTempBuffer;
	struct emmc *eMMC=NULL;

	UPG_LOG(UPG_LOG_DEBUG, "[%s] noffset=0x%x, size=0x%x, buffer=0x%x \n", __func__, noffset, size, (UINT32)buffer); 
	
	eMMC= find_emmc_device(0);
	if(eMMC == NULL)
	{
		printf("[EMMC][%s] Emmc Device == NULL \n", __func__);
		return -1;
	}	
	if((UINT32)buffer & 0x03){
		UPG_LOG(UPG_LOG_ERROR, "[%s] Read buffer pointer is not align \n", __func__); 
			
		u4MaxTransSize = 1024*1024; //EMMC_BLOCK_LEN*MSDC_MAX_BLOCK_NUM
		ucpTempBuffer = malloc(u4MaxTransSize);
		ASSERT(ucpTempBuffer != NULL);
		ASSERT(!((UINT32)ucpTempBuffer & 0x03));
					
		u4TotalLen = 0;
		if(u4MaxTransSize < size)
			u4CuLen = u4MaxTransSize;
		else
			u4CuLen = size;
		ucpCurBuffer = (UINT8*)buffer;
		u4CurAddr = noffset;
		while(u4TotalLen < size){	
			UPG_LOG(UPG_LOG_DEBUG, "[%s] u4CuLen=0x%x, u4TotalLen=0x%x\n", __func__, u4CuLen, u4TotalLen); 
					
			memcpy(ucpTempBuffer, ucpCurBuffer, u4CuLen);				
			i4Ret = emmc_write(eMMC, u4CurAddr, u4CuLen, ucpTempBuffer);
			if(i4Ret){
				free(ucpTempBuffer);
				UPG_LOG(UPG_LOG_ERROR, "[%s] write fail 1 \n", __func__);
				return i4Ret;
			}
	
			ucpCurBuffer += u4CuLen;
			u4CurAddr += u4CuLen;
			u4TotalLen += u4CuLen;				
			if((size-u4TotalLen) > u4MaxTransSize)
				u4CuLen = u4MaxTransSize;
			else
				u4CuLen = size-u4TotalLen;				
			}
		
		free(ucpTempBuffer);
	}
	else{
		i4Ret = emmc_write(eMMC, noffset, size, buffer);
	}

	UPG_LOG(UPG_LOG_ERROR, "[%s]--- Done !!! ---\n", __func__); 
	return i4Ret;

}


