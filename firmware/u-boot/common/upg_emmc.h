/*
 * common/upg_emmc.h
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


#ifndef _UPG_FLASH_H_
#define _UPG_FLASH_H_

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
INT32 readDataFromEMMC(UINT32 noffset, UINT32 size, void* buffer) ;

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
INT32 writeDataToEMMC(UINT32 noffset, UINT32 size, void* buffer) ;

	
#endif /*_UPG_FLASH_H_ */

