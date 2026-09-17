/*
 *  drivers/mtd/nandids.c
 *
 *  Copyright (C) 2002 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <common.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/mt85xx_nand_config.h>

/*
*	Chip ID list
*
*	Name. ID code, pagesize, chipsize in MegaByte, eraseblock size,
*	options
*
*	Pagesize; 0, 256, 512
*	0	get this information from the extended chip ID
+	256	256 Byte page size
*	512	512 Byte page size
*/
struct nand_flash_dev nand_flash_ids[] = {

#ifdef CONFIG_MTD_NAND_MUSEUM_IDS
	{"NAND 1MiB 5V 8-bit",		0x6e, 256, 1, 0x1000, 0},
	{"NAND 2MiB 5V 8-bit",		0x64, 256, 2, 0x1000, 0},
	{"NAND 4MiB 5V 8-bit",		0x6b, 512, 4, 0x2000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xe8, 256, 1, 0x1000, 0},
	{"NAND 1MiB 3,3V 8-bit",	0xec, 256, 1, 0x1000, 0},
	{"NAND 2MiB 3,3V 8-bit",	0xea, 256, 2, 0x1000, 0},
	{"NAND 4MiB 3,3V 8-bit", 	0xd5, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe3, 512, 4, 0x2000, 0},
	{"NAND 4MiB 3,3V 8-bit",	0xe5, 512, 4, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xd6, 512, 8, 0x2000, 0},

	{"NAND 8MiB 1,8V 8-bit",	0x39, 512, 8, 0x2000, 0},
	{"NAND 8MiB 3,3V 8-bit",	0xe6, 512, 8, 0x2000, 0},
	{"NAND 8MiB 1,8V 16-bit",	0x49, 512, 8, 0x2000, NAND_BUSWIDTH_16},
	{"NAND 8MiB 3,3V 16-bit",	0x59, 512, 8, 0x2000, NAND_BUSWIDTH_16},
#endif

	{"NAND 16MiB 1,8V 8-bit",	0x33, 512, 16, 0x4000, 0},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 512, 16, 0x4000, 0},
	{"NAND 16MiB 1,8V 16-bit",	0x43, 512, 16, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 16MiB 3,3V 16-bit",	0x53, 512, 16, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 32MiB 1,8V 8-bit",	0x35, 512, 32, 0x4000, 0},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 512, 32, 0x4000, 0},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 512, 32, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 512, 32, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 512, 64, 0x4000, 0},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 512, 64, 0x4000, 0},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 512, 64, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 512, 64, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 512, 128, 0x4000, 0},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 512, 128, 0x4000, 0},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 512, 128, 0x4000, NAND_BUSWIDTH_16},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 512, 128, 0x4000, NAND_BUSWIDTH_16},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 512, 256, 0x4000, 0},

	/*
	 * These are the new chips with large page size. The pagesize and the
	 * erasesize is determined from the extended id bytes
	 */
