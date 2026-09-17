/*
 * mt85xx partition operation functions
 */
#ifndef MT85XX_PART_OPER_C
#define MT85XX_PART_OPER_C


/*
 * header file
 */
#include <common.h>
#include <nand.h>
#include <malloc.h>
#include <asm/arch/x_typedef.h>

#include <asm/errno.h>
#include <asm/io.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/bitops.h>
#include <asm/arch/mt85xx_part_tbl.h>
#include "linux/mtd/mt85xx_part_oper.h"
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/err.h>
#include <mt85xx_gpio.h>
#include <asm/arch/x_bim.h>

#include <chip_ver.h>
/* proc */

#define MAX_PAGE_SIZE	8192
#define ISUBIVOLUME(binInfo) ((0x0000F000 & binInfo)>> 12) 	//bit[1512]

#define ASSERT(e) \
        if(!(e)){ \
          while(1); \
        }
#define NAND_LOG_ERROR 0
#define NAND_LOG_INFO  1
#define NAND_LOG_DEBUG 2
INT32 NAND_LOG_LEVEL = NAND_LOG_INFO; //default log level
#ifdef CFG_UBOOT_CUSTOM_JSN
#define SONY_GET_REGISTRY  1
#else
#define SONY_GET_REGISTRY  0
#endif

#if SONY_GET_REGISTRY
BOOL b_get_registry = FALSE;
#endif

void NAND_LOG(int level, char *s, ...)
{
  if(level<= NAND_LOG_LEVEL)
  {
	va_list args;
	int i;
	char printbuffer[400];
	char *log="[NAND]";

	va_start(args, log);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf (printbuffer, log, args);
	va_end (args);
	/* Print the string */
	puts (printbuffer);
   /*////////////////////*/
	va_start (args, s);

	/* For this to work, printbuffer must be larger than
	 * anything we ever want to print.
	 */
	i = vsprintf (printbuffer, s, args);
	va_end (args);
	/* Print the string */
	puts (printbuffer);
  }
}
UINT32 get_part_id_by_name(const char* part_name)
{
  PART_TBL_ITEM *r_part_tbl;
  if (part_name == NULL)
  {
    printk("get_part_id_by_name,name is NULL\n");
    return 0;
  }
  r_part_tbl = mt85xx_get_part_info_by_name(part_name);
  return r_part_tbl?r_part_tbl->u4_id:0;
}
UINT32 get_part_info_by_name(const char* part_name)
{
  PART_TBL_ITEM *r_part_tbl;
  if (part_name == NULL)
  {
    printk("get_part_info_by_name,name is NULL\n");
    return 0;
  }
  r_part_tbl = mt85xx_get_part_info_by_name(part_name);
  return r_part_tbl?r_part_tbl->u4_info:0;
}
UINT32 get_part_addr_by_name(const char* part_name)
{
  PART_TBL_ITEM *r_part_tbl;
  if (part_name == NULL)
  {
    printk("get_part_addr_by_name,name is NULL\n");
    return 0;
  }
  r_part_tbl = mt85xx_get_part_info_by_name(part_name);
  return r_part_tbl?r_part_tbl->u4_offset:0;
}
UINT32 get_part_size_by_name(const char* part_name)
{
  PART_TBL_ITEM *r_part_tbl;
  if (part_name == NULL)
  {
    printk("get_part_size_by_name,name is NULL\n");
    return 0;
  }
  r_part_tbl = mt85xx_get_part_info_by_name(part_name);
  return r_part_tbl?r_part_tbl->u4_size:0;
}
UINT32 get_page_size(void)
{
  struct mtd_info *mtd;
  mtd = get_mtd_device(NULL, 0);
  return mtd->writesize;
}
UINT32 get_block_size(void)
{
  struct mtd_info *mtd;
  mtd = get_mtd_device(NULL, 0);
  return mtd->erasesize;
}
/*static void erase_buffer(void *buffer, size_t size)
{
	const uint8_t kEraseByte = 0xff;
	if (buffer != NULL && size > 0) 
	{
		memset(buffer, kEraseByte, size);
	}
}*/

#if ADAPT_MTD_LINUX
enum PIT_VER_T _pit_ver=V_ERR;
UINT32 *orig_pit=NULL;
UINT32 *adpt_pit=NULL;
UINT32 _u4_orig_part_nr=0;
UINT32 _u4_same=0;

INT32 i4CheckPITVer(UINT32 *phead, UINT32 u4HeadSig)
{
  if ((*(phead) != u4HeadSig) || (*(phead+1) != u4HeadSig))
  {
    NAND_LOG(NAND_LOG_DEBUG, "no part head sig found -1\n");
  	return -1;
  }
  else
  {
    if (*(phead+2) != u4HeadSig || *(phead+3) != u4HeadSig)
    {
      NAND_LOG(NAND_LOG_DEBUG, "v1 pit found\n");
	  return V1;
    }
	else
	{
	  NAND_LOG(NAND_LOG_DEBUG, "v0 pit found\n");
	  return V0;
	}
  	return 0;
  }
}

UINT32 u4GetPartNum(UINT32 pit, UINT32 u4HeadSig, UINT32 u4TailSig)
{
  UINT32 i=0;
  V1_PIT_HEADER_T *v1part=NULL;
  PART_TBL_ITEM *v0Part=NULL;
  INT32 pitver=i4CheckPITVer((UINT32*)pit,u4HeadSig);
  UINT32 len=0;
  if(pitver == V0)
  {
	for(i=0; i<PART_TBL_ITEM_NR_MAX; i++)
	{
	  v0Part = (PART_TBL_ITEM*)(pit+i*sizeof(PART_TBL_ITEM));
	  if((v0Part->u4_id     != u4TailSig) ||
	  	 (v0Part->u4_info   != u4TailSig) ||
	  	 (v0Part->u4_offset != u4TailSig) ||
	  	 (v0Part->u4_size   != u4TailSig) )
	  	{
	  	  continue;
	  	}
	    /*end sig found*/
		len = i;
	}
  }
  else if(pitver == V1)
  {
    v1part = (V1_PIT_HEADER_T*)pit;
	len = v1part->u4_item_num;
  }
  else
  {
    len = 0;
  }
  NAND_LOG(NAND_LOG_DEBUG,"pit part num 0x%x\n", len);
  return len;
}

UINT32 u4GetItemLen(UINT32 pit, UINT32 u4HeadSig, UINT32 u4TailSig)
{
	V1_PIT_HEADER_T *v1part=NULL;
	INT32 pitver=i4CheckPITVer((UINT32*)pit,u4HeadSig);
    UINT32 len=0;
	
    if(pitver == V0)
    {
      len = sizeof(PART_TBL_ITEM);
    }
	else if (pitver == V1)
	{
	  v1part=(V1_PIT_HEADER_T*)pit;
	  len = v1part->u4_item_len;
	}
	else
	{
	  len = 0;
	}
	
	NAND_LOG(NAND_LOG_DEBUG,"pit item len 0x%x\n", len);
    return len;
}


INT32 u4SetPartHead(UINT32 pit, UINT32 pitver, UINT32 u4HeadSig)
{
  UINT32 *phead = (UINT32*)pit;
  int i=0;
  if (pitver == V0)
  {
    for (i=0; i<8; i++)
    {
      *(phead+i) = u4HeadSig;
    }
  }
  else if (pitver == V1)
  {
    *phead = u4HeadSig;
    *(phead+1) = u4HeadSig;
  }
  else
  {
    NAND_LOG(NAND_LOG_ERROR,"set part head failed!\n");
    return -1;
  }
  NAND_LOG(NAND_LOG_DEBUG,"set part head success!\n");
  return 0;
}
INT32 u4SetPartTail(UINT32 pit, UINT32 pitlen, UINT32 pitver, UINT32 u4TailSig)
{
  UINT32 *ptail = (UINT32*)(pit + sizeof(PART_TBL_ITEM)*(pitlen+1));
  int i=0;
  if (pitver == V0)
  {
    for (i=0; i<8; i++)
    {
      *(ptail+i) = u4TailSig;
    }
  }
  else if (pitver == V1)
  {
    ;//do nothing, v1 version has no tail.
  }
  else
  {
    NAND_LOG(NAND_LOG_ERROR,"set part tail failed!\n");
    return -1;
  }
  
  NAND_LOG(NAND_LOG_DEBUG,"set part tail success!\n");
  return 0;
}

PART_TBL_ITEM* pr_get_pit_item_by_idx(UINT32 pit, UINT32 idx, UINT32 pitver)
{
  V1_PIT_HEADER_T* hdr = (V1_PIT_HEADER_T*)pit;
  if (pitver == V0)
  {
  	return (PART_TBL_ITEM*)(pit + sizeof(PART_TBL_ITEM)*(idx+1));
  }
  return (PART_TBL_ITEM*)(pit + hdr->u4_header_len + (idx * hdr->u4_item_len));
}

PART_TBL_ITEM* mt85xx_get_part_item_by_name(UINT32 pit, const char* part_name, UINT32 pitver)
{
  UINT32 i=0;
  PART_TBL_ITEM *item=NULL;

  if(!part_name)
  	return NULL;
  
  for(i=0 ; i<u4_part_nr; i++)
  {
    item = pr_get_pit_item_by_idx(pit, i, pitver);
	if(strcmp(item->pc_name, part_name))
    {
            continue;
    }
	
	NAND_LOG(NAND_LOG_DEBUG, "get part item by name success\n");
	return item;
  }
  
  NAND_LOG(NAND_LOG_ERROR, "get part item by name error\n");
  return NULL;
}

UINT32 get_part_id_by_name_new(UINT32 pit, const char* part_name, UINT32 pitver)
{
  PART_TBL_ITEM *temp=NULL;

  if(!part_name)
  	return 0;
  
  temp = mt85xx_get_part_item_by_name(pit, part_name, pitver);
  return temp?temp->u4_id:0;
}
INT32 i4CopyPartItem(UINT32 pdest, UINT32 psource, UINT32 pitver)
{
  UINT32 u4ItemLen=0,u4CheckLen=0;
  
  if (pitver == V0)
  {
	u4CheckLen = 32;
  }
  else if (pitver == V1)
  {
	u4CheckLen = 40;
  }
  u4ItemLen = u4GetItemLen(psource, HEAD_SIG, TAIL_SIG);
  memcpy((u_char*)pdest, (u_char*)psource, u4CheckLen);
  return 0;
}
INT32 i4SetEnv(UINT32 pit, UINT32 u4PartCnt) 
{
    INT32 i = 0;	
    INT32 i4Ret = 0;
    char _bootargs_buf[1024] = "";
    char _tmp_buf[1024] = "";
	PART_TBL_ITEM *item=NULL;
    /* mtdparts */
    strcat(_bootargs_buf, "mtdparts=mt85xx_nand:");
    for (i = 0; (i < u4PartCnt); i++) 
    {
      item = pr_get_pit_item_by_idx(pit, i, _pit_ver);
      if( ISUBIVOLUME(item->u4_info) != 4 )
      {
        i4Ret = sprintf(_tmp_buf, "%dK(%s),", item->u4_size >> 10,item->pc_name);	
        if (-1 == i4Ret) 
		{
          NAND_LOG(NAND_LOG_ERROR, "[mt85xx_part_oper]creat szMtdPartsCmd failed\n");	
          return i4Ret;
        }
        strcat(_bootargs_buf, _tmp_buf);	
      }
    }
	NAND_LOG(NAND_LOG_INFO, "mt85xx_part_oper.c vSetEnv set mtdparts = %s \n", _bootargs_buf);	 
    i4Ret = setenv("mtdparts", _bootargs_buf);
    if (-1 == i4Ret) {
       NAND_LOG(NAND_LOG_ERROR, "mt85xx_part_oper set mtdparts failed, mtdparts = %s \n", _bootargs_buf);	
       return i4Ret;
    }
    i4Ret =  setenv("mtdids", "nand0=mt85xx_nand");
    if (-1 == i4Ret) {
       NAND_LOG(NAND_LOG_ERROR, "mt85xx_part_oper set mtdids failed\n");	
       return i4Ret;
    }
	NAND_LOG(NAND_LOG_INFO, "mt85xx_part_oper set env OK\n");

    return i4Ret;  
}
#if 0
INT32 i4SetMtdTbl(UINT32 pit, UINT32 u4PartCnt)
{
  UINT32 i=0;
  PART_TBL_ITEM *item=NULL;
  UINT32 pitver= i4CheckPITVer((UINT32*)pit, ADPT_HEAD_SIG);
  
  i4SetEnv(pit, u4PartCnt);
  for (i = 0; i < u4PartCnt; i++) 
  {
    item = pr_get_pit_item_by_idx(pit, i, pitver);
   // if( ISUBIVOLUME(item->u4_info) != 4 )
    {
	   if(mt85xx_init_mtd_table(item->pc_name) != 0)	
      {
        NAND_LOG(NAND_LOG_ERROR, "[Set Mtd Table]%s not be added!\n", item->pc_name);	
        return -1;
      }
    }
  }
	//set mtd success
	return 0;
}
#endif
INT32 i4FindPartTbl(UINT32 *pit, UINT32 *pitnr, UINT32 headsig, UINT32 tailsig)
{
    int r=0;
    UINT32 i=0;
	UINT32 internal=get_block_size();
	if(!pit)
	{
  	  NAND_LOG(NAND_LOG_ERROR, "[i4FindPartTbl] pit NULL.\n");
  	  r = -1;
  	  goto err;
	}
    for(i=SEARCH_PART_TBL_START; i<SEARCH_PART_TBL_END; i+=internal)
    {
        /* read nand */
		r = i4NFBPartitionRead(0,(UINT32)i,(UINT32)pit, PART_TBL_ITEM_NR_MAX*sizeof(NEW_PART_TBL_ITEM));
        if(r)
        {
            NAND_LOG(NAND_LOG_ERROR, "[i4FindPartTbl] NFBPartitionRead error!!\n");
            continue;
        }

        /* check head, the head sig is stored in the begining 8 byte of pc_name */
	    if ((i4CheckPITVer(pit, headsig)) < 0)
	    {
		  continue;
	    }
        r = 0;
		*pitnr = u4GetPartNum((UINT32)pit, headsig, tailsig);
		NAND_LOG(NAND_LOG_DEBUG, "[i4FindPartTbl] pit found!!\n");
		goto close;
		
    }
    /* not found */
err:
    NAND_LOG(NAND_LOG_ERROR, "[i4FindPartTbl] can't find pit!!\n");
	r = -1;
close:
	return r;

}
INT32 i4SearchAdptPIT(void)
{
  int r=0;
  UINT32 pitlen=0;
  /*find partition table in nand*/
  if(_u4_same)
  {
    if(!adpt_pit)
    {
      adpt_pit = kzalloc(sizeof(NEW_PART_TBL_ITEM)*PART_TBL_ITEM_NR_MAX, GFP_KERNEL);
      if(!adpt_pit)
      {
         NAND_LOG(NAND_LOG_ERROR, "[i4SearchAdptPIT] allocate adpt_pit memo failed.\n");
  	   return -1;
      }
    }
    r = i4FindPartTbl(adpt_pit, &pitlen, ADPT_HEAD_SIG, ADPT_TAIL_SIG);
    if(r)
    {
      NAND_LOG(NAND_LOG_INFO, "[i4SearchAdptPIT] error.\n");
  	  return -1;
    }
	#if 0
    if(i4SetMtdTbl((UINT32)adpt_pit, pitlen))
    {
      r = -1;
      NAND_LOG(NAND_LOG_ERROR, "mount adaptive mtd failed\n");
  	  return -1;
    }
	#endif
  }
  
  NAND_LOG(NAND_LOG_INFO, "[i4SearchAdptPIT] success.\n");
  return (INT32)adpt_pit;
}
INT32 i4GetAdptPit(void)
{
  return i4SearchAdptPIT();
}

