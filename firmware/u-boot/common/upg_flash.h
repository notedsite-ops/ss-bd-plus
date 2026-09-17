/*
 * common/upg_flash.h
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


#ifndef _UPG_FLASH_H_
#define _UPG_FLASH_H_

INT32 readDataFromNand(UINT32 dstaddr, UINT32 noffset, UINT32 size);

INT32 writeDataToNand(UINT32 * u4memStartAddr, UINT32 u4MemSize, UINT32 u4PartID, 
	UINT32 u4OffsetFromPart, UINT32 u4PartOffsetFromFlash, UINT32 u4PartSize);
	
#endif /*_UPG_FLASH_H_ */