#define LP_OPTIONS (NAND_SAMSUNG_LP_OPTIONS | NAND_NO_READRDY | NAND_NO_AUTOINCR)
#define LP_OPTIONS16 (LP_OPTIONS | NAND_BUSWIDTH_16)

	/*512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 0,  64, 0, LP_OPTIONS},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 0,  64, 0, LP_OPTIONS16},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 0,  64, 0, LP_OPTIONS16},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xF1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 3,3V 8-bit",	0xD1, 0, 128, 0, LP_OPTIONS},
	{"NAND 128MiB 1,8V 16-bit",	0xB1, 0, 128, 0, LP_OPTIONS16},
	{"NAND 128MiB 3,3V 16-bit",	0xC1, 0, 128, 0, LP_OPTIONS16},

	/* 2 Gigabit */
	{"NAND 256MiB 1,8V 8-bit",	0xAA, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 3,3V 8-bit",	0xDA, 0, 256, 0, LP_OPTIONS},
	//{"NAND 128MiB 3,3V 8-bit",	0x98, 0, 256, 0, LP_OPTIONS},
	{"NAND 256MiB 1,8V 16-bit",	0xBA, 0, 256, 0, LP_OPTIONS16},
	{"NAND 256MiB 3,3V 16-bit",	0xCA, 0, 256, 0, LP_OPTIONS16},

	/* 4 Gigabit */
	{"NAND 512MiB 1,8V 8-bit",	0xAC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 3,3V 8-bit",	0xDC, 0, 512, 0, LP_OPTIONS},
	{"NAND 512MiB 1,8V 16-bit",	0xBC, 0, 512, 0, LP_OPTIONS16},
	{"NAND 512MiB 3,3V 16-bit",	0xCC, 0, 512, 0, LP_OPTIONS16},

	/* 8 Gigabit */
	{"NAND 1GiB 3,3V 8-bit",	0x38, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 1,8V 8-bit",	0xA3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 3,3V 8-bit",	0xD3, 0, 1024, 0, LP_OPTIONS},
	{"NAND 1GiB 1,8V 16-bit",	0xB3, 0, 1024, 0, LP_OPTIONS16},
	{"NAND 1GiB 3,3V 16-bit",	0xC3, 0, 1024, 0, LP_OPTIONS16},

	/* 16 Gigabit */
	{"NAND 2GiB 1,8V 8-bit",	0xA5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 3,3V 8-bit",	0xD5, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 3,3V 8-bit",	0x48, 0, 2048, 0, LP_OPTIONS},
	{"NAND 2GiB 1,8V 16-bit",	0xB5, 0, 2048, 0, LP_OPTIONS16},
	{"NAND 2GiB 3,3V 16-bit",	0xC5, 0, 2048, 0, LP_OPTIONS16},

    /* 32 Gigabit */
    {"NAND 4GiB 3,3V 8-bit",	0xD7, 0, 4096, 0, LP_OPTIONS},
	/*
	 * Renesas AND 1 Gigabit. Those chips do not support extended id and
	 * have a strange page/block layout !  The chosen minimum erasesize is
	 * 4 * 2 * 2048 = 16384 Byte, as those chips have an array of 4 page
	 * planes 1 block = 2 pages, but due to plane arrangement the blocks
	 * 0-3 consists of page 0 + 4,1 + 5, 2 + 6, 3 + 7 Anyway JFFS2 would
	 * increase the eraseblock size so we chose a combined one which can be
	 * erased in one go There are more speed improvements for reads and
	 * writes possible, but not implemented now
	 */
	{"AND 128MiB 3,3V 8-bit",	0x01, 2048, 128, 0x4000,
	 NAND_IS_AND | NAND_NO_AUTOINCR |NAND_NO_READRDY | NAND_4PAGE_ARRAY |
	 BBT_AUTO_REFRESH
	},

	{NULL,}
};

#if CONFIG_READID6BYTE

struct nand_flash_dev_ext nand_flash_ids_ext[] = {
         /* name,          maf_id,  dev_id,  id3, id4, id5,id6,pagesize,chipsize,erasesize,oobsize,buswidth eccinfo*/
    {"NAND_TC58NVG4D1DTG",   0x98,  0xD5,  0x94, 0xBA, 0,    0,    4096,  2048,  0x80000,   208,     0,"no info" },
    {"NAND_TC58NVG1S3HTA00", 0x98,  0xDA,  0x90, 0x15, 0x76, 0x16, 2048,  256,   0x20000,   128,     0,"8bit_ecc/512 byte" },
    {"NAND_H27U2G8F2DTR",    0xAD,  0xDA,  0x90, 0x95, 0x46, 0xAD, 2048,	256,	 0x20000,   128,     0,"4bit_ecc/512 byte" },
    {"NAND_H27UAG8T2ATR" ,   0xAD,  0xD5,  0x94, 0x25, 0,    0,    4096,  2048,  0x80000,   224,     0,"12bit_ecc/528 byte" },
		{"NAND_H27UAG8T2BTR" ,   0xAD,  0xD5,  0x94, 0x9A, 0,    0,	   8192,	2048,	 0x200000,  448,	   0, "24bit_ecc/1024 byte" },
		{"NAND_H27UBG8T2ATR" ,   0xAD,  0xD7,  0x94, 0x9A, 0,    0,	   8192,	4096,	 0x200000,  448,	   0, "24bit_ecc/1024 byte" },
		{"NAND_TC58NVG0S3ETA00", 0x98,  0xD1,  0x90, 0x15, 0,    0,    2048,	128,	 0x20000,   64,      0,"1bit_ecc/512 byte" },
		{"NAND_MT29F4G08ABAEA",	 0x2C,  0xDC,  0x90, 0xA6, 0x54, 0,    4096,  512,   0x40000,   224,     0,"8bit_ecc/540 byte" },
		{"NAND_MT29F8G08ABACAWP",0x2C,  0xD3,  0x90, 0xA6, 0x64, 0,    4096,  1024,  0x40000,   224, 	   0,"no info" },
		{"NAND_MT29F8G08ABA",	   0x2C,  0x38,  0x00, 0x26, 0,    0,    4096,  1024,  0x80000,   224, 	   0,"no info" },
		{"NAND_MT29F16G08CBABA", 0x2C,  0x48,  0x04, 0x46, 0,    0,    4096,	2048,	 0x100000,  224,	   0,"no info" },
		{"NAND_MT29F16G08ABACA", 0x2C,  0x48,  0x00, 0x26, 0xA9, 0,    4096,	2048,	 0x80000,   224,	   0,"no info" },
		{"NAND_MT29F16G08CBACA", 0x2C,  0x48,  0x04, 0x4A, 0xA5, 0,    4096,  2048,  0x100000,  224,     0,"no info"  },
		{"NAND_S34ML04G200TFI00",0x01,  0xDC,  0x90, 0x95, 0x56, 0,    2048,  512,   0x20000,   128,     0,"4bit_ecc/512 byte" },
		{"NAND_S34ML02G200TFI00",0x01,  0xDA,  0x90, 0x95, 0x46, 0x01, 2048,  256,   0x20000,   128,     0,"4bit_ecc/512 byte" }, 
		{"NAND_MX30LF2G28AB",    0xC2,  0xDA,  0x90, 0x95, 0x07, 0x0 , 2048,  256,   0x20000,   112,     0,"8bit_ecc/512 byte" },
	    {"NAND_MX30LF4G28AB",    0xC2,  0xDC,  0x90, 0x95, 0x57, 0x0 , 2048,  512,   0x20000,   112,     0,"8bit_ecc/512 byte" },
		{"NAND_MX30LF2G18AC",	  0xC2,  0xDA,	0x90, 0x95, 0x06, 0x0 , 2048,  256,   0x20000,	 64 ,	  0,"4bit_ecc/512 byte" },
		{"NAND_F59L1G81A",       0xC8,  0xD1,  0x80, 0x95, 0x40, 0x0 , 2048,  128,   0x20000,   64,      0,"8bit_ecc/512 byte" },
	  {"NAND_H27U4G8F2ETR",    0xAD,  0xDC,  0x90, 0x95, 0x56, 0x0 , 2048,  512,   0x20000,   128,     0,"4bit_ecc/512 byte" },
	//{"NAND_TC58NVG2S0HTA00", 0x98,  0xDC,  0x90, 0x26 ,0x76,0x0 ,  4096,  512,   0x40000,   256,     0,"8bit_ecc/512 byte" },
	   	    {NULL,}
};

#else
struct nand_flash_dev_ext nand_flash_ids_ext[] = {
         /* name,                         maf_id,  dev_id,    id3,     id4,   pagesize,   chipsize,   erasesize,    oobsize,   buswidth*/
        {"NAND_TC58NVG4D1DTG",  0x98,  0xD5,  0x94, 0xBA,   4096,   2048,   0x80000,   208,     0 },
#if CONFIG_TC58NVG1S3HTA00        
        {"NAND_TC58NVG1S3HTA00", 0x98,  0xDA,  0x90, 0x15,   2048,   256,   0x20000,   128,     0 },
#endif
    	{"NAND_H27UAG8T2ATR" ,  0xAD,  0xD5,  0x94, 0x25,   4096,   2048,   0x80000,   224,     0 },
		{"NAND_H27UAG8T2BTR" ,  0xAD,  0xD5,  0x94, 0x9A,	8192,	2048,	0x200000,  448,	    0 },
		{"NAND_H27UBG8T2ATR" ,  0xAD,  0xD7,  0x94, 0x9A,	8192,	4096,	0x200000,  448,	    0 },
		{"NAND_TC58NVG0S3ETA00",0x98,  0xD1,  0x90, 0x15,   2048,	128,	0x20000,   64,      0 },
		{"NAND_MT29F8G08ABA",	0x2C,  0x38,  0x00, 0x26,   4096,   1024,   0x80000,   224, 	0 },
		{"NAND_MT29F8G08ABACA",	 0x2C,	   0xD3,  0x90, 0xA6,  4096,   1024,   0x40000,    224, 	0 },
		{"NAND_MT29F16G08CBABA",  0x2C,  0x48,  0x04, 0x46,   4096,	2048,	0x100000,  224,	    0 },
		{"NAND_MT29F16G08ABACA", 0x2C,  0x48,  0x00, 0x26, 4096,	2048,	0x80000,  224,	    0 },
			{"NAND_S34ML02G200TFI000", 0x01,  0xDA,  0x90, 0x95, 2048,    256,   0x20000, 128,      0}, 
		{"NAND_MT29F16G08CBACA", 0x2C,  0x48,  0x04, 0x4A, 4096,    2048,   0x100000, 224,      0 },
	    {NULL,}
};
#endif

/*
*	Manufacturer ID list
*/
struct nand_manufacturers nand_manuf_ids[] = {
	{NAND_MFR_TOSHIBA, "Toshiba"},
	{NAND_MFR_SAMSUNG, "Samsung"},
	{NAND_MFR_FUJITSU, "Fujitsu"},
	{NAND_MFR_NATIONAL, "National"},
	{NAND_MFR_RENESAS, "Renesas"},
	{NAND_MFR_STMICRO, "ST Micro"},
	{NAND_MFR_HYNIX, "Hynix"},
	{NAND_MFR_MICRON, "Micron"},
	{NAND_MFR_AMD, "SPANSION/AMD"},
    {NAND_MFR_POWERFLASH, "PowerFlash/ESMT"},
    {NAND_MFR_MXIC, "MXIC"},
	{0x0, "Unknown"}
};
