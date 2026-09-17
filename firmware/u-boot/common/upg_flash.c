/*
 * common/upg_flash.c
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


#include "common.h"
//#include "upg_typedef.h"
#include <asm/arch/x_typedef.h>
#include "upg_main.h"
#include "upg_flash.h"
#include "upg_util.h"
#include <asm/arch/nand_operation.h>
#include <linux/mtd/mtd.h>
#include <mt85xx_gpio.h>



//int nand_erase_raw( u_long size, u_long noffset, int partid)

extern BOOL _fgDoFastUpg;

/*-----------------------------------------------------------------------------
 * Name: readDataFromNand
 *
 * Description: Read data from nand flash
 *
 * Inputs:    	dstaddr			Dest address after read data
 *				noffset			Offset from nand flash
 
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Upgrade to nand success
 *				UPGR_FAIL		Upgrade to nand failed
 ----------------------------------------------------------------------------*/
INT32 eraseDataFromNand(UINT32 noffset, UINT32 size, INT32 partid) {
	INT32 u4Ret = 0;

//	extern int nand_erase_raw( u_long size, u_long noffset, int partid);

//	u4Ret = nand_erase_raw(size, noffset, partid);
#ifdef CONFIG_EMMC_BOOT
	u4Ret = mtk_emmc_erase(noffset, size);
#else
	u4Ret = mtk_nand_erase(noffset, size);
#endif
	if (u4Ret != 0) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR] erase nand failed\n");
		return UPGR_FAIL;

	} else {
		return UPGR_OK;
	}
}


/*-----------------------------------------------------------------------------
 * Name: readDataFromNand
 *
 * Description: Read data from nand flash
 *
 * Inputs:    	dstaddr			Dest address after read data
 *				noffset			Offset from nand flash
 
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Upgrade to nand success
 *				UPGR_FAIL		Upgrade to nand failed
 ----------------------------------------------------------------------------*/
INT32 readDataFromNand(UINT32 dstaddr, UINT32 noffset, UINT32 size) {
	INT32 u4Ret = 0;

//	extern int nand_read_raw_skip_bad(u_long dstaddr, u_long noffset, u_long size);

//	u4Ret = nand_read_raw_skip_bad(dstaddr, noffset, size);
#ifdef CONFIG_EMMC_BOOT
	u4Ret = mtk_emmc_read(dstaddr, noffset, size);
#else
	u4Ret = mtk_nand_read(dstaddr, noffset, size);
#endif

	if (u4Ret != 0) {
		UPG_LOG(UPG_LOG_ERROR, "UPG_ERROR read data from nand failed\n");
		return UPGR_FAIL;

	} else {
		return UPGR_OK;
	}
}


/*-----------------------------------------------------------------------------
 * Name: readDataFromNandByPar
 *
 * Description: Read data from nand flash
 *
 * Inputs:    	dstaddr			Dest address after read data
 *				noffset			Offset from nand flash
 
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Upgrade to nand success
 *				UPGR_FAIL		Upgrade to nand failed
 ----------------------------------------------------------------------------*/
INT32 readDataFromNandByPar(UINT32 u4MemStartAddr, UINT32 u4OffsetFromPart, UINT32 u4MemSize, UINT32 u4PartID) {
	INT32 u4Ret = 0;
   INT32 u4PartInfo = 0;
   INT32 u4PartFormat = 0;
   #if UPG_SUPPORT_UBI
   INT32 u4NewPartFormat = 0;
   char _tmp_buf[1024] = "";
   #endif
   #ifndef CONFIG_EMMC_BOOT          
   struct mtd_info* pNeedWriteMtdInfo;
   #endif   
   UINT32 u4WritePartID = u4PartID;
   char mtdbuffer[80] = "";

   
   u4PartInfo = getPartInfo(u4PartID);
   u4PartFormat = GETPARTFORMAT(u4PartInfo);
   u4NewPartFormat = ISUBIVOLUME(u4PartInfo);
   UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c readDataFromNandByPar,u4PartInfo=%d, u4PartFormat=%d\n", u4PartInfo, u4PartFormat);
   
   if (u4PartFormat == PART_FORMAT_RAW) {
    #ifndef CONFIG_EMMC_BOOT           
      sprintf(mtdbuffer, "mtd=(%s)", getPartName(u4PartID));
      pNeedWriteMtdInfo = get_mtd_device_nm(mtdbuffer);
      u4WritePartID = pNeedWriteMtdInfo->index;
    #endif  
	  UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c befor read, i4NFBPartitionWrite >> u4PartID=%d, u4OffsetFromPart=%d, u4MemStartAddr=0x%08x,u4MemSize=%d,mtdbuffer=%s \n", 
      u4WritePartID, u4OffsetFromPart, (UINT32)(u4MemStartAddr), u4MemSize, mtdbuffer);
      u4Ret = i4NFBPartitionRead( u4WritePartID, u4OffsetFromPart, (UINT32)u4MemStartAddr, u4MemSize);
      UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c, i4NFBPartitionRead >> u4PartID=%d, u4OffsetFromPart=%d, u4MemStartAddr=0x%08x,u4MemSize=%d,mtdbuffer=%s \n", 
      u4WritePartID, u4OffsetFromPart, (UINT32)(u4MemStartAddr), u4MemSize, mtdbuffer);
     
   
   } else if (u4PartFormat == PART_FORMAT_YAFFS) {
      
      UPG_LOG(UPG_LOG_ERROR," readDataFromNandByPar ->should not be YAFFS.\n");
	  return UPGR_FAIL;
   }
   #if UPG_SUPPORT_UBI
   if(u4NewPartFormat == 4)
   {    
	 UPG_LOG(UPG_LOG_ERROR," readDataFromNandByPar ->should not be ubi.\n");
	 return UPGR_FAIL;
   }
   #endif
   
   if (!u4Ret) {
     return UPGR_OK;
   } else {
     return UPGR_FAIL;
   }

}