//INT32 i4StoreAdptPit(void)
INT32 i4StoreAdptPit(UINT32 bin_pit)
{
  UINT32 u4Id=0;
  UINT32 offset=0;
  UINT32 itemnr=0;

  if (_pit_ver == V0)
  {
    itemnr = u4_part_nr + 2;
  }
  else if (_pit_ver == V1)
  {
    itemnr = u4_part_nr + 1;
  }
  else
  {
    NAND_LOG(NAND_LOG_DEBUG, "store adpt pit failed, due to part version error\n");
    return -1;
  }
  offset = ADPT_PIT_OFFSET;	  
  if (!_u4_same)
  {
    NAND_LOG(NAND_LOG_DEBUG, "store adpt pit in part_info_1 partition\n");
    u4Id = get_part_id_by_name_new((UINT32)adpt_pit, "part_info_1", _pit_ver);
	if (u4Id != 0)
	{
      /*store adaptive PIT part_info_1 parition.*/
	  if (get_block_size() > ADPT_PIT_OFFSET)
	    offset = get_block_size();
  	  offset += pr_get_pit_item_by_idx((UINT32)bin_pit, u4Id-1, _pit_ver)->u4_offset;
	  if (i4NFBPartitionWrite(0, offset, (UINT32)adpt_pit, 
      	                 (UINT32)(itemnr * sizeof(NEW_PART_TBL_ITEM))))
      {
        NAND_LOG(NAND_LOG_ERROR, "store part_info_1 adpt pit failed\n");
    	  return -1;
      }
	}
	else
	{
	  NAND_LOG(NAND_LOG_INFO, "no partition part_info_1 \n");
	}
    NAND_LOG(NAND_LOG_DEBUG, "store adpt pit in part_info_2 partition\n");
    u4Id = get_part_id_by_name_new((UINT32)adpt_pit, "part_info_2", _pit_ver);
	if (u4Id != 0)
	{
	  offset = ADPT_PIT_OFFSET;   
	  if (get_block_size() > ADPT_PIT_OFFSET)
	    offset = get_block_size();
  	  offset += pr_get_pit_item_by_idx((UINT32)bin_pit, u4Id-1, _pit_ver)->u4_offset;
      /*store adaptive PIT to part_info_2 parition.*/
      if (i4NFBPartitionWrite(0, offset, (UINT32)adpt_pit,
      	                 (UINT32)(itemnr * sizeof(NEW_PART_TBL_ITEM))))
      {
        NAND_LOG(NAND_LOG_ERROR, "store part_info_2 adpt pit failed\n");
    	  return -1;
      }
	}
    else
    {
      NAND_LOG(NAND_LOG_INFO, "no partition part_info_2 \n");
    }
    NAND_LOG(NAND_LOG_INFO, "store adpt pit success\n");
  }
  return 0;
}

