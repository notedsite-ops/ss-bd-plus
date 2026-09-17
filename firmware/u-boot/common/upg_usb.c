/*
 * common/upg_usb.c
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

#include <common.h>
//#include "upg_typedef.h"
#include <asm/arch/x_typedef.h>
#include "upg_main.h"
#include "upg_usb.h"
#include "upg_util.h"
#include "fat.h"

UINT32 UsbNum = 0;
CHAR FullPath[1024] = {0};

INT32 startUSB() {
	UPG_LOG(UPG_LOG_DEBUG, "[UPG]upg_usb.c call cmd: usb start\n");
    char * startusb = "usb start";
    INT32 u4Ret = run_command(startusb, 0);
    UPG_LOG(UPG_LOG_DEBUG, "[UPG]startUSB run_command:%s  ret:%d\n", startusb, u4Ret);
	if (-1 == u4Ret) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", startusb);	
		return UPGR_FAIL;
	}
	return UPGR_OK;
}

INT32 stopUSB(){
	UPG_LOG(UPG_LOG_ERROR, "[UPG] call cmd: usb stop.\n");
	char *stopusb = "usb stop";
	INT32 u4Ret = run_command(stopusb, 0);
	UPG_LOG(UPG_LOG_ERROR, "[UPG] run cmd: %s ret:%d\n", stopusb, u4Ret);
	if(-1 == u4Ret){
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd:%s error\n", stopusb);
		return UPGR_FAIL;
	}
	return UPGR_OK;
}

INT32 loadFile(CHAR * szFullPath, UINT32 *u4ImgStartAddr, UINT32 *u4ImgSize) {
	CHAR szCmd[1024];
	INT32 u4Ret = 0;
	UINT32 i = 0;
	for (i = 0; i < UPG_USB_NUM; i++) {
		u4Ret = sprintf(szCmd, "fatload usb %d 0x%X %s", i, u4ImgStartAddr, szFullPath);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_usb.c loadFile failed\n");	
			return UPGR_FAIL;
		}
	
	    u4Ret = run_command(szCmd, 0);
		UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile run_command:%s ret:%d\n", szCmd, u4Ret);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", szCmd);	
			continue;						//try to next usb port
		} else {
		    UsbNum = i; 
            strcpy(FullPath, szFullPath);     
			break;							//load file success
		}
	}

	if (u4Ret == -1) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile failed\n");	
		return UPGR_FAIL;
	} else {
		//Get loaded file info
		CHAR * tmp;
		UINT32 u4FileSize = 0;
		if ((tmp = getenv ("filesize")) == NULL) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==0\n");
			return UPGR_FAIL;
		} else {
			u4FileSize = simple_strtoul (tmp, NULL, 16);
			if (u4FileSize <= 0) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile filesize==0\n");
				return UPGR_FAIL;	
			} else {
				UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile filesize=0x%X\n", u4FileSize);
				*u4ImgSize = u4FileSize;
				return UPGR_OK;
			}
		}
			
		
	}
}

/*-----------------------------------------------------------------------------
 * Name: usbGetImage
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
INT32 usbGetImage(CHAR *szFullPath, UINT32 *u4pImgStartAddr, UINT32 *u4pImgSize) {
	INT32 i4Ret = UPGR_FAIL;
	/*i4Ret = startUSB();
	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}*/

	i4Ret = loadFile(szFullPath, u4pImgStartAddr, u4pImgSize);

	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}
	
	return i4Ret;
}

INT32 loadFilePart(CHAR * szFullPath, UINT32 *u4ImgStartAddr, UINT32 u4Length,UINT32 u4offset) {
	CHAR szCmd[1024];
	INT32 u4Ret = 0;
	//UINT32 i = 0;
	//for (i = 0; i < UPG_USB_NUM; i++) {
		u4Ret = sprintf(szCmd, "fatloadex usb %d 0x%X %s 0x%X 0x%X", UsbNum, u4ImgStartAddr, szFullPath,u4Length,u4offset);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_usb.c loadFile failed\n");	
			return UPGR_FAIL;
		}
	
		UPG_LOG(UPG_LOG_ERROR, "[UPG]run_command:%s\n", szCmd);	
	    u4Ret = run_command(szCmd, 0);
		UPG_LOG(UPG_LOG_DEBUG, "[UPG]loadFile run_command:%s ret:%d\n", szCmd, u4Ret);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", szCmd);	
			//continue;						//try to next usb port			
            return UPGR_FAIL;
		} else {
			//break;							//load file success
		}
	//}

	if (u4Ret == -1) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]loadFile failed\n");	
		return UPGR_FAIL;
	} 
    
	return UPGR_OK;

}
 