/*-----------------------------------------------------------------------------
 * Name: writeDataToNand
 *
 * Description: Write data to nand flash
 *
 * Inputs:    	u4pImgStartAddr	Final image start address loaded to memory
 *				u4ImgSize		Final image size
 * Outputs: -
 *
 * Returns: 	UPGR_OK			Upgrade to nand success
 *				UPGR_FAIL		Upgrade to nand failed
 ----------------------------------------------------------------------------*/
INT32 writeDataToNand(UINT32 * u4MemStartAddr, UINT32 u4MemSize, UINT32 u4PartID, 
	UINT32 u4OffsetFromPart, UINT32 u4PartOffsetFromFlash, UINT32 u4PartSize) {

	UPG_LOG(UPG_LOG_DEBUG, "[UPG] upg_flash.c writeDataToNand  u4PartID=%d, u4MemStartAddr=0x%x, u4MemSize=ox%x\
		FlashOffset=0x%x, u4PartSize=0x%x\n", u4PartID, u4MemStartAddr, u4MemSize,
		(u4OffsetFromPart + u4PartOffsetFromFlash), u4PartSize);
	UPG_LOG(UPG_LOG_DEBUG, "[UPG] upg_flash.c writeDataToNand eighth bytes is:0x%8x 0x%8x\n", 
		*u4MemStartAddr, *(u4MemStartAddr + 1));

#if 0 //TODO	
	CHAR szCmd[1024];
	INT32 u4Ret = 0;

	printf("[UPG]upg_flash.c writeDataToNand u4MemStartAddr=0x%X, u4MemSize=0x%X, u4PartID=0x%X\
		u4OffsetFromPart=0x%X, u4PartOffsetFromFlash=0x%X\n", 
		u4MemStartAddr, u4MemSize, u4PartID, u4OffsetFromPart, u4PartOffsetFromFlash);
	if ((u4MemSize % NAND_PAGE_SIZE) != 0) //ImageSize alignment TODO
  	{
    	u4MemSize = u4MemSize / NAND_PAGE_SIZE;
    	u4MemSize = (u4MemSize + 1) * NAND_PAGE_SIZE;
  	}

	printf("[UPG]upg_flash.c writeDataToNand aligned u4MemSize=0x%X\n",u4MemSize);
	//write
	u4Ret = sprintf(szCmd, "nand write 0x%X 0x%X 0x%X", 
		u4MemStartAddr, (u4PartOffsetFromFlash + u4OffsetFromPart), u4MemSize);

	if (-1 == u4Ret) {
		printf("[UPG_ERROR]upg_flash.c cat nand write cmd failed\n");	
		return UPGR_FAIL;
	}
	
    u4Ret = run_command(szCmd, 0);
	printf("[UPG]upg_flash.c nand write run_command:%s ret:%d\n", szCmd, u4Ret);
        
	if (-1 == u4Ret) {
		printf("[UPG_ERROR]upg_flash.c call cmd:%s failed\n", szCmd);	
		return UPGR_FAIL;
	}

	//Read back for check
	UINT32 * u4pReadBuf = malloc(u4MemSize);
	if (u4pReadBuf == NULL) {
		printf("[UPG_ERROR]upg_flash.c writeDataToNand malloc(size=0x%X) failed\n", u4MemSize);
		return UPGR_FAIL;
	}
	u4Ret = sprintf(szCmd, "nand read 0x%X 0x%X 0x%X", 
		u4pReadBuf, (u4PartOffsetFromFlash + u4OffsetFromPart), u4MemSize);

	if (-1 == u4Ret) {
		printf("[UPG_ERROR]upg_flash.c cat nand read cmd failed\n");	
		free(u4pReadBuf);
		return UPGR_FAIL;
	}
	
    u4Ret = run_command(szCmd, 0);
	printf("[UPG]upg_flash.c nand read run_command:%s ret:%d\n", szCmd, u4Ret);
        
	if (-1 == u4Ret) {
		printf("[UPG_ERROR]upg_flash.c call cmd:%s failed\n", szCmd);	
		free(u4pReadBuf);
		return UPGR_FAIL;
	}

	printf("[UPG]upg_flash.c Read and compare between Nand Flash Data and Memory\n");	
	if(fgUint32MemCmp(u4MemStartAddr, u4pReadBuf, u4MemSize)==FALSE)
  	{
    	printf("[UPG_ERROR]upg_flash.c Read and compare between Nand Flash Data and Memory failed\n");
		//free(u4pReadBuf); //todo
		//return UPGR_FAIL; //todo
  	}

	free(u4pReadBuf);	
	return UPGR_OK;

#else
   //	extern int nand_upgrade(u_long srcaddr, u_long size, u_long noffset, u_long partsize, u_long *retnand);
   
   INT32 u4Ret = 0;
   u_long retnand;
   INT32 u4PartInfo = 0;
   INT32 u4PartFormat = 0;
   #if UPG_SUPPORT_UBI
   INT32 u4NewPartFormat = 0;
   char _tmp_buf[1024] = "";
   #endif
   #ifndef CONFIG_EMMC_BOOT       
   struct mtd_info* pNeedWriteMtdInfo;
   #endif
   UINT32 u4WritePartID = u4PartID;
   const  UINT32 u4BlockSize = 1024 * 1000;
   char mtdbuffer[80] = "";
   char* puReadBackMem = kmalloc(u4BlockSize+1024, GFP_KERNEL);
   memset(puReadBackMem,0,u4BlockSize+1024);

   
   u4PartInfo = getPartInfo(u4PartID);
   u4PartFormat = GETPARTFORMAT(u4PartInfo);
   #if UPG_SUPPORT_UBI
   u4NewPartFormat = ISUBIVOLUME(u4PartInfo);
   #endif
   UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c u4PartInfo=%d, u4PartFormat=%d\n", u4PartInfo, u4PartFormat);
   
   if (u4PartFormat == PART_FORMAT_RAW) {
      //		extern int nand_write_raw_skip_bad(u_long srcaddr, u_long size, u_long noffset, int partid);
      #if 1
      //		u4Ret = nand_write_raw_skip_bad(u4MemStartAddr, u4MemSize, 
      //			(u4PartOffsetFromFlash + u4OffsetFromPart), u4PartID);
      //u4Ret = mtk_nand_write(u4MemStartAddr, u4MemSize, (u4PartOffsetFromFlash + u4OffsetFromPart));
    #ifndef CONFIG_EMMC_BOOT             
      sprintf(mtdbuffer, "mtd=(%s)", getPartName(u4PartID));
      pNeedWriteMtdInfo = get_mtd_device_nm(mtdbuffer);
      u4WritePartID = pNeedWriteMtdInfo->index;
    #endif  
	  UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c befor write, i4NFBPartitionWrite >> u4PartID=%d, u4OffsetFromPart=%d, u4MemStartAddr=0x%08x,u4MemSize=%d,mtdbuffer=%s \n", 
      u4WritePartID, u4OffsetFromPart, (UINT32)(u4MemStartAddr), u4MemSize, mtdbuffer);

    if(u4MemSize == 16)
    {
      UPG_LOG(UPG_LOG_ERROR, "upg_flash.c before write data 16 bytes is:0x%8x 0x%8x 0x%8x 0x%8x\n",
             *(UINT32 * ) (u4MemStartAddr), *((UINT32 * ) (u4MemStartAddr) + 1),
             *((UINT32 * ) (u4MemStartAddr) + 2),*((UINT32 * ) (u4MemStartAddr) + 3));
    }


      u4Ret = i4NFBPartitionWrite( u4WritePartID, u4OffsetFromPart, (UINT32)u4MemStartAddr, u4MemSize);
	  if (-1 == u4Ret) {
       UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c i4NFBPartitionWrite failed\n");	
       return UPGR_FAIL;
      }	 
      UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_flash.c, i4NFBPartitionWrite >> u4PartID=%d, u4OffsetFromPart=%d, u4MemStartAddr=0x%08x,u4MemSize=%d,mtdbuffer=%s \n", 
      u4WritePartID, u4OffsetFromPart, (UINT32)(u4MemStartAddr), u4MemSize, mtdbuffer);

      if(!_fgDoFastUpg)
      { //read back check       
         UINT32 u4BlockIndex = 0;
         const unsigned char *puStartTmp = (const unsigned char *)u4MemStartAddr;
         
         for ( ; u4BlockIndex < u4MemSize/u4BlockSize; u4BlockIndex++)
         {
            #if SUPPORT_UPG_LED_CUSTOMER_T
            v_set_upg_status(UPG_CHECK, 1000);
            #endif
            u4Ret = i4NFBPartitionRead( u4WritePartID, u4OffsetFromPart + (u4BlockSize*u4BlockIndex), (UINT32)puReadBackMem, u4BlockSize);
            if (-1 == u4Ret) {
              UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c i4NFBPartitionRead failed\n");	
              return UPGR_FAIL;
            }	 

            if(u4MemSize == 16)
            {
              UPG_LOG(UPG_LOG_ERROR, "upg_flash.c read back data 16 bytes is:0x%8x 0x%8x 0x%8x 0x%8x\n",
                     *(UINT32 * ) ((UCHAR *)puReadBackMem), *((UINT32 * ) ((UCHAR *)puReadBackMem) + 1),
                     *((UINT32 * ) ((UCHAR *)puReadBackMem) + 2),*((UINT32 * ) ((UCHAR *)puReadBackMem) + 3));
            }
            
            if(0 != memcmp(puReadBackMem, puStartTmp+u4BlockIndex*u4BlockSize, u4BlockSize))
            {
              UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c write data to nand read back check failed\n");    
              return UPGR_FAIL;
            }
         }
         #if SUPPORT_UPG_LED_CUSTOMER_T
         v_set_upg_status(UPG_CHECK, 1000);
         #endif
         u4Ret = i4NFBPartitionRead( u4WritePartID, u4OffsetFromPart + (u4BlockSize*u4BlockIndex), (UINT32)puReadBackMem, u4MemSize%u4BlockSize);
         if (-1 == u4Ret) {
           UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c i4NFBPartitionRead failed\n");	
           return UPGR_FAIL;
         }	 
         
         if(u4MemSize == 16)
         {
              UPG_LOG(UPG_LOG_ERROR, "upg_flash.c read back data 16 bytes is:0x%8x 0x%8x 0x%8x 0x%8x\n",
                     *(UINT32 * ) ((UCHAR *)puReadBackMem), *((UINT32 * ) ((UCHAR *)puReadBackMem) + 1),
                     *((UINT32 * ) ((UCHAR *)puReadBackMem) + 2),*((UINT32 * ) ((UCHAR *)puReadBackMem) + 3));
         }
                     
         if(0 != memcmp(puReadBackMem, puStartTmp+u4BlockIndex*u4BlockSize, u4MemSize%u4BlockSize))
         {
           UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c write data to nand read back check failed\n");    
           return UPGR_FAIL;
         }
         #if SUPPORT_UPG_LED_CUSTOMER_T
         v_set_upg_status(UPG_CHECK, 1000);
         #endif
      }

      #else
      //		u4Ret = nand_upgrade(u4MemStartAddr, u4MemSize, (u4PartOffsetFromFlash + u4OffsetFromPart),
      //			u4PartSize, &retnand);
      #endif
   
   } else if (u4PartFormat == PART_FORMAT_YAFFS) {
      //		extern int nand_write_yaffs_skip_bad(u_long srcaddr, u_long size, u_long noffset);
      //		u4Ret = nand_write_yaffs_skip_bad(u4MemStartAddr, u4MemSize, 
      //			(u4PartOffsetFromFlash + u4OffsetFromPart));
      printf("[ERROR] should not be YAFFS.\n");
   }
   #if UPG_SUPPORT_UBI
   if(u4NewPartFormat == 4)
   {
     u4Ret = sprintf(_tmp_buf, "ubi write %s 0x%x 0x%x", getPartName(u4PartID), u4MemStartAddr, u4MemSize);		  
     UPG_LOG(UPG_LOG_DEBUG, "[UPG_DEBUG]upg_flash.c create volume %s \n",_tmp_buf);	 
     if (-1 == u4Ret) {
       UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash.c create volume name failed\n");	
       return UPGR_FAIL;
     }	  
     u4Ret = run_command(_tmp_buf, 0);	  
     UPG_LOG(UPG_LOG_DEBUG, "[UPG_DEBUG]upg_flash run_command: cmd = %s, ret:%d\n", _tmp_buf, u4Ret);
     if(-1 == u4Ret)
     {
       UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_flash run_command:write volume failed,command = %s,	ret:%d\n", _tmp_buf, u4Ret);
       return UPGR_FAIL;
     }    
   }
   #endif
   kfree((void*)puReadBackMem);
   if (!u4Ret) {
     return UPGR_OK;
   } else {
     return UPGR_FAIL;
   }
   
#endif
}