INT32 i4SearchOrigPIT(void)
{
  int r=0;
  /*find partition table in nand*/
  if(!orig_pit)
  {
    orig_pit = kzalloc(sizeof(NEW_PART_TBL_ITEM)*PART_TBL_ITEM_NR_MAX, GFP_KERNEL);
	if(!orig_pit)
    {
       NAND_LOG(NAND_LOG_ERROR, "[i4SearchOrigPIT] allocate orig_pit memo failed.\n");
	   return -1;
    }
  }
  r = i4FindPartTbl(orig_pit, &_u4_orig_part_nr, HEAD_SIG, TAIL_SIG);
  if(r)
  {
    NAND_LOG(NAND_LOG_ERROR, "[i4SearchOrigPIT] ERROR.\n");
	return -1;
  }
  _pit_ver = i4CheckPITVer(orig_pit, HEAD_SIG);
  _u4_orig_part_nr = u4GetPartNum((UINT32)orig_pit, HEAD_SIG, TAIL_SIG);
  NAND_LOG(NAND_LOG_INFO, "[i4SearchOrigPIT] SUCCESS.\n");
  return (INT32)orig_pit;

}
INT32 i4CompareFlashPIT(UINT32 bin_pit, UINT32 len)
{
  INT32 i4Ret=0;
  UINT32 u4ItemLen=0, i=0;
  UINT32 u4CheckLen=0;

  orig_pit = kzalloc(sizeof(NEW_PART_TBL_ITEM)*PART_TBL_ITEM_NR_MAX, GFP_KERNEL);
  adpt_pit = kzalloc(sizeof(NEW_PART_TBL_ITEM)*PART_TBL_ITEM_NR_MAX, GFP_KERNEL);
  if(!orig_pit)
  {
     NAND_LOG(NAND_LOG_ERROR, "[i4CompareFlashPIT] allocate orig_pit memo failed.\n");
	 i4Ret = -1;
     goto err;
  }
  if(!adpt_pit)
  {
     NAND_LOG(NAND_LOG_ERROR, "[i4CompareFlashPIT] allocate adpt_pit memo failed.\n");
	 i4Ret = -1;
     goto err;
  }
  if((i4SearchOrigPIT())<0)
  {
    goto err;
  }
  
  if(i4CheckPITVer((UINT32*)bin_pit, HEAD_SIG) != _pit_ver)
  {
    /*the part version is differnt*/
    NAND_LOG(NAND_LOG_INFO, "[i4CompareFlashPIT] version different.\n");
    return 0;
  }
  
  if (_pit_ver == V0)
  {
	u4CheckLen = 32;
  }
  else if (_pit_ver == V1)
  {
	u4CheckLen = 40;
  }
  
  u4ItemLen = u4GetItemLen((UINT32)orig_pit, HEAD_SIG, TAIL_SIG);

  if (i4Ret == 0)
  {
    /*check whether the partition number are same or not . 
       if not , the compared result can return 'different' directly and doesn't need to compare item by item*/
    if (u4GetPartNum(bin_pit, HEAD_SIG, TAIL_SIG) != _u4_orig_part_nr)
    {
      NAND_LOG(NAND_LOG_INFO, "[i4CompareFlashPIT] partition number is different.\n");
      return 0;
    }
	/*compare item by item, from the 1st item to start
	note: the revered filed is un-necessary to compare, because the content in reserved filed is meaningless.*/
    for (i = 1 ; i <= _u4_orig_part_nr ; i++)
    {
      i4Ret = memcmp(((UCHAR *)bin_pit + (u4ItemLen*i)) , ((UCHAR *)orig_pit + (u4ItemLen*i)), u4CheckLen);
      if (i4Ret != 0)
      {
        //compare failed.
        break;
      }
    }
	if (i4Ret == 0)
	{
	  if (u4_adpt == 1)
	  {
	    NAND_LOG(NAND_LOG_INFO, "[i4CompareFlashPIT] different, need to creat adpt pit!\n");
		return 0;
	  }
	  else
	  {
	    _u4_same = 1;
	    NAND_LOG(NAND_LOG_INFO, "[i4CompareFlashPIT] same.\n");
        return 1;
	  }
	}
	else
	{
	  NAND_LOG(NAND_LOG_INFO, "[i4CompareFlashPIT] different.\n");
      return 0;
	}
  }
err:
  NAND_LOG(NAND_LOG_ERROR, "[i4CompareFlashPIT] search original pit failed.\n");
  return -1;
}
INT32 i4SetNewFlashPIT(UINT32 bin_pit)
{
  UINT32 u4Id = 0;
  UINT32 u4Offset=0, u4Size=0, u4GoodCnt=0, u4BadCnt=0, u4Blockstart=0;  
  INT32 i4Ret = -1;
  struct mtd_info *mtd=NULL;
  PART_TBL_ITEM *prdest=NULL, *prdestup=NULL, *prsource=NULL;
  UINT32 u4DestOft=0;

  NAND_LOG(NAND_LOG_DEBUG, "set new flash pit\n");
  if(!adpt_pit)
  {
    adpt_pit = kzalloc(sizeof(NEW_PART_TBL_ITEM)*PART_TBL_ITEM_NR_MAX, GFP_KERNEL);
    if(!adpt_pit)
    {
      NAND_LOG(NAND_LOG_ERROR, "[i4SearchAdptPIT] allocate adpt_pit memo failed.\n");
  	  return -1;
    }
  }
	 
  mtd = get_mtd_device(NULL, 0);
  _pit_ver = i4CheckPITVer((UINT32*) bin_pit, HEAD_SIG);
  if (_pit_ver == V0)
  {
	prdest = kzalloc(sizeof(PART_TBL_ITEM),GFP_KERNEL);
	prdestup = kzalloc(sizeof(PART_TBL_ITEM),GFP_KERNEL);
	prsource = kzalloc(sizeof(PART_TBL_ITEM),GFP_KERNEL);
  }
  else if (_pit_ver == V1)
  {
	prdest = kzalloc(sizeof(NEW_PART_TBL_ITEM),GFP_KERNEL);
	prdestup = kzalloc(sizeof(PART_TBL_ITEM),GFP_KERNEL);
    prsource = kzalloc(sizeof(PART_TBL_ITEM),GFP_KERNEL);
  }

  if (!prdest || !prdestup || !prsource)
  {
    NAND_LOG(NAND_LOG_ERROR, "[i4SetNewFlashPIT]allocate prdest memo failed\n");
    i4Ret = -1;
	goto closeall;
  }		
  u4_part_nr = u4GetPartNum(bin_pit, HEAD_SIG, TAIL_SIG);
  if (u4_part_nr == 0)
  {
    NAND_LOG(NAND_LOG_ERROR, "the size of part tbl you past is zero\n");
    i4Ret = -1;
    goto closeall;
  }

  /*scan each partition to creat a new adaptive table*/
  /*step 0: fill in the header of adapt pit, according to bin_pit*/
  i4CopyPartItem((UINT32)adpt_pit,(UINT32)bin_pit, _pit_ver);
  
  /*step 1: check 1st partition alone*/
  NAND_LOG(NAND_LOG_DEBUG, "step 1: check 1st partition\n");
  prdest = pr_get_pit_item_by_idx((UINT32)adpt_pit, 0, _pit_ver);
  prsource = pr_get_pit_item_by_idx((UINT32)bin_pit, 0, _pit_ver);
  u4Offset = prsource->u4_offset;
  u4Size = prsource->u4_size;
  u4BadCnt = 0;
  for (u4GoodCnt = 0; (u4GoodCnt * mtd->erasesize) < u4Size;)
  {
    u4Blockstart = u4Offset & (~mtd->erasesize + 1);
    u4Offset += mtd->erasesize;
	if(mtd->block_isbad(mtd, u4Blockstart))
	{
	  u4BadCnt ++;
      continue;
	}
	u4GoodCnt++;
  }
  i4CopyPartItem((UINT32)prdest,(UINT32)prsource, _pit_ver);
  if( ISUBIVOLUME(prdest->u4_info) != 4 )
  {
    prdest->u4_size = prsource->u4_size + u4BadCnt*mtd->erasesize; 
  }
  /*step 2: check the 2nd~last partition*/
  NAND_LOG(NAND_LOG_DEBUG, "step 2: check 2nd~last partition\n");
  for (u4Id = 1; u4Id < u4_part_nr; u4Id ++) //scan flash to creat adaptive 
  {
    prdest = pr_get_pit_item_by_idx((UINT32)adpt_pit, u4Id, _pit_ver);
	prsource = pr_get_pit_item_by_idx((UINT32)bin_pit, u4Id, _pit_ver);
	prdestup = pr_get_pit_item_by_idx((UINT32)adpt_pit, u4Id-1, _pit_ver);
	if( ISUBIVOLUME(prsource->u4_info) != 4 ) // it is main partition
    {
      if (ISUBIVOLUME(prdestup->u4_info) != 4)//main partition
      {
	    prdest->u4_offset = prdestup->u4_offset + prdestup->u4_size;
      }
	  else //vol partition
	  {
	    prdestup = pr_get_pit_item_by_idx((UINT32)adpt_pit, u4_get_parent_part_id(prdestup)-1, _pit_ver);
	    prdest->u4_offset = prdestup->u4_offset + prdestup->u4_size;
	  }
    }
	else //it is vol partition
	{
	  if (ISUBIVOLUME(prdestup->u4_info) != 4)//main partition
	  {
	    prdest->u4_offset = prdestup->u4_offset;
	  }
	  else // vol partition
	  {
	    prdest->u4_offset = prdestup->u4_offset + prdestup->u4_size;
	  }
	}
    u4DestOft=u4Offset = prdest->u4_offset;
	u4Size = prsource->u4_size;
	u4BadCnt = 0;
#if !CONFIG_NOR_BOOT
	for (u4GoodCnt = 0; ((u4GoodCnt * mtd->erasesize) < u4Size);)
	{
	  u4Blockstart = u4Offset & (~mtd->erasesize + 1);
	  if (u4Offset >= (mtd->size - 12*mtd->erasesize)) //reserve at least 12 block to store bbt
	  {
	    NAND_LOG(NAND_LOG_ERROR, "[i4SetNewFlashPIT]Sorry, no enough block to use for \"%s\", please check PIT\n", prsource->pc_name);
        while(1);
	  }
	  u4Offset += mtd->erasesize;
	  if(mtd->block_isbad(mtd, u4Blockstart))
	  {
	    u4BadCnt ++;
		NAND_LOG(NAND_LOG_INFO, "[i4SetNewFlashPIT]bad block found at 0x%x\n", u4Blockstart);
		continue;
	  }
	  u4GoodCnt++;
	}
#endif
	i4CopyPartItem((UINT32)prdest,(UINT32)prsource, _pit_ver);
	prdest->u4_offset = u4DestOft;
	if (ISUBIVOLUME(prsource->u4_info) == 3)//mtd-ubi partition
	{
	    u4BadCnt++;
	  //u4BadCnt = (u4BadCnt * mtd->erasesize)/(mtd->erasesize-2*mtd->writesize)+1;
	}
	if (ISUBIVOLUME(prsource->u4_info) == 4)//ubi volume, keep it's size
      u4BadCnt = 0;
	prdest->u4_size = prsource->u4_size + u4BadCnt*mtd->erasesize; 
 
  }
  /*step 3: add the head&tail as adaptive pit, the head&tail style is same the binary pit, because it will be searched by other modules*/
  NAND_LOG(NAND_LOG_DEBUG, "step 3: set adpt pit head&tail\n");
  u4SetPartHead((UINT32)adpt_pit, _pit_ver, ADPT_HEAD_SIG);
  u4SetPartTail((UINT32)adpt_pit, u4_part_nr, _pit_ver, ADPT_TAIL_SIG);
  #if 0
  /*re-install mtd with new adaptive PIT */
  NAND_LOG(NAND_LOG_DEBUG, "step 5: re-init adpt mtd\n");
  if (i4SetMtdTbl((UINT32)adpt_pit, u4_part_nr))
  {
    i4Ret = -1;
    NAND_LOG(NAND_LOG_ERROR, "mount adaptive mtd failed\n");
	goto closeall;
  }
   
  NAND_LOG(NAND_LOG_DEBUG, "step 6: store adpt pit in part_info_1 partition\n");
  u4Id = get_part_id_by_name_new((UINT32)adpt_pit, "part_info_1", _pit_ver);
  /*store adaptive PIT part_info_1 parition.*/
   if (i4NFBPartitionWrite(u4Id, ADPT_PIT_OFFSET,(UINT32)adpt_pit,
  	                 (UINT32)((u4_part_nr+1) * sizeof(NEW_PART_TBL_ITEM))))
  {
    i4Ret = -1;
    NAND_LOG(NAND_LOG_ERROR, "store part_info_1 adpt pit failed\n");
	goto closeall;
  }
  NAND_LOG(NAND_LOG_DEBUG, "step 7: store adpt pit in part_info_2 partition\n");
  u4Id = get_part_id_by_name_new((UINT32)adpt_pit, "part_info_2", _pit_ver);
  /*store adaptive PIT to part_info_2 parition.*/
  if (i4NFBPartitionWrite(u4Id,ADPT_PIT_OFFSET,(UINT32)adpt_pit,
  	                 (UINT32)((u4_part_nr+1) * sizeof(NEW_PART_TBL_ITEM))))
  {
    i4Ret = -1;
    NAND_LOG(NAND_LOG_ERROR, "store part_info_2 adpt pit failed\n");
	goto closeall;
  }
  #else
  //if(i4StoreAdptPit())
  if(i4StoreAdptPit(bin_pit))
  {
	i4Ret = -1;
	goto closeall;
  }
  #endif
  i4Ret = 0;
  
closeall:
  if (i4Ret<0)
  {
    NAND_LOG(NAND_LOG_ERROR, "set new flash pit failed\n");
  }
  else
  {
    NAND_LOG(NAND_LOG_DEBUG, "set new flash pit success\n");
  }
  return i4Ret;
}
#endif
INT32 i4NFBPartitionRead(UINT32 u4DevId, UINT32 u8Offset, UINT32 u4MemPtr, UINT32 u4MemLen)
{
    UINT32 start_addr=0;        // start address
    UINT32 ofs=0;
    UINT32 blockstart = 1;
    int  pagelen, badblock = 0;
    UINT32 u4RdByteCnt = 0;
	UINT32 u4RdLen = 0;
    INT32 i4Ret = 0;
	UINT8 *pu1MemBuf = (UINT8*)u4MemPtr;
	struct mtd_info *mtd=NULL;
    NAND_LOG(NAND_LOG_INFO, "[part read]u4DevId = %d, u8Offset = 0x%x, u4MemPtr = 0x%x, u4MemLen = 0x%x \n", 
		    u4DevId, u8Offset, u4MemPtr, u4MemLen);

	ASSERT(pu1MemBuf != NULL);
	mtd = get_mtd_device(NULL, u4DevId);

    /* Initialize page size */
    pagelen = mtd->writesize;
    start_addr = u8Offset;        // start address
    
    // Check, if length fits into device
    if ( ((u4MemLen / pagelen) * mtd->writesize) > (mtd->size - u8Offset)) 
    {
        NAND_LOG(NAND_LOG_ERROR, "[part write] Image %d bytes, NAND page %d bytes, OOB area %u bytes, device size %u bytes\n",
                 u4MemLen, pagelen, mtd->oobsize, mtd->size);
        NAND_LOG(NAND_LOG_ERROR, "[part write] Error : Input file can not fit into device\n");        
        i4Ret = -1;
        goto closeall;
    }
    #if SONY_GET_REGISTRY
    if (b_get_registry  && (u4DevId == 0))
    {
      ofs = get_part_addr_by_name("fast_init_param");
    }
    else
    {
    ofs = 0;
    }
    #else
    ofs = 0;
    #endif
    /* Check all the blocks from the begining of the partition for bad blocks */
    while ( (ofs < (start_addr & (~mtd->erasesize + 1))) && (ofs < mtd->size))
    {
        if ((i4Ret = mtd->block_isbad(mtd, ofs)) < 0) 
        {
            NAND_LOG(NAND_LOG_ERROR, "[part read]Error check bad block\n");
            i4Ret = -1;
            goto closeall;
        }
        
        if (i4Ret == 1) 
        {
            start_addr = start_addr + mtd->erasesize;
            NAND_LOG(NAND_LOG_INFO, "[part read]=> Bad block at partition_%d, 0x%x will be skipped\n", u4DevId, ofs);
        }
        else
        {
            NAND_LOG (NAND_LOG_DEBUG, "[part read]=> Good block\n");
        }

        ofs +=  mtd->erasesize;
    }

    for (ofs = start_addr; ofs < mtd->size && u4RdByteCnt < u4MemLen ;) 
    {
        // new eraseblock , check for bad block
        if (blockstart != (ofs & (~mtd->erasesize + 1))) 
        {
            blockstart = ofs & (~mtd->erasesize + 1);
		    //NAND_LOG("[part read] partition_%d - check block 0x%X\n", u4DevId, blockstart);
			if ((badblock = mtd->block_isbad(mtd, blockstart)) < 0)
            {
                NAND_LOG(NAND_LOG_ERROR, "[part read]Error check bad block\n");
                goto closeall;
            }
        }

        if (badblock) 
        {
            // skip the bad block
            ofs += mtd->erasesize;
			badblock = 0;
            NAND_LOG(NAND_LOG_INFO, "[part read] partition_%d - bad block found at : 0x%X\n", u4DevId, blockstart);
            continue;
        }
        else 
        {
            /* Read page data and exit on failure */
			pagelen = mtd->writesize;
			if (((ofs % mtd->writesize) != 0) && ((u4MemLen - u4RdByteCnt) >= mtd->writesize))
            { /*if ofs is not page aligned, and the read length is more than one page ==> 
                        then, read the 1st block data to enable 2nd block data page aligned, it is important if meet the next block is bad*/
              pagelen = mtd->writesize - (ofs % mtd->writesize);
            }
			else if ((u4MemLen - u4RdByteCnt) < mtd->writesize)
			{
			  pagelen = (u4MemLen - u4RdByteCnt);
			}

            if ((i4Ret = mtd->read(mtd, ofs, pagelen, &u4RdLen, (u_char *)(pu1MemBuf + u4RdByteCnt)))<0)
            {
                NAND_LOG(NAND_LOG_ERROR, "[part read]Page read error occur at : 0x%x\n", ofs);
                goto closeall;
            }
            ofs += u4RdLen;
            u4RdByteCnt += (UINT32)u4RdLen;
        }

    }

    if (u4RdByteCnt < u4MemLen)
    {
        NAND_LOG(NAND_LOG_ERROR, "[part read]Can't read enough data, u4RdByteCnt = 0x%x, u4MemLen = 0x%x\n", u4RdByteCnt, u4MemLen);        
        goto closeall;
    }
    /* Exit happy */
    return i4Ret;

closeall:    
    i4Ret = -1;    
    return i4Ret;
}
#define PART_WRITE_VERIFY 0
#define ERASE_FAIL_RECOVER 1
INT32 i4NFBPartitionWrite(UINT32 u4DevId, UINT32 u8Offset , UINT32 u4MemPtr, UINT32 u4MemLen)
{
    int imglen = u4MemLen, pagelen=0;
	size_t retlen=0;
    UINT8 baderaseblock = FALSE;
    int blockstart = -1;
	loff_t offs=0;    
    int readlen=0, writelen=0, readcnt=0, headoffset=0, tailoffset=0, headsize=0, tailsize=0, headend=0, tailstart=0;
    UINT32 mtdoffset=0, writeoffset=0, tempoffset=0;
    INT32 i4Ret = 0;
    UINT32 u4WtByteCnt = 0;
	UINT8  *pu1MemBuf = (UINT8*)u4MemPtr;
	u_char *pu1TempMemBuf = NULL;
	struct erase_info *erase=NULL;
	struct mtd_info *mtd;
	u_char *pu1BlockMemBuf = NULL;
	BOOL fgBlockDataBack = FALSE;
	
    erase = kzalloc(sizeof(struct erase_info),GFP_KERNEL);
	if (!erase)
	{
	  NAND_LOG(NAND_LOG_ERROR, "[part write]allocate erase info memo failed\n");
	  return -ENOMEM;
	}
	
    ASSERT(pu1MemBuf != NULL);
    mtd = get_mtd_device(NULL, u4DevId);
   
    mtdoffset = u8Offset;        // start address
    
    NAND_LOG(NAND_LOG_INFO, "[part write]u4DevId = %d, u8Offset = 0x%x, u4MemPtr = 0x%x, u4MemLen = 0x%x, name = %s\n", 
		    u4DevId, u8Offset, u4MemPtr, u4MemLen, mtd->name);
    
    pagelen = mtd->writesize;

    headsize = 0;
	tailsize = 0;
	
    /*
     * For the standard input case, the input size is merely an
     * invariant placeholder and is set to the write page
     * size. Otherwise, just use the input file size.
     *
     * TODO: Add support for the -l,--length=length option (see
     * previous discussion by Tommi Airikka <tommi.airikka@ericsson.com> at
     * <http://lists.infradead.org/pipermail/linux-mtd/2008-September/
     * 022913.html>
     */

    // Check, if length fits into device
    if ( ((imglen / pagelen) * mtd->writesize) > (mtd->size - mtdoffset)) 
    {
        NAND_LOG(NAND_LOG_ERROR, "[part write] Image %d bytes, NAND page %d bytes, OOB area %u bytes, device size %u bytes\n",
                 imglen, pagelen, mtd->oobsize, mtd->size);
        NAND_LOG(NAND_LOG_ERROR, "[part write] Error : Input file can not fit into device\n");        
        i4Ret = -1;
        goto closeall;
    }
    pu1TempMemBuf = kzalloc(MAX_PAGE_SIZE,GFP_KERNEL);
    ASSERT(pu1TempMemBuf != NULL);

	pu1BlockMemBuf = kzalloc(mtd->erasesize,GFP_KERNEL);
    ASSERT(pu1BlockMemBuf != NULL);
#if SONY_GET_REGISTRY
    if (b_get_registry  && (u4DevId == 0))
    {
       offs = get_part_addr_by_name("fast_init_param");
    }
    else
    {
        offs = 0;
    }
#else
    offs = 0;
#endif

    /* Check all the blocks in an erase block for bad blocks */
    while ( (offs < (mtdoffset & (~mtd->erasesize + 1))) && (offs < mtd->size))
    {
        if ((i4Ret = mtd->block_isbad(mtd, offs)) < 0) 
        {
            NAND_LOG(NAND_LOG_ERROR, "[part write]Error check bad block\n");
            i4Ret = -1;
            goto closeall;
        }
        
        if (i4Ret == 1) 
        {
            NAND_LOG(NAND_LOG_INFO, "[part write]=> Bad block at partition_%d, offset 0x%x \n", (int)u4DevId, (uint32_t)offs);
            mtdoffset = mtdoffset + mtd->erasesize;
        }
        else
        {
            NAND_LOG (NAND_LOG_DEBUG, "[part write]=> Good block\n");
        }

        offs +=  mtd->erasesize;
    }

    /*
     * Get data from input and write to the device while there is
     * still input to read and we are still within the device
     * bounds. Note that in the case of standard input, the input
     * length is simply a quasi-boolean flag whose values are page
     * length or zero.
     */    
    while ((imglen || headsize || tailsize) && (mtdoffset < mtd->size)) 
    {
        // new eraseblock , check for bad block(s)
        // Stay in the loop to be sure if the mtdoffset changes because
        // of a bad block, that the next block that will be written to
        // is also checked. Thus avoiding errors if the block(s) after the
        // skipped block(s) is also bad (number of blocks depending on
        // the blockalign
        while (blockstart != (mtdoffset & (~mtd->erasesize + 1))) 
        {
		    headoffset = 0;
			tailoffset = 0;
			headsize = 0;
			tailsize = 0;
			headend = blockstart;
            blockstart = mtdoffset & (~mtd->erasesize + 1);
			tailstart = blockstart + mtd->erasesize;
            offs = blockstart;
            baderaseblock = FALSE;
            NAND_LOG(NAND_LOG_INFO, "[part write] Try to writing data to block %d at offset 0x%x\n",
                         blockstart / mtd->erasesize, blockstart);

            /* Check all the blocks in an erase block for bad blocks */
            do 
            {
                if ((i4Ret = mtd->block_isbad(mtd, offs)) < 0) 
                {
                    NAND_LOG(NAND_LOG_ERROR, "[part write]Error check bad block\n");
                    i4Ret = -1;
                    goto closeall;
                }
                
                if (i4Ret == 1) 
                {
                    baderaseblock = TRUE;
                    NAND_LOG(NAND_LOG_INFO, "[part write] => Bad block at partition_%x, offset 0x%x, will be skipped\n", u4DevId, blockstart);
                }
                else
                {
                    NAND_LOG (NAND_LOG_DEBUG, "[part write]=> Good block\n");
                }

                if (baderaseblock) 
                {
                    mtdoffset = mtdoffset + mtd->erasesize;
                }
                offs +=  mtd->erasesize;
            } while ( offs < blockstart + mtd->erasesize );

            if (!baderaseblock)
            {
                // Erase the erase-block before write data into the block  
               // wait_queue_head_t waitq;
			   // DECLARE_WAITQUEUE(wait, current);
			   // init_waitqueue_head(&waitq);
                headsize = mtdoffset - blockstart;
				headend = mtdoffset;
				if((mtdoffset+imglen) >= (blockstart+mtd->erasesize))
				{
				  tailsize = 0;  
				}
				else
				{
				  tailsize = (blockstart+mtd->erasesize) - (mtdoffset+imglen);
				} 
				tailoffset = mtd->erasesize - tailsize;
				tailstart = blockstart + tailoffset;
				if((headsize > 0) || (tailsize > 0))
				{
				   tempoffset = blockstart;
				   
                   if(!fgBlockDataBack)
				   {
    				   readlen = 0;
    				   memset(pu1BlockMemBuf, 0xFF, mtd->erasesize);
                       for (offs = blockstart; offs < mtd->size && readlen < mtd->erasesize ; offs+=pagelen) 
                       {
                               /* Read page data and exit on failure */
                               if (mtd->read(mtd, offs, pagelen, &readcnt, (u_char *)(pu1BlockMemBuf + readlen)))
                               {
                                   NAND_LOG(NAND_LOG_ERROR, "[part read]Page read error occur at : 0x%x\n", offs);
                                   goto closeall;
                               }
    
    			               if(readcnt != pagelen)
    			               {
    			                   NAND_LOG(NAND_LOG_ERROR, "[part read]Page read error (not): readcnt = 0x%x\n", readcnt);
                                   goto closeall;
    			               }
    						   else
    			               {
                                   readlen += readcnt;
    			               }
                       }
				   }
                   else
                   {
				       fgBlockDataBack = FALSE;
                   }
				}
				
                erase->addr = blockstart;
                erase->len = mtd->erasesize;
				erase->mtd = mtd;
				//erase->callback = mtdchar_erase_callback;
			   // erase->priv = (unsigned long)&waitq;
				
                //NAND_LOG("[part write] Erasing this block before write from 0x%08lX-0x%08lX\n", (long)erase->addr, (long)erase->addr+erase->len-1);
                i4Ret = mtd->erase(mtd, erase);
				/*if (!i4Ret) 
				{
    				set_current_state(TASK_UNINTERRUPTIBLE);
    				add_wait_queue(&waitq, &wait);
    				if (erase->state != MTD_ERASE_DONE &&
    				    erase->state != MTD_ERASE_FAILED)
    					schedule();
    				remove_wait_queue(&waitq, &wait);
    				set_current_state(TASK_RUNNING);
    
    				i4Ret = (erase->state == MTD_ERASE_FAILED)?-5:0;
			    }*/
				if (i4Ret != 0)
                {
                    NAND_LOG(NAND_LOG_ERROR, "[part write] block erase failed !!\n");
					#if ERASE_FAIL_RECOVER  //erase failed, not to exit, just mark this block as bad and continue to write to the next block anyway
					offs = blockstart;
					if ((i4Ret = mtd->block_markbad(mtd,offs)) != 0)
					{
					  NAND_LOG(NAND_LOG_ERROR, "[part write] mark bad failed !!");
                    goto closeall;
                }
                    NAND_LOG(NAND_LOG_ERROR, "[NAND] bad memory block was found [NO = 0x%x]\n",(unsigned int)offs);
                    mtdoffset = mtdoffset + mtd->erasesize;					
					continue;
					#else
                    goto closeall;
					#endif
                }
            }
        }

        writelen = 0;
		readlen = 0;
		readcnt = 0;
        memset(pu1TempMemBuf, 0xFF, mtd->writesize);
		
        if(headsize > 0)
        {
            writelen = mtd->writesize;

            if(headsize >= writelen)
            {
               headsize -= writelen;
            }
			else
			{
			   writelen = headsize;
			   headsize = 0;
			}
            writeoffset = tempoffset;
			tempoffset += writelen; 

			memcpy(pu1TempMemBuf, (pu1BlockMemBuf+headoffset), writelen);

			headoffset += writelen;
        }

		if((imglen > 0) && (writelen < mtd->writesize))
		{
		    readlen = mtd->writesize - writelen;
        if (imglen < readlen)
        {
            readlen = imglen;
        }
			memcpy((pu1TempMemBuf+writelen), (pu1MemBuf+u4WtByteCnt), readlen);

            if(writelen == 0)
            {
                writeoffset = mtdoffset;
            }

			imglen -= readlen;
            u4WtByteCnt += readlen;
			mtdoffset +=readlen;
		}

		if((tailsize > 0)&&((readlen + writelen) < mtd->writesize))
		{
		    readcnt = mtd->writesize - (readlen + writelen);
			
            if(tailsize >= readcnt)
            {
               tailsize -= readcnt;
            }
			else
			{
			   readcnt = tailsize;
			   tailsize = 0;
			}

			if((readlen + writelen) == 0)
			{
			   writeoffset = mtdoffset; 
			}

			memcpy((pu1TempMemBuf+(readlen + writelen)), (pu1BlockMemBuf+tailoffset), readcnt);

            mtdoffset += readcnt;
			tailoffset += readcnt;
		}
		
        /* Write out the Page data */
        if (mtd->write(mtd, writeoffset, mtd->writesize, &retlen, (u_char *)(pu1TempMemBuf)))
        {
            int rewind_blocks;
          //  struct erase_info *erase;

            NAND_LOG(NAND_LOG_ERROR, "[part write] mtd->write error at offset 0x%x\n", writeoffset);

            /* Must rewind to blockstart if we can */
            rewind_blocks = (writeoffset - blockstart) / mtd->writesize; /* Not including the one we just attempted */
            
           // wait_queue_head_t waitq;
			//DECLARE_WAITQUEUE(wait, current);
			//init_waitqueue_head(&waitq);
				
            erase->addr = blockstart;
            erase->len = mtd->erasesize;
			erase->mtd = mtd;
			//erase->callback = mtdchar_erase_callback;
			//erase->priv = (unsigned long)&waitq;
				
            NAND_LOG(NAND_LOG_ERROR, "[NAND] Erasing failed write from 0x%08lX-0x%08lX\n", 
                (long)erase->addr, (long)(erase->addr+erase->len-1));
                
			i4Ret = mtd->erase(mtd, erase);
			/*if (!i4Ret) 
			{
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&waitq, &wait);
				if (erase->state != MTD_ERASE_DONE &&
					erase->state != MTD_ERASE_FAILED)
					schedule();
				remove_wait_queue(&waitq, &wait);
				set_current_state(TASK_RUNNING);
					
				i4Ret = (erase->state == MTD_ERASE_FAILED)?-5:0;
		    }*/
			if (i4Ret != 0)
			{
				NAND_LOG(NAND_LOG_ERROR, "[part write] block erase failed-1 !!");
                #if !ERASE_FAIL_RECOVER  //erase failed, not to exit, just mark this block as bad and continue to write to the next block anyway
			    goto closeall;
                #endif			
			}


            // Mark bad block
            {
                loff_t bad_addr = writeoffset & (~mtd->erasesize + 1);
                
                NAND_LOG(NAND_LOG_INFO, "[part write]Marking block at 0x%08lX bad\n", (long)bad_addr);
                
                if (mtd->block_markbad(mtd, bad_addr)) 
                {
                    NAND_LOG(NAND_LOG_ERROR, "[part write]block mark bad block Error !!\n");                    
                    /* But continue anyway */
                }
            }
            
            if(mtdoffset <= headend)
            {
                writelen = 0;
            }
			else if(mtdoffset <= tailstart)
			{
			    writelen = mtdoffset - headend;
			}
			else 
			{
			    writelen = tailstart- headend;
            }
        
            mtdoffset = blockstart + mtd->erasesize;			
            imglen += writelen;
            u4WtByteCnt -= writelen;

			fgBlockDataBack = TRUE;
        
            continue;
        }
    }

closeall:
    kfree(erase);
	kfree(pu1TempMemBuf);
	kfree(pu1BlockMemBuf);
    if (imglen > 0) 
    {
        NAND_LOG(NAND_LOG_ERROR, "[part write]Data was only partially written due to error, imglen = 0x%x\n", imglen);
        i4Ret = -1;
    }
#if PART_WRITE_VERIFY
    // Read back for verify
    if (i4Ret == 0)
    {
          UINT8 *pu1MemVerfBuf = NULL;
          pu1MemVerfBuf = kzalloc(u4MemLen, GFP_KERNEL);
          if (!pu1MemVerfBuf)
          {
            NAND_LOG(NAND_LOG_ERROR, "[part write] Memo kzalloc failed\n");
		    return -ENOMEM;
          }

          NAND_LOG(NAND_LOG_INFO, "[part write]Read back verify at offset 0x%x, len 0x%x ... \n", u8Offset, u4MemLen);
		  i4Ret = i4NFBPartitionRead(u4DevId, u8Offset, (UINT32)pu1MemVerfBuf, u4MemLen);
          if (i4Ret != 0)
          {
              NAND_LOG(NAND_LOG_ERROR, "[part write] Read back failed !! Fetal Error\n");
              ASSERT(i4Ret == 0);
          }

          i4Ret = memcmp(pu1MemBuf, pu1MemVerfBuf, u4MemLen);
          
          if (i4Ret == 0)
          {
              NAND_LOG(NAND_LOG_INFO, "[part write]Read back compare check OK ^^\n");
          }
          else
          {
              NAND_LOG(NAND_LOG_ERROR, "[part write]Read back compare failed !! Fetal Error\n");
              ASSERT(i4Ret == 0);
          }
          
          kfree(pu1MemVerfBuf);
    }
#endif    
    /* Return happy */
    return i4Ret;
}