INT32 usbGetImagePart(UINT32 *u4pImgStartAddr, UINT32 u4Length,UINT32 u4offset) {
	INT32 i4Ret = UPGR_FAIL;
	/*i4Ret = startUSB();
	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}*/

	i4Ret = loadFilePart(FullPath, u4pImgStartAddr, u4Length,u4offset);

	if (i4Ret == UPGR_FAIL) {
		return UPGR_FAIL;
	}
	
	return i4Ret;
}

INT32 getFileSize(CHAR *szFullPath, UINT32 *u4pImgSize) {
	CHAR szCmd[1024];
	INT32 u4Ret = 0;
	UINT32 i = 0;
	for (i = 0; i < UPG_USB_NUM; i++) {
		u4Ret = sprintf(szCmd, "fatgetlength usb %d %s", i, szFullPath);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]upg_usb.c getFileSize failed\n");	
			return UPGR_FAIL;
		}
	
	    u4Ret = run_command(szCmd, 0);
		UPG_LOG(UPG_LOG_DEBUG, "[UPG]getFileSize run_command:%s ret:%d\n", szCmd, u4Ret);
		if (-1 == u4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]call cmd: %s failed\n", szCmd);	
			continue;						//try to next usb port
		} else {
		    UsbNum = i; 
            strcpy(FullPath, szFullPath);     
			break;							//load file success
		}
	}

	if (u4Ret < 0) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]getFileSize failed\n");	
		return UPGR_FAIL;
	} else {
		CHAR * tmp;
		UINT32 u4FileSize = 0;
		if ((tmp = getenv ("filelength")) == NULL) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]filelength==0\n");
			return UPGR_FAIL;
		} else {
			u4FileSize = simple_strtoul (tmp, NULL, 16);
			if (u4FileSize <= 0) {
				UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]get filelength==0\n");
				return UPGR_FAIL;	
			} else {
				UPG_LOG(UPG_LOG_DEBUG, "[UPG]filelength=0x%X\n", u4FileSize);
				*u4pImgSize = u4FileSize;
				return UPGR_OK;
			}
		}
	}

	return u4Ret;
}

/*-----------------------------------------------------------------------------
 * Name: usbFindFile
 *
 * Description: find file by name(such as DVD.bin, 8530_linux_demo_dbg.bin) from USB/directory
 *
 * Inputs:    	None
 * Outputs: -	szDirectory		Which directory to search 
 *				szFileName	    The searched file name
 *
 * Returns: 	UPGR_OK			Find the matched file success
 *				UPGR_FAIL		Find the matched file failed
 ----------------------------------------------------------------------------*/
INT32 usbFindFile(CHAR *szDirectory, CHAR *szFileName, CHAR *szMatchedfilename) {
	INT32 i4Ret = UPGR_FAIL;
    CHAR* szFullDirectory = malloc(256 * sizeof(CHAR));
	CHAR* szPart = malloc(256 * sizeof(CHAR));
	UINT32 i = 0;
	memset((void*)szPart, 0, 256);
	memset((void*)szFullDirectory, 0, 256);
	strcat(szFullDirectory, "/");
	strcat(szFullDirectory, szDirectory);
    for (i = 0; i < UPG_USB_NUM; i++) {		
		sprintf(szPart,"%d",i);
        i4Ret = x_match_file_from_dir("usb", szPart,szFullDirectory, szFileName, szMatchedfilename);
		if (-1 == i4Ret) {
			UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]find file from usb %d failed\n", i);	
			continue;						//try to next usb port
		} else {
			break;							//find file success
		}
    }
	
	if (i4Ret == UPGR_FAIL) {
		UPG_LOG(UPG_LOG_ERROR, "[UPG_ERROR]cannot find file %s from usb \n",szFileName);	
		return UPGR_FAIL;
	}
	free(szFullDirectory);
	free(szPart);
	return i4Ret;
}

