/*
 * common/cmd_test.c
 *
 * Upgrade firmware entry. It responses for parse command from user input and check upgrade whether is
 * is success
 *
 * Copyright (C) 2010-2012 MediaTek Inc.
 * $Author:
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
#include <command.h>
#include <asm/byteorder.h>
#include <malloc.h>
#include <asm/arch/x_typedef.h>

#include <mt85xx_gpio.h>
#include <asm-arm/arch-mt8530/args_to_uboot.h>

#define IO_WRITE32(_reg_, _val_)   (*((volatile UINT32*)(_reg_)) = (_val_))



static void ZP_LOG(int level, const char *fmt, ...)
{
    if (level <= 5)
    {
        //printf(fmt);

        va_list args;
        int i;
        char printbuffer[256];

        va_start (args, fmt);

        /* For this to work, printbuffer must be larger than
         * anything we ever want to print.
         */
        i = vsprintf (printbuffer, fmt, args);
        va_end (args);

        /* Print the string */
        puts (printbuffer);
    }
}

static UINT32 x_strlen(const CHAR *ps_s)
{
    CHAR *pc_c;

    if (ps_s == (CHAR *) NULL)
    {
        return(0);
    }

    pc_c = (CHAR *) ps_s;
    while (*ps_s != '\0')
    {
        ++ps_s;
    }

    return(ps_s - pc_c);
}

static UINT32 StrToHex(const CHAR* pszStr, UINT32 u4Len)
{
    UINT32 u4Idx;
    UINT32 u4ReturnValue = 0;

    if ((pszStr == NULL) || (u4Len == 0))
    {
        return 0;
    }

    u4Len = (u4Len > 8) ? 8 : u4Len;

    for (u4Idx = 0;
        u4Idx < u4Len;
        u4Idx++)
    {
        if ((pszStr[u4Idx] >= '0') && (pszStr[u4Idx] <= '9'))
        {
            u4ReturnValue = u4ReturnValue << 4;
            u4ReturnValue += (UINT32)(UINT8)(pszStr[u4Idx] - '0');
        }
        else
        if ((pszStr[u4Idx] >= 'A') && (pszStr[u4Idx] <= 'F'))
        {
            u4ReturnValue = u4ReturnValue << 4;
            u4ReturnValue += (UINT32)(UINT8)(pszStr[u4Idx] - 'A' ) + 10;
        }
        else
        if ((pszStr[u4Idx] >= 'a') && (pszStr[u4Idx] <= 'f'))
        {
            u4ReturnValue = u4ReturnValue << 4;
            u4ReturnValue += (UINT32)(UINT8)(pszStr[u4Idx] - 'a') + 10;
        }
        else
        {
            return 0;
        }
    }

    return u4ReturnValue;
}

static UINT32 StrToDec(const CHAR* pszStr, UINT32 u4Len)
{
    UINT32 u4Idx;
    UINT32 u4ReturnValue = 0;

    if ((pszStr == NULL) || (u4Len == 0))
    {
        return 0;
    }

    // 0xFFFFFFFF = 4294967295
    u4Len = (u4Len > 10) ? 10 : u4Len;

    for (u4Idx = 0;
        u4Idx < u4Len;
        u4Idx++)
    {
        if ((pszStr[u4Idx] >= '0') && (pszStr[u4Idx] <= '9'))
        {
            u4ReturnValue *= 10;
            u4ReturnValue += (UINT32)(UINT8)(pszStr[u4Idx] - '0');
        }
        else
        {
            return 0;
        }
    }

    return u4ReturnValue;
}


static UINT32 StrToInt(const CHAR* pszStr)
{
    UINT32 u4Len;

    if (pszStr == NULL)
    {
        return 0;
    }

    u4Len = x_strlen(pszStr);

    if (u4Len > 2)
    {
        if ((pszStr[0] == '0') && (pszStr[1] == 'x'))
        {
            return StrToHex(&pszStr[2], u4Len - 2);
        }
    }

    return StrToDec(pszStr, u4Len);
}


static INT32 MemWrite(cmd_tbl_t *cmdtp, int flag, INT32 i4Argc, const CHAR ** szArgv)
{
#if 1
    UINT32 u4DestAddr;
    UINT32 u4Value;

    if ((i4Argc < 2) || (szArgv == NULL) || (szArgv[1] == NULL) || (szArgv[2] == NULL))
    {
        return 0;
    }

    u4DestAddr = StrToInt(szArgv[1]);
    u4Value = StrToInt(szArgv[2]);

    ZP_LOG(0,"Write addr = 0x%08x, u4Value = 0x%08x\n",u4DestAddr, u4Value);
    // DW alignment
    u4DestAddr &= 0xFFFFFFFC;

    //ASSERT(u4DestAddr != NULL);
    IO_WRITE32(u4DestAddr, u4Value);

    ZP_LOG(0, "write addr: 0x%08X  =  0x%08X\n",u4DestAddr,u4Value);

#endif
    return 0;
    //lint -e{818}
}



U_BOOT_CMD(
    write,    5,  1,  MemWrite,
    "MemWrite          - Used for write register \n", //Simple help
    "                  - (Default)dword aligh\n"
    "MemWrite            Parameter : regiser address , value\n"
);