#ifdef CFG_UBOOT_CUSTOM_JSN
#define DRV_CFG_CEC_ON_ADDR_SIZE          1
#define DRV_CFG_VFD_DIMMER_ADDR_SIZE      1
#define DRV_CFG_COMPONENT_RESOLUTION_SIZE 1
#define DRV_CFG_HDMI_RESOLUTION_SIZE      1
#define DRV_CFG_CVBS_RESOLUTION_SIZE      1
#define DRV_CFG_CONNECTION_TYPE_SIZE      1
#define DRV_CFG_CHILD_LOCK_SIZE           1
#define DRV_CFG_QUICK_START_SIZE          1
#define DRV_CFG_REFRESH_BOOT_SIZE         1
#define DRV_CFG_RTC_TRIM_ADDR_SIZE        1
#define DRV_CFG_HDMI_DEBUG_ADDR_SIZE      4
#define DRV_CFG_PRODUCT_ID_ADDR_SIZE      20
#define DRV_CFG_FACT_STEP_ADDR_SIZE       2
#define DRV_CFG_BAD_STEP_ADDR_SIZE        20
#define DRV_CFG_PWB_ID_ADDR_SIZE          15
#define DRV_CFG_TV_ID_ADDR_SIZE           32
#define DRV_CFG_D_PWR_MODE_ADDR_SIZE      1
#define DRV_CFG_HDMI_LOG_ADDR_SIZE        1
#define DRV_CFG_CEC_LOG_ADDR_SIZE         1
#define DRV_CFG_IFCON_LOG_ADDR_SIZE       1
#define DRV_CFG_CD_IOP_ADDR_SIZE          2
#define DRV_CFG_DVD_IOP_ADDR_SIZE         2
#define DRV_CFG_BD_IOP_ADDR_SIZE          2
#define DRV_CFG_HDMI_KSV_ADDR_SIZE        11
#define DRV_CFG_MAC_ID_ADDR_SIZE          12
#define DRV_CFG_BDP_ID_ADDR_SIZE          16
#define DRV_CFG_AACS_ID_ADDR_SIZE         8
#define DRV_CFG_CPRM_ID_ADDR_SIZE         16
#define DRV_CFG_HDCP_ID_ADDR_SIZE         16
#define DRV_CFG_WMDRM_ID_ADDR_SIZE        96
#define DRV_CFG_MODEL_ADDR_SIZE           16
#define DRV_CFG_DESTINATION_SIZE          8
#define DRV_CFG_BD_REG_CODE_SIZE          1
#define DRV_CFG_DVD_REG_CODE_SIZE         1
#define DRV_CFG_REG_FREE_FLAG_SIZE        1
#define DRV_CFG_UPDT_TYPE_SIZE            1
#define DRV_CFG_UPDT_MEDIA_SIZE           1
#define DRV_CFG_UPDT_STEP_SIZE            1
#define DRV_CFG_UPDT_RETRY_SIZE           1
#define DRV_CFG_UPDT_URL_TYPE_SIZE        1
#define DRV_CFG_BD_PLUS_CHK_SIZE          1
#define DRV_CFG_AWM_DETECT_SIZE           1
#if 0//msb9g, nms/dms new setting.
#define DRV_CFG_DMS_OUTPUT_SIZE           1
#define DRV_CFG_NMS_OUTPUT_SIZE           1
#define DRV_CFG_NMS_INPUT_SIZE            1
#endif
#define DRV_CFG_HDMI_PA_SIZE              2
#define DRV_CFG_CEC_LA_SIZE               1
#define DRV_CFG_IFCON_WDTM_FLAG_SIZE      1

