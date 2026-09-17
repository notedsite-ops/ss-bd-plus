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

#define IO_READ32(_reg_)           (*((volatile UINT32*)(_reg_)))


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


static INT32 MemRead(cmd_tbl_t *cmdtp, int flag, INT32 i4Argc, const CHAR ** szArgv)
{
#if 1
    UINT32 u4SrcAddr;
    UINT32 u4Len;
    UINT32 u4Idx;

    if ((i4Argc < 2) || (szArgv == NULL) || (szArgv[1] == NULL) || (szArgv[2] == NULL))
    {
        ZP_LOG(0, "read <addr> <length>\n");
        return 0;
    }

    u4SrcAddr = StrToInt(szArgv[1]);
    u4Len = StrToInt(szArgv[2]);
    ZP_LOG(0,"Read addr = 0x%08x, length = %d\n",u4SrcAddr, u4Len);
    if (u4Len == 0)
    {
        return 0;
    }
    if (u4Len > 0x1000)
    {
        u4Len = 0x1000;
    }

    // DW alignment
    u4SrcAddr &= 0xFFFFFFFC;

    for (u4Idx = 0;
        u4Idx < u4Len;
        u4Idx += 16)
    {
        //lint -e{613}
        ZP_LOG(0,"0x%08X | %08X %08X %08X %08X\n"
            , (u4SrcAddr + u4Idx)
            ,IO_READ32((u4SrcAddr + u4Idx+ 0))
            ,IO_READ32((u4SrcAddr + u4Idx+ 4))
            ,IO_READ32((u4SrcAddr + u4Idx+ 8))
            ,IO_READ32((u4SrcAddr + u4Idx+ 12)));
    }
#endif
    return 0;
//lint -e{818}
}


U_BOOT_CMD(
    read,    5,  1,  MemRead,
    "MemRead          - Used for read memory \n", //Simple help
    "                 - (Default)dword aligh\n"
    "MemRead            Parameter : regiser start , memory length\n"
);


