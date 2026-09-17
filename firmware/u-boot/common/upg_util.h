/*
 * common/upg_util.h
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


#ifndef _UPG_UTIL_H_
#define _UPG_UTIL_H_

void UPG_LOG(int level, const char *fmt, ...);

BOOL fgUint32MemCmp(UINT32 u4MemAddr1, UINT32 u4MemAddr2, UINT32 u4Size);
CHAR * catFilePathName(CHAR* szFullPath, CHAR* szPath, CHAR* szName);
void md5(unsigned char *input, int len, unsigned char output[16]);

#endif /* _UPG_UTIL_H_ */