#define DRV_CFG_D_DIAG_MODE_SIZE          1
#define DRV_CFG_DTCP_ID_SIZE              16
#define DRV_CFG_MRLNBB_ID_SIZE            32
#define DRV_CFG_WL_SERIAL_SIZE            20
#define DRV_CFG_WL_MAC_SIZE               20
#define DRV_CFG_WL_HW_VERSION_SIZE        20
#define DRV_CFG_WL_WEP_KEY_SIZE           26
#define DRV_CFG_WL_SSID2_SIZE             32
#define DRV_CFG_WL_SSID5_SIZE             32
#define DRV_CFG_WL_IP_SIZE                20
#define DRV_CFG_WL_NETMASK_SIZE           20
#define DRV_CFG_WL_BROADCAST_SIZE         20
#define DRV_CFG_WL_PMK_SIZE               64
#define DRV_CFG_WL_COUNT_SIZE             1
#define DRV_CFG_WL_PACKETSIZE_SIZE        3
#define DRV_CFG_WL_IPADDR2_SIZE           20
#define DRV_CFG_WL_IPADDR5_SIZE           20
#define DRV_CFG_WL_RSSI2_U_SIZE           4
#define DRV_CFG_WL_RSSI2_L_SIZE           4
#define DRV_CFG_WL_RSSI5_U_SIZE           4
#define DRV_CFG_WL_RSSI5_L_SIZE           4
#define DRV_CFG_WL_RSSI_DATA2_SIZE        4
#define DRV_CFG_WL_RSSI_DATA5_SIZE        4
#define DRV_CFG_IF_MODEL_SIZE             16
#define DRV_CFG_IF_DEST_SIZE              16
#define DRV_CFG_ICE_FLAG_SIZE             1
#define DRV_CFG_DEMO_MODE_FLAG_SIZE       1
#define DRV_CFG_EXT_HDMI_KSV_SIZE		  11
#define DRV_CFG_RX_HDMI_KSV_SIZE	      11
#define DRV_CFG_HDD_SERIAL_SIZE			  32
#define DRV_CFG_HDD_REVISION_SIZE		  32
#define DRV_CFG_HDD_MODEL_SIZE		  	  32
#define DRV_CFG_TC_KEY_ID_SIZE		  	  16
#define DRV_CFG_HDCP_RX_ID_SIZE		 	  16
#define DRV_CFG_HDD_CAP_SIZE			  8
#define DRV_CFG_TC_RPD_SIZE			      16
#define DRV_CFG_TC_SERVICE_FLAG_SIZE       1
#define DRV_CFG_TC_REPLACE_FLAG_SIZE       1
#define DRV_CFG_MAGIC_SIZE                64
#define DRV_CFG_DOWNSTREAM_EDID_SIZE       1
#define DRV_CFG_FORCE_UPG_SIZE             1
#define DRV_CFG_SERVICE_MODE_SIZE          1
#define DRV_CFG_USBCLI_MODE_SIZE           1
#define DRV_CFG_IR_DIAG_MODE_FLAG_SIZE     1 

#if 1  // ZLL_CEC_TEST
#define DRV_CFG_HDMI_PA2_SIZE              2
#define DRV_CFG_CEC_LA2_SIZE               1
#endif // ZLL_CEC_TEST

#if 1//def SUPPORT_WAKE_ON_LAN
#define DRV_CFG_WOL_SIZE                   1
#endif

#define DRV_CFG_BDP_MONITOR_SIZE           1

#if 1  // SACD_LAYER_TEST
#define DRV_CFG_SACD_LAYER_SIZE            1
#endif // SACD_LAYER_TEST

#define DRV_CFG_LAST_FUNCTION_TYPE_SIZE   1 
#define DRV_CFG_TV_TYPE_SIZE              1
#define DRV_CFG_SCREEN_FORMAT_SIZE        1
#define DRV_CFG_DVD_ASPECT_RATIO_SIZE     1
#define DRV_CFG_COLOR_SPACE_SIZE          1
#define DRV_CFG_HDMI_DEEP_COLOR_SIZE      1
#define DRV_CFG_AV_SEPARATION_OUTPUT_SIZE 1

#if 1  // CX_IFCON_VER
#define DRV_CFG_IFCON_VER_DATA_SIZE        2
#endif // CX_IFCON_VER

#define DRV_CFG_MASS_PRODUCT_SIZE         1
#define DRV_CFG_PROGRESS_SKIP_SIZE        1
#define DRV_CFG_HDMI_EDID_ADDR_SIZE        1
#define DRV_CFG_HDMI2_LOG_ADDR_SIZE        1
#define DRV_CFG_HDMI2_EDID_ADDR_SIZE       1

#define DRV_CFG_UPDATE_MAC_ADDR_2_SIZE     1

#if 1 // ZLL_RUN_ON_REFER_BOARD
#define DRV_CFG_RUN_ON_REFER_BOARD_FLAG_SIZE  1
#endif // ZLL_RUN_ON_REFER_BOARD


#if (CONFIG_CHIP_VER_CURR == CONFIG_CHIP_VER_MT8561 || CONFIG_CHIP_VER_CURR == CONFIG_CHIP_VER_MT8563)
#define JIG_MODE0       PIN_LED1      //JIG mode flag input 0
#define JIG_MODE1       PIN_LED0      //JIG mode flag input 1
#else
#define JIG_MODE0       PIN_GPIO4      //JIG mode flag input 0
#define JIG_MODE1       PIN_GPIO5      //JIG mode flag input 1
#endif

/* 
    If you want to change the offset or size of any item , for example,
delete any item from the table , or insert any item into the table 
( not at the end ), please check "drv_misc.h" ,whose directory 
is "/BDP_Generic/src/driver/inc/" , to make it consistent .

    To make it easy to find , please define the size of the item
at the end of current "table " as well .

   --------- Added by liangliang  -------------

*/
//copy from drv_misc.h for MSB9G.
#define MISC_SECTOR_SZ                 512
#define MISC_MAC_ADDR_SIZE             7
#define MISC_RTC_TRIM_ADDR_SIZE        1
#define MISC_HDMI_DEBUG_ADDR_SIZE      4
#define MISC_CUS_MODEL_SIZE            1
#define MISC_CUS_SUB_MODEL_SIZE        4
#define MISC_CUS_MODEL_STR_SIZE        16
#define MISC_CUS_DEST_STR_SIZE         8
#define MISC_CUS_IF_MODEL_SIZE         16
#define MISC_CUS_IF_DEST_SIZE          16
#define MISC_CUS_BDROM_REGIONCODE_SIZE 1
#define MISC_CUS_DVDROM_REGIONCODE_SIZE 1
#define MISC_CUS_DVDROM_TEST_FLAG_SIZE 1
#define MISC_CUS_BDPLUS_TEST_FLAG_SIZE 1
#define MISC_CUS_AWM_TEST_FLAG_SIZE    1
#define MISC_CUS_PRODUCT_ID_SIZE       20
#define MISC_CUS_PWB_ID_SIZE           15
#if 1//msb9g, nms/dms new setting.
#define MISC_CUS_DMS_OUTPUT_SIZE           1
#define MISC_CUS_NMS_OUTPUT_SIZE           1
#define MISC_CUS_NMS_INPUT_SIZE            1
#endif

/* offset definition => sum of the previous offset and size */
typedef enum 
{
    MISC_MAC_ADDR_OFFSET = 0,                                                           /* size = 7, 6 byte + one byte checksum */     
    MISC_RTC_TRIM_OFFSET = (MISC_MAC_ADDR_OFFSET + MISC_MAC_ADDR_SIZE),                 /* size = 1 */
    MISC_HDMI_DEBUG_OFFSET = (MISC_RTC_TRIM_OFFSET + MISC_RTC_TRIM_ADDR_SIZE),          /* size = 4 */
    MISC_CUS_MODEL_OFFSET = (MISC_HDMI_DEBUG_OFFSET + MISC_HDMI_DEBUG_ADDR_SIZE),       /* size = 1 */
    MISC_CUS_SUB_MODEL_OFFSET = (MISC_CUS_MODEL_OFFSET + MISC_CUS_MODEL_SIZE),          /* size = 4 */
    MISC_CUS_MODEL_STR_OFFSET = (MISC_CUS_SUB_MODEL_OFFSET + MISC_CUS_SUB_MODEL_SIZE),  /* size = 16 */
    MISC_CUS_DEST_STR_OFFSET = (MISC_CUS_MODEL_STR_OFFSET + MISC_CUS_MODEL_STR_SIZE),   /* size = 8 */
    MISC_CUS_IF_MODEL_OFFSET = (MISC_CUS_DEST_STR_OFFSET + MISC_CUS_DEST_STR_SIZE),   /* 16 */
    MISC_CUS_IF_DEST_OFFSET = (MISC_CUS_IF_MODEL_OFFSET + MISC_CUS_IF_MODEL_SIZE),    /* 16 */
    MISC_CUS_BDROM_REGIONCODE_OFFSET = (MISC_CUS_IF_DEST_OFFSET + MISC_CUS_IF_DEST_SIZE),   /* 1 */
    MISC_CUS_DVDROM_REGIONCODE_OFFSET = (MISC_CUS_BDROM_REGIONCODE_OFFSET + MISC_CUS_BDROM_REGIONCODE_SIZE),   /* 1 */
    MISC_CUS_DVDROM_TEST_FLAG_OFFSET = (MISC_CUS_DVDROM_REGIONCODE_OFFSET + MISC_CUS_DVDROM_REGIONCODE_SIZE),   /* 1 */
    MISC_CUS_BDPLUS_TEST_FLAG_OFFSET = (MISC_CUS_DVDROM_TEST_FLAG_OFFSET + MISC_CUS_DVDROM_TEST_FLAG_SIZE),   /* 1 */
    MISC_CUS_AWM_TEST_FLAG_OFFSET = (MISC_CUS_BDPLUS_TEST_FLAG_OFFSET + MISC_CUS_BDPLUS_TEST_FLAG_SIZE),   /* 1 */
    MISC_CUS_PRODUCT_ID_OFFSET = (MISC_CUS_AWM_TEST_FLAG_OFFSET + MISC_CUS_AWM_TEST_FLAG_SIZE),   /* 20 */
    MISC_CUS_PWB_ID_OFFSET = (MISC_CUS_PRODUCT_ID_OFFSET + MISC_CUS_PRODUCT_ID_SIZE),   /* 15 */
#if 1//msb9g, nms/dms new setting.
    MISC_CUS_DMS_OUTPUT_OFFSET = 0xA0, /* diag mode serial output */
    MISC_CUS_NMS_OUTPUT_OFFSET = (MISC_CUS_DMS_OUTPUT_OFFSET + MISC_CUS_DMS_OUTPUT_SIZE), /* normal mode serial output */
    MISC_CUS_NMS_INPUT_OFFSET = (MISC_CUS_NMS_OUTPUT_OFFSET + MISC_CUS_NMS_OUTPUT_SIZE), /* normal mode serial input */
    MISC_OFFSET_MAX = (MISC_CUS_NMS_INPUT_OFFSET + MISC_CUS_NMS_INPUT_SIZE)
#else
    MISC_OFFSET_MAX = (MISC_CUS_PWB_ID_OFFSET + MISC_CUS_PWB_ID_SIZE)
#endif
} MISC_PARTITION_IDX_T;
//end of copy from drv_misc.h

typedef enum 
{
    DRV_CFG_CEC_ON_OFFSET = 0,  /* cec on/off flag */
    DRV_CFG_VFD_DIMMER_OFFSET = (DRV_CFG_CEC_ON_OFFSET + DRV_CFG_CEC_ON_ADDR_SIZE), /* for start up VFD dimmer setting */
    DRV_CFG_COMP_RES_OFFSET = (DRV_CFG_VFD_DIMMER_OFFSET + DRV_CFG_VFD_DIMMER_ADDR_SIZE), /* fast logo component resolution */
    DRV_CFG_HDMI_RES_OFFSET = (DRV_CFG_COMP_RES_OFFSET + DRV_CFG_COMPONENT_RESOLUTION_SIZE), /* fast logo hdmi resolution */
    DRV_CFG_CVBS_RES_OFFSET = (DRV_CFG_HDMI_RES_OFFSET + DRV_CFG_HDMI_RESOLUTION_SIZE), /* fast logo cvbs resolution */
    DRV_CFG_CONNECT_OFFSET = (DRV_CFG_CVBS_RES_OFFSET + DRV_CFG_CVBS_RESOLUTION_SIZE), /* fast logo output type */
    DRV_CFG_CHILD_LOCK_OFFSET = (DRV_CFG_CONNECT_OFFSET + DRV_CFG_CONNECTION_TYPE_SIZE), /* whether child lock mode */
    DRV_CFG_QUICK_START_OFFSET = (DRV_CFG_CHILD_LOCK_OFFSET + DRV_CFG_CHILD_LOCK_SIZE), /* quick start mode on/off */
    DRV_CFG_REFRESH_BOOT_OFFSET = (DRV_CFG_QUICK_START_OFFSET + DRV_CFG_QUICK_START_SIZE),
    DRV_CFG_RTC_TRIM_OFFSET = (DRV_CFG_REFRESH_BOOT_OFFSET + DRV_CFG_REFRESH_BOOT_SIZE),     /* size = 1 */
    DRV_CFG_HDMI_DEBUG_OFFSET = (DRV_CFG_RTC_TRIM_OFFSET + DRV_CFG_RTC_TRIM_ADDR_SIZE),     /* size = 1 */
    DRV_CFG_PRODUCT_ID_OFFSET = (DRV_CFG_HDMI_DEBUG_OFFSET + DRV_CFG_HDMI_DEBUG_ADDR_SIZE), /* production id registry 20 BYTE */
    DRV_CFG_FACT_STEP_OFFSET = (DRV_CFG_PRODUCT_ID_OFFSET + DRV_CFG_PRODUCT_ID_ADDR_SIZE), /* factory step registry 2 BYTE */
    DRV_CFG_BAD_STEP_OFFSET = (DRV_CFG_FACT_STEP_OFFSET +DRV_CFG_FACT_STEP_ADDR_SIZE ), /* bad step registry 20 BYTE */
    DRV_CFG_PWB_ID_OFFSET = (DRV_CFG_BAD_STEP_OFFSET +DRV_CFG_BAD_STEP_ADDR_SIZE ), /* power board id registry 15 BYTE */
    DRV_CFG_TV_ID_OFFSET = (DRV_CFG_PWB_ID_OFFSET +DRV_CFG_PWB_ID_ADDR_SIZE ), /* TV ID registry 32 BYTE */
    DRV_CFG_D_PWR_MODE_OFFSET = (DRV_CFG_TV_ID_OFFSET +DRV_CFG_TV_ID_ADDR_SIZE ), /* power mode (ac on standby or not) 1 BYTE */
    DRV_CFG_HDMI_LOG_OFFSET = (DRV_CFG_D_PWR_MODE_OFFSET +DRV_CFG_D_PWR_MODE_ADDR_SIZE ), /* hdmi log control registry */
    DRV_CFG_CEC_LOG_OFFSET = (DRV_CFG_HDMI_LOG_OFFSET +DRV_CFG_HDMI_LOG_ADDR_SIZE ), /* cec log control registry */
    DRV_CFG_IFCON_LOG_OFFSET = (DRV_CFG_CEC_LOG_OFFSET +DRV_CFG_CEC_LOG_ADDR_SIZE ), /* ifcon log control registry */
    DRV_CFG_CD_IOP_OFFSET = (DRV_CFG_IFCON_LOG_OFFSET +DRV_CFG_IFCON_LOG_ADDR_SIZE ), /* CD IOP registry */
    DRV_CFG_DVD_IOP_OFFSET = (DRV_CFG_CD_IOP_OFFSET +DRV_CFG_CD_IOP_ADDR_SIZE ), /* DVD IOP registry */
    DRV_CFG_BD_IOP_OFFSET = (DRV_CFG_DVD_IOP_OFFSET +DRV_CFG_DVD_IOP_ADDR_SIZE ), /* BD IOP registry */
    DRV_CFG_HDMI_KSV_OFFSET = (DRV_CFG_BD_IOP_OFFSET +DRV_CFG_BD_IOP_ADDR_SIZE ), /* HDMI KSV registry */
    DRV_CFG_MAC_ID_OFFSET = (DRV_CFG_HDMI_KSV_OFFSET + DRV_CFG_HDMI_KSV_ADDR_SIZE), /* MAC ID registry */
    DRV_CFG_BDP_ID_OFFSET = (DRV_CFG_MAC_ID_OFFSET + DRV_CFG_MAC_ID_ADDR_SIZE), /* BDP ID registry */
    DRV_CFG_AACS_ID_OFFSET = (DRV_CFG_BDP_ID_OFFSET + DRV_CFG_BDP_ID_ADDR_SIZE), /* AACS ID registry */
    DRV_CFG_CPRM_ID_OFFSET = (DRV_CFG_AACS_ID_OFFSET + DRV_CFG_AACS_ID_ADDR_SIZE), /* CPRM ID registry */
    DRV_CFG_HDCP_ID_OFFSET = (DRV_CFG_CPRM_ID_OFFSET + DRV_CFG_CPRM_ID_ADDR_SIZE), /* HDCP ID registry */
    DRV_CFG_WMDRM_ID_OFFSET = (DRV_CFG_HDCP_ID_OFFSET + DRV_CFG_HDCP_ID_ADDR_SIZE), /* WMDRM ID registry */
    DRV_CFG_MODEL_OFFSET = (DRV_CFG_WMDRM_ID_OFFSET + DRV_CFG_WMDRM_ID_ADDR_SIZE), /* model */
    DRV_CFG_DEST_OFFSET = (DRV_CFG_MODEL_OFFSET + DRV_CFG_MODEL_ADDR_SIZE), /* product destnation */
    DRV_CFG_BD_REG_OFFSET = (DRV_CFG_DEST_OFFSET + DRV_CFG_DESTINATION_SIZE), /* BD region code */
    DRV_CFG_DVD_REG_OFFSET = (DRV_CFG_BD_REG_OFFSET + DRV_CFG_BD_REG_CODE_SIZE), /* DVD region code */
    DRV_CFG_REG_FREE_OFFSET = (DRV_CFG_DVD_REG_OFFSET + DRV_CFG_DVD_REG_CODE_SIZE), /* DVD region free flag */
    DRV_CFG_UPDT_TYPE_OFFSET = (DRV_CFG_REG_FREE_OFFSET + DRV_CFG_REG_FREE_FLAG_SIZE), /* F/W update type */
    DRV_CFG_UPDT_MEDIA_OFFSET = (DRV_CFG_UPDT_TYPE_OFFSET + DRV_CFG_UPDT_TYPE_SIZE), /* F/W updata media */
    DRV_CFG_UPDT_STEP_OFFSET = (DRV_CFG_UPDT_MEDIA_OFFSET + DRV_CFG_UPDT_MEDIA_SIZE), /* F/W update step */
    DRV_CFG_UPDT_RETRY_OFFSET = (DRV_CFG_UPDT_STEP_OFFSET + DRV_CFG_UPDT_STEP_SIZE), /* F/W update retry count */
    DRV_CFG_UPDT_URL_TYPE_OFFSET = (DRV_CFG_UPDT_RETRY_OFFSET + DRV_CFG_UPDT_RETRY_SIZE), /* F/W update url type */
    DRV_CFG_BD_PLUS_CHK_OFFSET = (DRV_CFG_UPDT_URL_TYPE_OFFSET + DRV_CFG_UPDT_URL_TYPE_SIZE), /* BD plus check */
    DRV_CFG_AWM_DETECT_OFFSET = (DRV_CFG_BD_PLUS_CHK_OFFSET + DRV_CFG_BD_PLUS_CHK_SIZE), /* AWM detect */
#if 1
    DRV_CFG_HDMI_PA_OFFSET = (DRV_CFG_AWM_DETECT_OFFSET + DRV_CFG_AWM_DETECT_SIZE), /* diag mode serial output */
#else
    DRV_CFG_DMS_OUTPUT_OFFSET = (DRV_CFG_AWM_DETECT_OFFSET + DRV_CFG_AWM_DETECT_SIZE), /* diag mode serial output */
    DRV_CFG_NMS_OUTPUT_OFFSET = (DRV_CFG_DMS_OUTPUT_OFFSET + DRV_CFG_DMS_OUTPUT_SIZE), /* normal mode serial output */
    DRV_CFG_NMS_INPUT_OFFSET = (DRV_CFG_NMS_OUTPUT_OFFSET + DRV_CFG_NMS_OUTPUT_SIZE), /* normal mode serial input */
    DRV_CFG_HDMI_PA_OFFSET = (DRV_CFG_NMS_INPUT_OFFSET + DRV_CFG_NMS_INPUT_SIZE), /* HDMI physical address */
#endif    
    DRV_CFG_CEC_LA_OFFSET = (DRV_CFG_HDMI_PA_OFFSET + DRV_CFG_HDMI_PA_SIZE), /* CEC logical address */
    DRV_CFG_IFCON_WDTM_FLAG_OFFSET = (DRV_CFG_CEC_LA_OFFSET + DRV_CFG_CEC_LA_SIZE), /* ifcon power watch dog timer flag */

//Add by mtk70629 for 5G new request for wlan 

    DRV_CFG_D_DIAG_MODE_OFFSET = (DRV_CFG_IFCON_WDTM_FLAG_OFFSET + DRV_CFG_IFCON_WDTM_FLAG_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_DTCP_ID_OFFSET = (DRV_CFG_D_DIAG_MODE_OFFSET + DRV_CFG_D_DIAG_MODE_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_MRLNBB_ID_OFFSET = (DRV_CFG_DTCP_ID_OFFSET + DRV_CFG_DTCP_ID_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_SERIAL_OFFSET = (DRV_CFG_MRLNBB_ID_OFFSET + DRV_CFG_MRLNBB_ID_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_MAC_OFFSET = (DRV_CFG_WL_SERIAL_OFFSET + DRV_CFG_WL_SERIAL_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_HW_VERSION_OFFSET = (DRV_CFG_WL_MAC_OFFSET + DRV_CFG_WL_MAC_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_WEP_KEY_OFFSET = (DRV_CFG_WL_HW_VERSION_OFFSET + DRV_CFG_WL_HW_VERSION_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_SSID2_OFFSET = (DRV_CFG_WL_WEP_KEY_OFFSET + DRV_CFG_WL_WEP_KEY_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_SSID5_OFFSET = (DRV_CFG_WL_SSID2_OFFSET + DRV_CFG_WL_SSID2_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_IP_OFFSET = (DRV_CFG_WL_SSID5_OFFSET + DRV_CFG_WL_SSID5_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_NETMASK_OFFSET = (DRV_CFG_WL_IP_OFFSET + DRV_CFG_WL_IP_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_BROADCAST_OFFSET = (DRV_CFG_WL_NETMASK_OFFSET + DRV_CFG_WL_NETMASK_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_PMK_OFFSET = (DRV_CFG_WL_BROADCAST_OFFSET + DRV_CFG_WL_BROADCAST_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_COUNT_OFFSET = (DRV_CFG_WL_PMK_OFFSET + DRV_CFG_WL_PMK_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_PACKETSIZE_OFFSET= (DRV_CFG_WL_COUNT_OFFSET + DRV_CFG_WL_COUNT_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_IPADDR2_OFFSET = (DRV_CFG_WL_PACKETSIZE_OFFSET + DRV_CFG_WL_PACKETSIZE_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_IPADDR5_OFFSET = (DRV_CFG_WL_IPADDR2_OFFSET + DRV_CFG_WL_IPADDR2_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI2_U_OFFSET = (DRV_CFG_WL_IPADDR5_OFFSET + DRV_CFG_WL_IPADDR5_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI2_L_OFFSET = (DRV_CFG_WL_RSSI2_U_OFFSET + DRV_CFG_WL_RSSI2_U_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI5_U_OFFSET = (DRV_CFG_WL_RSSI2_L_OFFSET + DRV_CFG_WL_RSSI2_L_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI5_L_OFFSET = (DRV_CFG_WL_RSSI5_U_OFFSET + DRV_CFG_WL_RSSI5_U_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI_DATA2_OFFSET = (DRV_CFG_WL_RSSI5_L_OFFSET + DRV_CFG_WL_RSSI5_L_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_WL_RSSI_DATA5_OFFSET = (DRV_CFG_WL_RSSI_DATA2_OFFSET + DRV_CFG_WL_RSSI_DATA2_SIZE), /* ifcon power watch dog timer flag */
    DRV_CFG_IF_MODEL_OFFSET = (DRV_CFG_WL_RSSI_DATA5_OFFSET + DRV_CFG_WL_RSSI_DATA5_SIZE), /* ifcon model */
    DRV_CFG_IF_DEST_OFFSET = (DRV_CFG_IF_MODEL_OFFSET + DRV_CFG_IF_MODEL_SIZE), /* ifcon destination */
    DRV_CFG_ICE_FLAG_OFFSET = (DRV_CFG_IF_DEST_OFFSET + DRV_CFG_IF_DEST_SIZE),
    DRV_CFG_DEMO_MODE_OFFSET = (DRV_CFG_ICE_FLAG_OFFSET + DRV_CFG_ICE_FLAG_SIZE),
	DRV_CFG_EXT_HDMI_KSV_OFFSET = (DRV_CFG_DEMO_MODE_OFFSET + DRV_CFG_DEMO_MODE_FLAG_SIZE),
	DRV_CFG_RX_HDMI_KSV_OFFSET = (DRV_CFG_EXT_HDMI_KSV_OFFSET + DRV_CFG_EXT_HDMI_KSV_SIZE),
	DRV_CFG_HDD_SERIAL_OFFSET = (DRV_CFG_RX_HDMI_KSV_OFFSET + DRV_CFG_RX_HDMI_KSV_SIZE),
	DRV_CFG_HDD_REVISION_OFFSET = (DRV_CFG_HDD_SERIAL_OFFSET + DRV_CFG_HDD_SERIAL_SIZE),
	DRV_CFG_HDD_MODEL_OFFSET = (DRV_CFG_HDD_REVISION_OFFSET + DRV_CFG_HDD_REVISION_SIZE),
	DRV_CFG_TC_KEY_ID_OFFSET = (DRV_CFG_HDD_MODEL_OFFSET + DRV_CFG_HDD_MODEL_SIZE),
	DRV_CFG_HDCP_RX_ID_OFFSET = (DRV_CFG_TC_KEY_ID_OFFSET + DRV_CFG_TC_KEY_ID_SIZE),
	DRV_CFG_HDD_CAP_OFFSET = (DRV_CFG_HDCP_RX_ID_OFFSET + DRV_CFG_HDCP_RX_ID_SIZE),
	DRV_CFG_TC_RPD_OFFSET = (DRV_CFG_HDD_CAP_OFFSET + DRV_CFG_HDD_CAP_SIZE),
	DRV_CFG_TC_SERVICE_FLAG_OFFSET = (DRV_CFG_TC_RPD_OFFSET + DRV_CFG_TC_RPD_SIZE),
	DRV_CFG_TC_REPLACE_FLAG_OFFSET = (DRV_CFG_TC_SERVICE_FLAG_OFFSET + DRV_CFG_TC_SERVICE_FLAG_SIZE),
	DRV_CFG_MAGIC_OFFSET = (DRV_CFG_TC_REPLACE_FLAG_OFFSET + DRV_CFG_TC_REPLACE_FLAG_SIZE), /* cfg area magic number */
	DRV_CFG_DOWNSTREAM_EDID_OFFSET = (DRV_CFG_MAGIC_OFFSET + DRV_CFG_MAGIC_SIZE),
	DRV_CFG_FORCE_UPG_OFFSET = (DRV_CFG_DOWNSTREAM_EDID_OFFSET + DRV_CFG_DOWNSTREAM_EDID_SIZE),
	DRV_CFG_SERVICE_MODE_OFFSET = (DRV_CFG_FORCE_UPG_OFFSET + DRV_CFG_FORCE_UPG_SIZE),
	DRV_CFG_USBCLI_MODE_OFFSET = (DRV_CFG_SERVICE_MODE_OFFSET + DRV_CFG_USBCLI_MODE_SIZE),
    DRV_CFG_USB_IR_DIAG_MODE_FLAG_OFFSET = (DRV_CFG_USBCLI_MODE_OFFSET + DRV_CFG_USBCLI_MODE_SIZE),
    
#if 1  // ZLL_CEC_TEST
    DRV_CFG_HDMI_PA2_OFFSET = (DRV_CFG_USB_IR_DIAG_MODE_FLAG_OFFSET + DRV_CFG_IR_DIAG_MODE_FLAG_SIZE),
    DRV_CFG_CEC_LA2_OFFSET = (DRV_CFG_HDMI_PA2_OFFSET + DRV_CFG_HDMI_PA2_SIZE),
#endif // ZLL_CEC_TEST

#if 1//def SUPPORT_WAKE_ON_LAN
    DRV_CFG_WOL_OFFSET = (DRV_CFG_CEC_LA2_OFFSET + DRV_CFG_CEC_LA2_SIZE),
#endif

    DRV_CFG_BDP_MONITOR_OFFSET = (DRV_CFG_WOL_OFFSET + DRV_CFG_WOL_SIZE),
    
#if 1  // SACD_LAYER_TEST
    DRV_CFG_SACD_LAYER_OFFSET = (DRV_CFG_BDP_MONITOR_OFFSET + DRV_CFG_BDP_MONITOR_SIZE) ,
#endif // SACD_LAYER_TEST
            
    DRV_CFG_LAST_FUNCTION_TYPE_OFFSET = (DRV_CFG_SACD_LAYER_OFFSET + DRV_CFG_SACD_LAYER_SIZE),    
    DRV_CFG_TV_TYPE_OFFSET = (DRV_CFG_LAST_FUNCTION_TYPE_OFFSET + DRV_CFG_LAST_FUNCTION_TYPE_SIZE),
    DRV_CFG_SCREEN_FORMAT_OFFSET = (DRV_CFG_TV_TYPE_OFFSET + DRV_CFG_TV_TYPE_SIZE),
    DRV_CFG_DVD_ASPECT_RATIO_OFFSET = (DRV_CFG_SCREEN_FORMAT_OFFSET + DRV_CFG_SCREEN_FORMAT_SIZE),
    DRV_CFG_HDMI_RESOLUTION_OFFSET = (DRV_CFG_DVD_ASPECT_RATIO_OFFSET + DRV_CFG_DVD_ASPECT_RATIO_SIZE),
    DRV_CFG_COLOR_SPACE_OFFSET = (DRV_CFG_HDMI_RESOLUTION_OFFSET + DRV_CFG_HDMI_RESOLUTION_SIZE),
    DRV_CFG_HDMI_DEEP_COLOR_OFFSET = (DRV_CFG_COLOR_SPACE_OFFSET + DRV_CFG_COLOR_SPACE_SIZE),
    DRV_CFG_AV_SEPARATION_OUTPUT_OFFSET = (DRV_CFG_HDMI_DEEP_COLOR_OFFSET + DRV_CFG_HDMI_DEEP_COLOR_SIZE),
    
#if 1  // CX_IFCON_VER
    DRV_CFG_IFCON_VER_DATA_OFFSET = (DRV_CFG_AV_SEPARATION_OUTPUT_OFFSET + DRV_CFG_AV_SEPARATION_OUTPUT_SIZE),
#endif // CX_IFCON_VER

    DRV_CFG_MASS_PRODUCT_OFFSET = (DRV_CFG_IFCON_VER_DATA_OFFSET + DRV_CFG_IFCON_VER_DATA_SIZE),
    DRV_CFG_PROGRESS_SKIP_OFFSET = (DRV_CFG_MASS_PRODUCT_OFFSET + DRV_CFG_MASS_PRODUCT_SIZE),
    DRV_CFG_HDMI_EDID_OFFSET = (DRV_CFG_PROGRESS_SKIP_OFFSET +DRV_CFG_PROGRESS_SKIP_SIZE ), /* hdmi log control registry */
    DRV_CFG_HDMI2_LOG_OFFSET = (DRV_CFG_HDMI_EDID_OFFSET +DRV_CFG_HDMI_EDID_ADDR_SIZE ), /* hdmi log control registry */
    DRV_CFG_HDMI2_EDID_OFFSET = (DRV_CFG_HDMI2_LOG_OFFSET +DRV_CFG_HDMI2_LOG_ADDR_SIZE ), /* hdmi log control registry */

    DRV_CFG_UPDATE_MAC_ADDR_2_OFFSET = (DRV_CFG_HDMI2_EDID_OFFSET + DRV_CFG_HDMI2_EDID_ADDR_SIZE),
    
#if 1 // ZLL_RUN_ON_REFER_BOARD
    DRV_CFG_RUN_ON_REFER_BOARD_FLAG_OFFSET = (DRV_CFG_UPDATE_MAC_ADDR_2_OFFSET + DRV_CFG_UPDATE_MAC_ADDR_2_SIZE),
#endif //ZLL_RUN_ON_REFER_BOARD   
    
	DRV_CFG_OFFSET_MAX = (DRV_CFG_RUN_ON_REFER_BOARD_FLAG_OFFSET + DRV_CFG_RUN_ON_REFER_BOARD_FLAG_SIZE)
} DRV_CFG_PARTITION_IDX_T;


BOOL b_disable_log = FALSE;
BOOL b_disable_cli = FALSE;

void log_onoff_control (void)
{
    UINT32 tmp = 0;
    INT32	i4_ret = 0;
    UINT32 dms_output = 0;
    UINT32 nms_output = 0;
    UINT32 nms_input = 0;
    UINT32 u4Addr =0;
    BOOL b_is_diag_mode = FALSE;
#if 1//msb9g, nms/dms new setting.
    UINT32 u4DevId = 0;
    u4Addr = get_part_addr_by_name("mac_addr_1");
    u4DevId = get_part_id_by_name("mac_addr_1");
    printf ("u4DevId:%d! \n", u4DevId);
#else
    u4Addr = get_part_addr_by_name("fast_init_param");
#endif
    GPIO_Config(JIG_MODE0, INPUT, LOW);
    GPIO_Config(JIG_MODE1, INPUT, LOW);

    if(GPIO_Input(JIG_MODE0) || GPIO_Input(JIG_MODE1))
    {
        b_is_diag_mode = FALSE;
        puts("normal boot mode! \n");
    }
    else
    {
        /* this two pin all low */
        b_is_diag_mode = TRUE;
        puts("diag boot mode! \n");
    }
#if 0//msb9g, nms/dms new setting.
#if SONY_GET_REGISTRY
    b_get_registry = TRUE;
#endif
#endif

#if 1//msb9g, nms/dms new setting.
    i4NFBPartitionRead(0,u4Addr + MISC_CUS_DMS_OUTPUT_OFFSET,(UINT32)(&dms_output),1);
    #if 0
        if(b_is_diag_mode)
            printf ("dms_output:[%d]\n", dms_output);
    #endif
    i4NFBPartitionRead(0,u4Addr + MISC_CUS_NMS_OUTPUT_OFFSET,(UINT32)(&nms_output),1);
    #if 0
        if(b_is_diag_mode)
            printf ("nms_output:[%d]\n", nms_output);
    #endif
    i4NFBPartitionRead(0,u4Addr + MISC_CUS_NMS_INPUT_OFFSET,(UINT32)(&nms_input),1);
    #if 0
        if(b_is_diag_mode)
            printf ("nms_input:[%d]\n", nms_input);
    #endif
#else
    i4NFBPartitionRead(0,u4Addr + DRV_CFG_DMS_OUTPUT_OFFSET,(UINT32)(&dms_output),1);
    //if(b_is_diag_mode)
    //printf ("dms_output:%d!  ", dms_output);

    i4NFBPartitionRead(0,u4Addr + DRV_CFG_NMS_OUTPUT_OFFSET,(UINT32)(&nms_output),1);
    //if(b_is_diag_mode)
    //printf ("nms_output:%d!  ", nms_output);

    i4NFBPartitionRead(0,u4Addr + DRV_CFG_NMS_INPUT_OFFSET,(UINT32)(&nms_input),1);
    //if(b_is_diag_mode)
    //printf ("nms_input:%d!\n", nms_input);
#endif

#if 0//msb9g, nms/dms new setting.
#if SONY_GET_REGISTRY
    b_get_registry = FALSE;
#endif
#endif
    if((b_is_diag_mode && (0x01 == dms_output)) || 
        ((!b_is_diag_mode) && (0x01 == nms_output)))
    {
        b_disable_log = TRUE;
        tmp |= 0x01;
    }
    else
    {
        b_disable_log = FALSE;
    }

    if((!b_is_diag_mode) && (0x01 == nms_input))
    {
        b_disable_cli = TRUE;
        tmp |= 0x02;
    }
    else
    {
        b_disable_cli = FALSE;
    }

    BIM_WRITE32(REG_RW_GPRB2, tmp);

    return 0;
}
#if CFG_UBOOT_CUSTOM_JSN
//The function can only be called once because the force_upg flag is cleared here
BOOL b_force_upg_detect (void)
{
    INT32  i4_ret = 0;
    UINT32 force_upg = 0;
    UINT32 u4Addr =0;

    u4Addr = get_part_addr_by_name("fast_init_param");

#if SONY_GET_REGISTRY
      b_get_registry = TRUE;
#endif
    //Read Force UPG Flag
    i4NFBPartitionRead(0,u4Addr + DRV_CFG_FORCE_UPG_OFFSET,(UINT32)(&force_upg),1);
    //printf ("force_upg:%d!  ", force_upg);

    if(0x01 == force_upg)
    {
        puts("Force UPG mode! \n");
        //Clean Force UPG Flag
    #if SONY_GET_REGISTRY
        b_get_registry = TRUE;
    #endif
        force_upg = 0;
        i4NFBPartitionWrite(0,u4Addr + DRV_CFG_FORCE_UPG_OFFSET,(UINT32)(&force_upg),1);
    #if SONY_GET_REGISTRY
        b_get_registry = FALSE;
    #endif
        return 1;
    }
    else
    {
        puts("Not Force UPG mode! \n");
        return 0;
    }
}
#endif
BOOL b_force_usbcli_mode_set(void)
{
    INT32  i4_ret = 0;
	UINT32 force_usbcli = 0;
  	UINT32 u4Addr =0;

	u4Addr = get_part_addr_by_name("fast_init_param");
    printf ("USBCLI get_part_addr_by_name:0x%x!  ", u4Addr);
	if (0x0 == u4Addr)
	{
	    printf("mt85xx_part_tbl_init fail\n");
	    mt85xx_part_tbl_init();
		u4Addr = get_part_addr_by_name("fast_init_param");
        printf ("DIAG get_part_addr_by_name:0x%x!  \n", u4Addr);
		if (0x0 == u4Addr)
		{
		    printf("mt85xx_part_tbl_init fail return\n");
			return FALSE;
		}
	}
#if SONY_GET_REGISTRY
      b_get_registry = TRUE;
#endif
    //Read Force USBCLI Flag
    i4NFBPartitionRead(0,u4Addr + DRV_CFG_USBCLI_MODE_OFFSET,(UINT32)(&force_usbcli),1);
    printf ("force_usbcli:%d!  ", force_usbcli);

	if(0x00 == force_usbcli)
	{
	    puts("Force USBCLI mode! \n");
		//Clean Force USBCLI Flag
		return 0;
	}
	else
	{
	    puts("Not Force USBCLI mode! \n");
		#if SONY_GET_REGISTRY
        b_get_registry = TRUE;
		#endif
        force_usbcli = 0;
		i4NFBPartitionWrite(0,u4Addr + DRV_CFG_USBCLI_MODE_OFFSET,(UINT32)(&force_usbcli),1);
		#if SONY_GET_REGISTRY
    	b_get_registry = FALSE;
		#endif
		return 0;
	}
}


BOOL b_force_diag_mode_set(void)
{
    INT32  i4_ret = 0;
	UINT32 force_diag = 0;
  	UINT32 u4Addr =0;

	u4Addr = get_part_addr_by_name("fast_init_param");
    printf ("DIAG get_part_addr_by_name:0x%x!  ", u4Addr);

	if (0x0 == u4Addr)
	{
	    printf("mt85xx_part_tbl_init fail\n");
	    mt85xx_part_tbl_init();
		u4Addr = get_part_addr_by_name("fast_init_param");
        printf ("DIAG get_part_addr_by_name:0x%x!  \n", u4Addr);
		if (0x0 == u4Addr)
		{
		    printf("mt85xx_part_tbl_init fail return\n");
			return FALSE;
		}
	}
#if SONY_GET_REGISTRY
      b_get_registry = TRUE;
#endif
    //Read Force UPG Flag
    i4NFBPartitionRead(0,u4Addr + DRV_CFG_D_DIAG_MODE_OFFSET,(UINT32)(&force_diag),1);
    printf ("force_upg:%d!  ", force_diag);

	if(0x01 == force_diag)
	{
	    puts("Force DIAG mode! \n");
		//Clean Force UPG Flag
		return 0;
	}
	else
	{
	    puts("Not Force DIAG mode! \n");
		#if SONY_GET_REGISTRY
        b_get_registry = TRUE;
		#endif
        force_diag = 1;
		i4NFBPartitionWrite(0,u4Addr + DRV_CFG_D_DIAG_MODE_OFFSET,(UINT32)(&force_diag),1);
		#if SONY_GET_REGISTRY
    	b_get_registry = FALSE;
		#endif
		return 0;
	}
}
#endif

#if CONFIG_AWM_ROBUSTNESS_FINAL_SOLUTION
INT32 i4NFBPartitionRead_NoSkipBad(UINT32 u4DevId, UINT32 u8Offset, UINT32 u4MemPtr, UINT32 u4MemLen)
{
    UINT32 start_addr=0;        // start address
    UINT32 ofs=0;
    UINT32 blockstart = 1;
    int  pagelen, badblock = 0;
    UINT32 u4RdByteCnt = 0;
	UINT32 u4RdLen = 0;
    INT32 i4Ret = 0;
	UINT8 *pu1MemBuf = (UINT8*)u4MemPtr;
	struct mtd_info *mtd=NULL;
    NAND_LOG(NAND_LOG_INFO, "[Read_NoSkipBad]u4DevId = %d, u8Offset = 0x%x, u4MemPtr = 0x%x, u4MemLen = 0x%x \n", 
		    u4DevId, u8Offset, u4MemPtr, u4MemLen);

	ASSERT(pu1MemBuf != NULL);
	mtd = get_mtd_device(NULL, u4DevId);

    /* Initialize page size */
    pagelen = mtd->writesize;
    start_addr = u8Offset;        // start address
    
    // Check, if length fits into device
    if ( ((u4MemLen / pagelen) * mtd->writesize) > (mtd->size - u8Offset)) 
    {
        NAND_LOG(NAND_LOG_ERROR, "[part read] Image %d bytes, NAND page %d bytes, OOB area %u bytes, device size %u bytes\n",
                 u4MemLen, pagelen, mtd->oobsize, mtd->size);
        NAND_LOG(NAND_LOG_ERROR, "[part read] Error : Input file can not fit into device\n");        
        i4Ret = -1;
        goto closeall;
    }
    
    for (ofs = start_addr; ofs < mtd->size && u4RdByteCnt < u4MemLen ;) 
    {
        // new eraseblock , check for bad block
        if (blockstart != (ofs & (~mtd->erasesize + 1))) 
        {
            blockstart = ofs & (~mtd->erasesize + 1);
		    //NAND_LOG("[part read] partition_%d - check block 0x%X\n", u4DevId, blockstart);
			if ((badblock = mtd->block_isbad(mtd, blockstart)) < 0)
            {
                NAND_LOG(NAND_LOG_ERROR, "[part read]Error check bad block\n");
                goto closeall;
            }
        }

        if (badblock) 
        {
            // skip the bad block
            ofs += mtd->erasesize;
			badblock = 0;
            NAND_LOG(NAND_LOG_INFO, "[part read] partition_%d - bad block found at : 0x%X\n", u4DevId, blockstart);
            continue;
        }
        else 
        {
            /* Read page data and exit on failure */
			pagelen = mtd->writesize;
			if (((ofs % mtd->writesize) != 0) && ((u4MemLen - u4RdByteCnt) >= mtd->writesize))
            { /*if ofs is not page aligned, and the read length is more than one page ==> 
                        then, read the 1st block data to enable 2nd block data page aligned, it is important if meet the next block is bad*/
              pagelen = mtd->writesize - (ofs % mtd->writesize);
            }
			else if ((u4MemLen - u4RdByteCnt) < mtd->writesize)
			{
			  pagelen = (u4MemLen - u4RdByteCnt);
			}

            if ((i4Ret = mtd->read(mtd, ofs, pagelen, &u4RdLen, (u_char *)(pu1MemBuf + u4RdByteCnt)))<0)
            {
                NAND_LOG(NAND_LOG_ERROR, "[part read]Page read error occur at : 0x%x\n", ofs);
                goto closeall;
            }
            ofs += u4RdLen;
            u4RdByteCnt += (UINT32)u4RdLen;
        }

    }

    if (u4RdByteCnt < u4MemLen)
    {
        NAND_LOG(NAND_LOG_ERROR, "[part read]Can't read enough data, u4RdByteCnt = 0x%x, u4MemLen = 0x%x\n", u4RdByteCnt, u4MemLen);        
        goto closeall;
    }
    /* Exit happy */
    return i4Ret;

closeall:    
    i4Ret = -1;    
    return i4Ret;
}

extern void serial_disable(int idisable);
extern unsigned int uboot_flag;

void read_awm_flag(void)
{
    unsigned char tmp = 0;
    INT32  i4Ret = 0;
  	UINT32 u4Addr =0;
    UINT32 u4Size =0x100000;

	u4Addr = get_part_addr_by_name("mac_addr_1");
    u4Size = get_part_size_by_name("mac_addr_1");

    if( (0==u4Addr) || (0==u4Size))
        tmp = 0xFF;
    else
    {    
        u4Size = u4Size - (u4Size%0x100000);//work around for bad block in mac addr partition
        i4Ret = i4NFBPartitionRead_NoSkipBad(0,u4Addr + (u4Size>>1),(UINT32)(&tmp),1);
        if(i4Ret!=0)
        {
            tmp = 0xCC;
            /*i4Ret = */i4NFBPartitionWrite(0,u4Addr + (u4Size>>1),(UINT32)(&tmp),1);
            //if(i4Ret<0)
                //NAND_LOG(NAND_LOG_ERROR, "[read_awm_flag]i4NFBPartitionWrite failed\n");
        }
    }
    NAND_LOG(NAND_LOG_INFO, "[read_awm_flag]uart_flag=0x%x\n",tmp);
    if(0xCC==tmp)

    {
        uboot_flag = FALSE;
        serial_disable(1);
    }    
    
}
#endif


EXPORT_SYMBOL(get_part_id_by_name);
EXPORT_SYMBOL(get_part_info_by_name);
EXPORT_SYMBOL(get_part_addr_by_name);
EXPORT_SYMBOL(get_part_size_by_name);
EXPORT_SYMBOL(get_page_size);
EXPORT_SYMBOL(get_block_size);
EXPORT_SYMBOL(i4NFBPartitionRead);
EXPORT_SYMBOL(i4NFBPartitionWrite);

#endif

