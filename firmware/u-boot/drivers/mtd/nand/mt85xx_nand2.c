#include <common.h>
#include <nand.h>
#include <malloc.h>
#include <asm/arch/x_typedef.h>

#include <asm/errno.h>
#include <asm/io.h>

#include <linux/mtd/NFI2.h>
#include <linux/mtd/NFIECC2.h>

static int _data_buf_index = 0;
BOOL _fgUsingDMA = TRUE;
BOOL _fgAUTO_FMT = TRUE;
BOOL _fgECCSWCorrect = FALSE;
u32 _u4ECC_BIT = ECC_12_BITS;

/* Define default oob placement schemes for large and small page devices */
#if 0
static struct nand_oobinfo nand_oob_8 = {
	.useecc = MTD_NANDECC_AUTOPLACE,
	.eccbytes = 3,
	.eccpos = {0, 1, 2},
	.oobfree = { {3, 2}, {6, 2} }
};

static struct nand_oobinfo nand_oob_16 = {
	.useecc = MTD_NANDECC_AUTOPLACE,
	.eccbytes = 6,
	.eccpos = {0, 1, 2, 3, 6, 7},
	.oobfree = { {8, 8} }
};

static struct nand_oobinfo nand_oob_64 = {
	.useecc = MTD_NANDECC_AUTOPLACE,
	.eccbytes = 24,
	.eccpos = {
		40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55,
		56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = { {2, 38} }
    };
#endif

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr mt85xx_largepage_bbt_descr = {
	.options = 0,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};

static struct nand_bbt_descr mt85xx_smallpage_bbt_descr = {
	.options = 0,
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};


#if 0
static struct nand_oobinfo mt85xx_oobinfo_2048 = {
         .useecc = MTD_NANDECC_AUTOPL_USR,
	.eccbytes = 40,
	.eccpos = { 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55},
	.oobfree = {{2, 22}}
};

static struct nand_oobinfo mt85xx_oobinfo_512 = {
         .useecc = MTD_NANDECC_AUTOPL_USR,    
	.eccbytes = 10,
	.eccpos = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	.oobfree = {{0, 6}}
};
#endif
static struct nand_ecclayout mt85xx_oobinfo_512 = {
	.eccbytes = 10,
	.eccpos = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	.oobfree = {{0, 6}}
};

static struct nand_ecclayout mt85xx_oobinfo_2048 = {
	.eccbytes = 40,
	.eccpos = { 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63},
	.oobfree = {{2, 22}}
};    

/*
static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr mt85xx_bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr mt85xx_bbt_mirror_descr_512 = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = mirror_pattern
};
*/

static int mt85xx_nand_device_ready(struct mtd_info *mtd)
{
	int result = !( *NFI_STA & STATUS_BUSY);

	return result;
}


//------------------------------------------------------------------------------
// Reset Device Callback Function
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_Reset(
        const struct mtd_info *mtd
        ,const u32  c_timeout
) {
    u32    timeout = c_timeout;
    STATUS_E  ret=S_UNKNOWN_ERR;
    s32    i4Val;
//    struct mt85xx_priv *mt85xx = mtd->priv;    
#ifdef _XOS_ISR_ENABLE_
    EV_GRP_EVENT_T rRecvEvt;
#endif //_XOS_ISR_ENABLE_

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_RESET;

    // enable interrupt
    *NFI_INTR_EN = RESET_DONE_EN;

    // reset cmd
    *NFI_CMD = NAND_CMD_RESET;

    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // wait for reset finish
    timeout = c_timeout;
#ifdef _XOS_ISR_ENABLE_
    if (_bUsingISR)
    {
        i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                          RESET_DONE,
                                          &rRecvEvt,
                                          X_EV_OP_OR_CONSUME,
                                          timeout);
        ASSERT( (i4Val == OSR_OK) || (OSR_TIMEOUT == i4Val));
        if (OSR_TIMEOUT == i4Val)
        {
            ret = S_TIMEOUT;
            goto end;
        }
    }
    else
#endif //_XOS_ISR_ENABLE_
#ifdef LINUX_ISR_ENABLE
    if (_fgUsingISR)
    {
            wait_for_completion((struct completion*)(&mt85xx->reset_completion));
    }
    else
#endif
    {
        NFI_Wait( !(*NFI_INTR & RESET_DONE), timeout);
        i4Val = *NFI_INTR;
#ifdef INT_WR_CLR
        *NFI_INTR = i4Val;
#endif        
        //UNUSED(i4Val);
        if( 0 == timeout ) {
            ret = S_TIMEOUT;
            goto end;
        }
    }

    ret = S_DONE;

end:
    // disable interrupt
    *NFI_INTR_EN = 0;


    return ret;
}


static void  NAND_COMMON_ReadID(struct mtd_info *mtd, unsigned command)
{
    register struct nand_chip *this = mtd->priv;
    uint  nfi_pagefmt;
    u32* p4Data;    

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_READ_ID_ST;

    // always use 8bits I/O interface to read device id
    nfi_pagefmt = *NFI_PAGEFMT;
    *NFI_PAGEFMT = (nfi_pagefmt&(~PAGEFMT_16BITS))|PAGEFMT_8BITS;

    // read id cmd
    *NFI_CMD = NAND_CMD_READID;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // issue addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = 0;
    *NFI_ADDRNOB = 1;
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );

    // set single read by DWORD
    *NFI_CON = SINGLE_RD | NOB_DWORD;
    // wait til DATA_READ is completely issued
    while( *NFI_STA  & STATUS_DATAR );

    p4Data = (u32*)this->buffers->databuf;
    p4Data[0] = *NFI_DATAR;

}

//------------------------------------------------------------------------------
// ECC Error Software Correct
//------------------------------------------------------------------------------
STATUS_E NAND_COMMON_ECCErrCorrect(
        const u32  c_timeout
        ,const u32 u4SectIdx
        ,const u32 *p_data32 /* MUST be 32bits alignment addr */
)
{
    u32 timeout;
    u32 i;
    u32 u4ErrNum;
    u32 u4ErrVal;
    u16 u2ErrLoc;
    STATUS_E ret = S_UNKNOWN_ERR;
    u32 u4ErrLoc;

    timeout = c_timeout;
    NFI_Wait( !(*NFIECC_DECDONE & (1 << u4SectIdx)), timeout); // wait for all block decode done
    if( 0 == timeout ) {
        ret = S_TIMEOUT;
        goto end;
    }

    u4ErrNum = (*NFIECC_DECENUM & (0xF << (u4SectIdx*4))) >> (u4SectIdx*4);
    if ( 0xF == u4ErrNum)
    {
        ret = S_ECC_UNCORRECT_ERR;
        goto end;
    }
    else if ( 0x0 == u4ErrNum)
    {
        ret = S_DONE;
        goto end;
    }
    else
    {
        for (i = 0 ; i < u4ErrNum ; i++)
        {
            u2ErrLoc = *(u16*)((u32)(NFIECC_BASE + NFIECC_DECEL0) + i*2);
            if ( (u2ErrLoc /8) < 512)
            {
                u4ErrLoc = (u32)p_data32+u4SectIdx*512+u2ErrLoc/8;
                u4ErrVal = *(u8*)u4ErrLoc;
                u4ErrVal = u4ErrVal & (1 << (u2ErrLoc%8));
                if (u4ErrVal)
                    *(u8*)u4ErrLoc &= (~u4ErrVal);
                else
                    *(u8*)u4ErrLoc |= (1 << (u2ErrLoc%8));
            }
            else
            {
                u4ErrLoc = (u32)(NFIECC_BASE + NFI_FDM0L)+u4SectIdx*8 + (((u2ErrLoc/8) -512)/4)*4;
                u4ErrVal = *(u32*)u4ErrLoc;
                u4ErrVal = u4ErrVal & (1 << ((u2ErrLoc - 512*8)%32));
                if (u4ErrVal)
                    *(u32*)u4ErrLoc &= (~u4ErrVal);
                else
                    *(u32*)u4ErrLoc |= (1 << ((u2ErrLoc - 512*8)%32));
            }
        }
        ret = S_ECC_CORRECTABLE_ERR;
    }
    end:
    return ret;
}

u32 _u4ErrBits=0; //record err bit number
//------------------------------------------------------------------------------
// ECC Error Detect
//------------------------------------------------------------------------------
STATUS_E NAND_COMMON_ECCErrDetect(
       struct mtd_info *mtd
        ,const u32  c_timeout
        //,const u32  row_addr
        //,const u32 *p_data32 /* MUST be 32bits alignment addr */
)
{
    u32 u4BlockNum, page_size,i, u4Tmp, timeout;
    STATUS_E ret = S_DONE;
    //  volatile UINT32* p4ErrLoc = *NFIECC_DECEL0;

    page_size =  mtd->writesize;
    u4BlockNum =  page_size/SECTOR_BYTES;
//    mtd->ecc_stats.failed = 0;
//    mtd->ecc_stats.corrected = 0;

    u4Tmp = 0;
    _u4ErrBits = 0;
    for ( i = 0 ; i < u4BlockNum ; i++)
        u4Tmp |= 1 << i;

    timeout = c_timeout;
    NFI_Wait( (*NFIECC_DECDONE != u4Tmp), timeout); // wait for all block decode done
    if( 0 == timeout ) {
        return S_TIMEOUT;
    }

    if ( !*NFIECC_DECFER)
    {
//        mtd->ecc_stats.failed = 0;
        return S_DONE;
    }
    else
    {
        u4Tmp = *NFIECC_DECENUM; // error number
        for ( i = 0 ; i < 8 ; i++)
        {
            if ( 0xF == (u4Tmp & 0xF))
            {
                //Printf("Page 0x%x Sector %d with ECC Error\n", row_addr, i);
                printk("Page 0x%x Sector %d with ECC un-correctable Error\n", *NFI_ROWADDR, i);
//                mtd->ecc_stats.failed++;
                return S_ECC_UNCORRECT_ERR;
            }
            else if ( u4Tmp & 0xF)
            {
                _u4ErrBits += (u4Tmp&0xF);
                ret = S_ECC_CORRECTABLE_ERR;
                //printk("Page 0x%x Sector %d with ECC correctable Error\n", *NFI_ROWADDR, i);
                //printk("AHB address = 0x%x\n", *NFI_STRADDR);
//                mtd->ecc_stats.failed = 0;
//                mtd->ecc_stats.corrected++;
            }

            u4Tmp >>= 4;
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
// Read From NFI FIFO
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_FIFO_Read(
        const struct nand_chip *mt85xx
        ,const u32  c_timeout
        ,const BOOL bUsingDMA
        ,u32 *p_data32 /* MUST be 32bits alignment addr */
        ,const u32 data_len
) {
    u32  timeout = c_timeout;
    u32  i ;
#ifdef _XOS_ISR_ENABLE_
    EV_GRP_EVENT_T rRecvEvt;
    s32 i4Val;
#endif //_XOS_ISR_ENABLE_

    if(bUsingDMA)
    {
        // read page data with DMA
        timeout = c_timeout;
        // wait for DMA transmission complete
#ifdef _XOS_ISR_ENABLE_
        if (_bUsingISR)
        {
            i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                              AHB_DONE,
                                              &rRecvEvt,
                                              X_EV_OP_OR_CONSUME,
                                              timeout);

            if(i4Val == OSR_TIMEOUT) {
                return S_TIMEOUT;
            }
        }
        else
#endif //_XOS_ISR_ENABLE_
#ifdef LINUX_ISR_ENABLE
        if (_fgUsingISR)
        {
        	    wait_for_completion((struct completion*)(&mt85xx->dma_completion));
        }
        else
#endif
        {
            //--------------
            /* Trigger DMA action */
            //*NFI_CNFG |= AHB_MODE;
            NFI_Wait( (AHB_DONE != (*NFI_INTR&AHB_DONE)), timeout);
            *NFI_INTR_EN &= ~AHB_DONE_EN; // disable INT first
            i = *NFI_INTR;//read clear again
#ifdef INT_WR_CLR
            *NFI_INTR = i;
#endif            
            if( 0 == timeout) {
                return S_TIMEOUT;
            }
        }
    }
    else
    {
    // read page data
        for(i=0; i<data_len; i+=4)
        {
            // wait for data ready
            // when RD_EMPTY_MASK flag is poll-down, it means data is ready in FIFO at least 4 bytes.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & RD_EMPTY_MASK), timeout);
            if( 0 == timeout ) {
                return S_TIMEOUT;
              }
            *(u32*)((u32)p_data32 + i) = *NFI_DATAR;
            if ( !((i+4) % SECTOR_BYTES))
            {
                if ( _fgECCSWCorrect)
                {
                    STATUS_E ret = S_UNKNOWN_ERR;
                    ret = NAND_COMMON_ECCErrCorrect(c_timeout, ((i+4)/SECTOR_BYTES)-1, p_data32); // AUTO_FMT must be enabled
                    if ( (S_DONE != ret) && (S_ECC_CORRECTABLE_ERR != ret))
                        return ret;
                }
            }
        }
    }
    return S_DONE;

}


static void  NAND_COMMON_PageRead(struct mtd_info *mtd, unsigned command, int column, int page_addr)
{
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size = mtd->writesize;    
    u32     spare_size = mtd->oobsize;
    u32    column_addr_bytes, row_addr_bytes, addr_cycle;
        u32* p_data32 = (u32*)mt85xx->buffers->databuf;
    u32 i;
//    u32 timeout;
//        u32 c_timeout = MTD_NAND_DEFAULT_TIMEOUT;
//    STATUS_E ret = S_UNKNOWN_ERR;
//         volatile u32* p4TmpSpare = (u32*)(mt85xx->buffers->databuf + page_size);
//         u32 p4TmpBuf[128];
//         volatile u32* p4TmpSpare2 = p4TmpBuf;

        //flush_cache_all();
        if (page_size > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_READ|READ_MODE |HW_ECC_EN;
    if (_fgAUTO_FMT)
        *NFI_CNFG |= AUTO_FMT_EN  ;
    if (page_size == 512)
        *NFI_CNFG |= SEL_SEC_512BYTE;    
#ifdef ARCH_MT8530
        if ( CS1 == *NFI_CSEL)
            *NFI_CNFG |= RB_CS1;
#endif    
    *NFI_CON = SEC_NUM(page_size/SECTOR_BYTES);
    if (_fgECCSWCorrect)
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_LOCATE) | DEC_NFI_MODE;
    else
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_CORRECT) | DEC_NFI_MODE;
    
    if ( spare_size/(page_size/SECTOR_BYTES) >= 52)
        *NFIECC_DECCNFG |= DEC_TNUM(_u4ECC_BIT) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + _u4ECC_BIT*14 );
    else if (page_size == 512)
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_4_BITS) | DEC_CS( ((SECTOR_BYTES + 8) << 3) + ECC_4_BITS*14 );
    else
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_12_BITS) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + ECC_12_BITS*14 );

    *NFIECC_DECCON = 0; // reset
    *NFIECC_DECCON = DEC_EN;
    //*NFIECC_DECIRQEN = DEC_IRQEN;
    //i = *NFIECC_DECIRQSTA ; //read clear
    if ( AUTO_FMT_EN & (*NFI_CNFG))
        *NFIECC_FDMADDR = (u32)NFI_FDM0L;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE;
//            *NFI_STRADDR = (u32)p_data32 + 0xC0000000;
    }

    // read cmd
    *NFI_CMD = NAND_CMD_READ0;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page_addr;
    //  if( 4 < addr_cycle ) {
    // if addr cycle is more than 4, you have to fill 5th cycle addr
    //    *NFI_ADDRM = (row_addr>>(32-column_addr_bits));
    //  }
#if 0        
    // no. of addr cycle
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );


    // read confirm
    if(mtd->writesize > 512) {
        *NFI_CMD = NAND_CMD_READSTART;
        while( *NFI_STA  & STATUS_BUSY);
    }
    if (_fgUsingDMA)
        *NFI_INTR_EN |= AHB_DONE_EN ;
    i = *NFI_INTR;
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif       
#endif
}


//------------------------------------------------------------------------------
// Read Status Callback Function
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_ReadStatus(
        const struct mtd_info *mtd
        ,const u32  c_timeout
) {
    register struct nand_chip *mt85xx = mtd->priv;
//    struct mt85xx_priv *mt85xx = mtd->priv;
    u32* p4Data = (u32*)mt85xx->buffers->databuf;   

    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_READ_ID_ST;
    // read status cmd
    *NFI_CMD = NAND_CMD_STATUS;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // set single read by DWORD
    *NFI_CON = SINGLE_RD | NOB_DWORD;
    // wait til DATA_READ is completely issued
    while( *NFI_STA  & STATUS_DATAR );

    // single read doesn't need to polling FIFO
    p4Data[0] = *NFI_DATAR;

    return S_DONE;
/*
    if (_fgDataInvert)
    {
        _u4RightMask = status & 0x17171717;
        _u4InvertMask = (~(status & 0xE8E8E8E8)&0xE8E8E8E8);
        status = _u4RightMask | _u4InvertMask;
    }

    // check READY/BUSY status first
    if( !(STATUS_READY&status) ) {
        return S_IN_PROGRESS;
    }

    // flash is ready now, check status code
    if( STATUS_FAIL & status ) {
        if( !(STATUS_WR_ALLOW&status) ) {
          return S_BLOCK_LOCKED_ERR;
    }
    else {
          return S_UNKNOWN_ERR;
        }
    }
    else {
        return S_DONE;
    }
*/    
}

//------------------------------------------------------------------------------
// Block Erase Related Callback Function
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_BlockErase(
        const struct mtd_info *mtd
        ,const u32  page_addr
) {
//  uint32  page_size;
//  uint32  spare_size;
    u32    page_size = mtd->writesize;    
    s32 i4Val;
#ifdef _XOS_ISR_ENABLE_
    EV_GRP_EVENT_T rRecvEvt;
#endif
    u32 timeout;
    u32  addr_cycle, column_addr_bytes,row_addr_bytes;
    STATUS_E ret = S_UNKNOWN_ERR;

    if (page_size > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;


    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_ERASE ;
    *NFI_INTR_EN = ERASE_DONE_EN;

    // block erase cmd
    *NFI_CMD = NAND_CMD_ERASE1;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr, erase command only fill row address, so column bits shift is unnecessary
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page_addr;
    // no. of addr cycle
    *NFI_ADDRNOB = ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );

    // block erase confirm
    *NFI_CMD = NAND_CMD_ERASE2;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    timeout = MTD_NAND_DEFAULT_TIMEOUT;
#ifdef _XOS_ISR_ENABLE_
    if (_bUsingISR)
    {
        i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                      ERASE_DONE,
                                      &rRecvEvt,
                                      X_EV_OP_OR_CONSUME,
                                      timeout);
        if( i4Val == OSR_TIMEOUT )
        {
            ret = S_TIMEOUT;
            goto end;
        }
    }
    else
#endif //_XOS_ISR_ENABLE_
#ifdef LINUX_ISR_ENABLE
    if (_fgUsingISR)
    {
        struct mt85xx_priv *mt85xx = mtd->priv;
        wait_for_completion((struct completion*)&mt85xx->erase_completion);
    }
    else
#endif
    {
        //NFI_Wait( (STATUS_BUSY == (*NFI_STA & STATUS_BUSY)), timeout);
        NFI_Wait( (ERASE_DONE != (*NFI_INTR&ERASE_DONE)), timeout); //clear INT status
        *NFI_INTR_EN &= ~ERASE_DONE_EN; // disable INT first
        i4Val =  *NFI_INTR; // read clear
#ifdef INT_WR_CLR
        *NFI_INTR = i4Val;
#endif  
        //UNUSED(i4Val);
        if( 0 == timeout) {
            ret = S_TIMEOUT;
            goto end;
        }
    }
    ret = S_DONE;

end:
    if (S_DONE!= ret)
        printk("\nBlock erase failed at page 0x%x, ret = 0x%x\n", page_addr, ret);    
    *NFI_CON = 0;
    *NFIECC_ENCCON = 0x0;


    return ret;
}

//------------------------------------------------------------------------------
// Write To NFI FIFO
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_FIFO_Write(
        const struct nand_chip *mt85xx
        ,const u32  c_timeout
        ,const BOOL bUsingDMA
        ,const u32 *p_data32 /* MUST be 32bits alignment addr */
        ,const u32 data_len
) {
    u32  timeout = c_timeout;
    u32  i;
#ifdef _XOS_ISR_ENABLE_
    int32 i4Val;
    EV_GRP_EVENT_T rRecvEvt;
#endif //_XOS_ISR_ENABLE_

    if(bUsingDMA)
    {
        // program page data with DMA

        // wait for DMA transmission complete
        timeout = c_timeout;
#ifdef _XOS_ISR_ENABLE_
        if (_bUsingISR)
        {
            i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                              AHB_DONE,
                                              &rRecvEvt,
                                              X_EV_OP_OR_CONSUME,
                                              timeout);

            if( i4Val == OSR_TIMEOUT )
                return S_TIMEOUT;
        }
        else
#endif //_XOS_ISR_ENABLE_
#ifdef LINUX_ISR_ENABLE
        if (_fgUsingISR)
        {
            wait_for_completion((struct completion*)&mt85xx->dma_completion);
        }
        else
#endif
        {
            NFI_Wait( (AHB_DONE != (*NFI_INTR&AHB_DONE)), timeout); //clear INT status
            *NFI_INTR_EN &= ~AHB_DONE; // disable INT
            i = *NFI_INTR;//read clear again
#ifdef INT_WR_CLR
            *NFI_INTR = i;
#endif
            if( 0 == timeout) {
                return S_TIMEOUT;
            }
        }
    }
    else
    {
        // program page data
        for(i=0; i<data_len; i+=4, p_data32++)
        {
            // wait for FIFO has space to enqueue
            // when WR_FULL_MASK flag is poll-down, it means there are at least 4 bytes free space in FIFO.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & WR_FULL_MASK), timeout);
            if( 0 == timeout ) {
                return S_TIMEOUT;
            }
            *NFI_DATAW = *p_data32;
        }
    }

    return S_DONE;

}

//------------------------------------------------------------------------------
// Page Program Callback Function
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_SEQIN(
        const struct mtd_info *mtd
        ,const u32  c_timeout
        ,const u32  page_addr
        //,const u32 *p_data32 /* MUST be 32bits alignment addr */
        //,const u32 *p_spare32
        //,uint32  ecc_parity_from_reg[4]
) {
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size, spare_size;
    u32    column_addr_bytes,row_addr_bytes;
    u32    addr_cycle;
//    u32     timeout;
    //  uint32    io_interface;
//    STATUS_E  ret=S_UNKNOWN_ERR;
//    u32 i;
    u32* p_data32 = (u32*)mt85xx->buffers->databuf;
#ifdef _XOS_ISR_ENABLE_
    s32 i4Val;
    EV_GRP_EVENT_T rRecvEvt;
#endif //_XOS_ISR_ENABLE_

    //ASSERT(!((UINT32)p_data32%0x4)); //assert

    //if (_fgDataInvert && _bAUTO_FMT)  //  _bAUTO_FMT to avoid the recursive loop
    //    return NAND_PATCH_PageProgram(nand_info,c_timeout, row_addr, p_data32, p_spare32);

    page_size = mtd->writesize;    
    spare_size = mtd->oobsize;

    if (page_size > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_PROGRAM ;
    if (_fgAUTO_FMT)
    *NFI_CNFG |= AUTO_FMT_EN | HW_ECC_EN;
    if ( spare_size/(page_size/SECTOR_BYTES) >= 26)
    {
        *NFIECC_ENCCNFG = ENC_TNUM(_u4ECC_BIT) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;
    }
    else
    {
        *NFIECC_ENCCNFG = ENC_TNUM(ECC_6_BITS) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;
    }
    *NFIECC_ENCCON = 0;
    *NFIECC_ENCCON = ENC_EN;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE ;
        *NFI_STRADDR = ((u32)p_data32) + 0xC0000000;
    }

    // in most 512 page size NAND flash, you have to setup destination pointer to 1st half area
    if(page_size <=512) {
        *NFI_CMD = NAND_CMD_READ0;
        // wait til CMD is completely issued
        while( *NFI_STA  & STATUS_CMD );
        *NFI_CON = NFI_RST;
        while( *NFI_STA  & STATUS_CMD );
    }
    *NFI_CON = SEC_NUM(page_size/512);
    // program cmd
    *NFI_CMD = NAND_CMD_SEQIN;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page_addr;
    // no. of addr cycle
    //*NFI_ADDRNOB = addr_cycle;
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );

    return S_DONE;
}


//------------------------------------------------------------------------------
// Page Program Callback Function
//------------------------------------------------------------------------------
STATUS_E  NAND_COMMON_PageProgram(
        const struct mtd_info *mtd
        ,const u32  c_timeout
        ,const u32  page_addr
        //,const u32 *p_data32 /* MUST be 32bits alignment addr */
        //,const u32 *p_spare32
        //,uint32  ecc_parity_from_reg[4]
) {
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size, spare_size;
    u32    column_addr_bytes,row_addr_bytes;
    u32    addr_cycle;
    u32     timeout;
    //  uint32    io_interface;
    STATUS_E  ret=S_UNKNOWN_ERR;
    u32 i;
    u32* p_data32 = (u32*)mt85xx->buffers->databuf;
#ifdef _XOS_ISR_ENABLE_
    s32 i4Val;
    EV_GRP_EVENT_T rRecvEvt;
#endif //_XOS_ISR_ENABLE_

    //ASSERT(!((UINT32)p_data32%0x4)); //assert

    //if (_fgDataInvert && _bAUTO_FMT)  //  _bAUTO_FMT to avoid the recursive loop
    //    return NAND_PATCH_PageProgram(nand_info,c_timeout, row_addr, p_data32, p_spare32);
    
    page_size =  mtd->writesize;
    spare_size = mtd->oobsize;

    if (page_size > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;
#if 0
    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_PROGRAM ;
    if (_fgAUTO_FMT)
    *NFI_CNFG |= AUTO_FMT_EN | HW_ECC_EN;
    if ( spare_size/(page_size/SECTOR_BYTES) >= 26)
    {
        *NFIECC_ENCCNFG = ENC_TNUM(_u4ECC_BIT) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;
    }
    else
    {
        *NFIECC_ENCCNFG = ENC_TNUM(ECC_6_BITS) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;
    }
    *NFIECC_ENCCON = 0;
    *NFIECC_ENCCON = ENC_EN;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE ;
        *NFI_STRADDR = ((u32)p_data32);
    }

    // in most 512 page size NAND flash, you have to setup destination pointer to 1st half area
    if(page_size <=512) {
        *NFI_CMD = NAND_CMD_READ0;
        // wait til CMD is completely issued
        while( *NFI_STA  & STATUS_CMD );
        *NFI_CON = NFI_RST;
        while( *NFI_STA  & STATUS_CMD );
    }
    *NFI_CON = SEC_NUM(page_size/512);
    // program cmd
    *NFI_CMD = NAND_CMD_SEQIN;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page_addr;
    // no. of addr cycle
    //*NFI_ADDRNOB = addr_cycle;
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );
#endif
    // prepare FDM data
    if ( AUTO_FMT_EN & (*NFI_CNFG))
    {
        volatile u32* pFDMAddr = NFI_FDM0L;
        volatile u32* pTmpAddr = (u32*)(mt85xx->buffers->databuf+ page_size);
        u32 p4TmpBuf[128];
        volatile u32* p4TmpSpare2 = pTmpAddr;            
        
        for (i = 0; i < (page_size/512) ; i++)
        {
            memcpy((void*)p4TmpBuf,(void*) p4TmpSpare2, FDM_BYTES);
            *pFDMAddr++ = p4TmpBuf[0];
            *pFDMAddr++ = p4TmpBuf[1];
            p4TmpSpare2 = (u32*)((u32)p4TmpSpare2 + FDM_BYTES);
        }
        
    }
    if (_fgUsingDMA)
        *NFI_INTR_EN = AHB_DONE_EN ;
    i = *NFI_INTR;  // read clear
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif    
    // set burst program by DWORD
    *NFI_CON |= BURST_WR | NOB_DWORD ;
    // wait til DATA_WRITE is completely issued
    //while( *NFI_STA  & STATUS_DATAW );

    // program page data
    if( S_DONE != (ret=NAND_COMMON_FIFO_Write(mt85xx, c_timeout, _fgUsingDMA, (u32*)p_data32, page_size)) ) {
        goto end;
    }

    if ( !(*NFI_CNFG & (AUTO_FMT_EN | AHB_MODE)) ) // NFI will automatically fetch data for spare under AHB_MODE, and NFI will
    {                                                                          // also fetch data from FDM register under AUTO_FMT mode
        volatile u32* p_spare32 = (u32*)(mt85xx->buffers->databuf+ page_size);
        u32 i;
        for(i=0; i<spare_size; i+=4, p_spare32++)
        {
            // wait for FIFO has space to enqueue
            // when WR_FULL_MASK flag is poll-down, it means there are at least 4 bytes free space in FIFO.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & WR_FULL_MASK), timeout);
            if( 0 == timeout ) {
                ret = S_TIMEOUT;
                goto end;
            }
            *NFI_DATAW = *p_spare32;
        }
    }

    // <<<<  WARNING!! >>>>
    // 1. You MUST read parity registers before issue program confirm (0x10) command.
    //    Since the parity registers will be clean by NFI after issue program confirm.
    // 2. You MUST wait until the NFI FIFO is empty!
    //    It means all data in the FIFO had been written to NAND flash, and then you can
    //    start to read ECC parity registers.
    //while(!(*NFI_FIFOSTA & WR_EMPTY_MASK));
    //while ( *NFI_ADDRCNTR & 0x3FF);
    timeout = c_timeout;
    NFI_Wait( ((*NFI_ADDRCNTR & 0xF000) >> 12) != (page_size/SECTOR_BYTES) , timeout);
    if( 0 == timeout ) {
        ret = S_TIMEOUT;
        goto end;
    }


    *NFI_INTR_EN |= WR_DONE_EN;
    // program confirm
    *NFI_CMD = NAND_CMD_PAGEPROG;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    timeout = c_timeout;
#ifdef _XOS_ISR_ENABLE_
    if (_bUsingISR)
    {
        i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                      WR_DONE,
                                      &rRecvEvt,
                                      X_EV_OP_OR_CONSUME,
                                      timeout);
        if( i4Val == OSR_TIMEOUT )
        {
            ret = S_TIMEOUT;
            goto end;
        }
    }
    else
#endif //_XOS_ISR_ENABLE_
    {
        //NFI_Wait( (STATUS_BUSY == (*NFI_STA & STATUS_BUSY)), timeout);
        NFI_Wait( (WR_DONE != (*NFI_INTR&WR_DONE)), timeout); //clear INT status
        *NFI_INTR_EN &= ~WR_DONE; // disable INT first
        //i =  *NFI_INTR; // read clear
        if( 0 == timeout) {
          ret = S_TIMEOUT;
          goto end;
        }
    }

    ret = S_DONE;
end:
    *NFI_CON = 0;
    *NFIECC_ENCCON = 0x0;    
    if ( ret != S_DONE)
        printk("\nwrite page error, error code = 0x%x\n", ret);

//    printk("\nPageProgram completed , FDM = 0x%x, ret = 0x%x\n", nfi_readl(NFI_FDM0L), ret);

    return ret;
}

static void mt85xx_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
			      int column, int page_addr)
{
//    register struct nand_chip *this = mtd->priv;
    unsigned long    page_size = mtd->writesize;
    _data_buf_index = 0;

    if (command == NAND_CMD_ERASE2 ) {
        /* Second half of a command we already calculated */
        goto do_command;
    }

    /* Emulate NAND_CMD_READOOB on large-page chips */
    if (page_size > 512 &&  command == NAND_CMD_READOOB) {
        //column += mtd->writesize;
        command = NAND_CMD_READ0;
    }

    switch (command)
    {
        case NAND_CMD_READID:
            NAND_COMMON_ReadID(mtd, command);
             break;
             
        case NAND_CMD_READ0:
            NAND_COMMON_PageRead(mtd, command, column, page_addr);
            break;

        case NAND_CMD_STATUS:
            NAND_COMMON_ReadStatus(mtd,MTD_NAND_DEFAULT_TIMEOUT);
            break;

        case NAND_CMD_ERASE1:
            NAND_COMMON_BlockErase(mtd, page_addr);
            break;

        case NAND_CMD_SEQIN:
            NAND_COMMON_SEQIN(mtd,MTD_NAND_DEFAULT_TIMEOUT,page_addr);
            break;

        case NAND_CMD_PAGEPROG:
            NAND_COMMON_PageProgram(mtd,MTD_NAND_DEFAULT_TIMEOUT,page_addr);
            break;

        case NAND_CMD_RESET:
            NAND_COMMON_Reset(mtd, MTD_NAND_DEFAULT_TIMEOUT);
            break;
            
        default:
            printk("\ncommand code = 0x%x\n", command);
            BUG();

    }


do_command:
    return;
}

static void mt85xx_nand_select_chip(struct mtd_info *mtd, int chipnr)
{
            *NFI_CSEL = chipnr;
}
static int mt85xx_nand_read_oob_nonautofmt(struct mtd_info *mtd, struct nand_chip *chip,
			      int page, int sndcmd)
{
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size = mtd->writesize;    
    u32     spare_size = mtd->oobsize;
    u32    column_addr_bytes, row_addr_bytes, addr_cycle;
    u32* p_data32 = (u32*)mt85xx->buffers->databuf;
    u32 i;
    u32 timeout;
    u32 c_timeout = MTD_NAND_DEFAULT_TIMEOUT;
    STATUS_E ret = S_UNKNOWN_ERR;
     volatile u32* p4TmpSpare = (u32*)(chip->oob_poi);
     u32 p4TmpBuf[128];
     volatile u32* p4TmpSpare2 = p4TmpBuf;          
	 _fgAUTO_FMT = FALSE;

    memset(chip->oob_poi, 0xFF, mtd->oobsize);

    if (mtd->writesize > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_READ|READ_MODE |HW_ECC_EN;
    if (!_fgAUTO_FMT)
        *NFI_CNFG &= ~AUTO_FMT_EN  ;
    
    if (page_size == 512)
        *NFI_CNFG |= SEL_SEC_512BYTE;
    
#ifdef  ARCH_MT8530
        if ( CS1 == *NFI_CSEL)
            *NFI_CNFG |= RB_CS1;
#endif    
    *NFI_CON = SEC_NUM(page_size/SECTOR_BYTES);
    if (_fgECCSWCorrect)
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_LOCATE) | DEC_NFI_MODE;
    else
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_CORRECT) | DEC_NFI_MODE;
    
    if ( spare_size/(page_size/SECTOR_BYTES) >= 52)
        *NFIECC_DECCNFG |= DEC_TNUM(_u4ECC_BIT) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + _u4ECC_BIT*14 );
    else if (page_size == 512)
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_4_BITS) | DEC_CS( ((SECTOR_BYTES + 8) << 3) + ECC_4_BITS*14 );
    else
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_12_BITS) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + ECC_12_BITS*14 );

    *NFIECC_DECCON = 0; // reset
    *NFIECC_DECCON = DEC_EN;
    //*NFIECC_DECIRQEN = DEC_IRQEN;
    //i = *NFIECC_DECIRQSTA ; //read clear
    if ( AUTO_FMT_EN & (*NFI_CNFG))
        *NFIECC_FDMADDR = (u32)NFI_FDM0L;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE;
        *NFI_STRADDR = ((u32)p_data32)|0xC0000000;
    }

    // read cmd
    *NFI_CMD = NAND_CMD_READ0;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page;
    //  if( 4 < addr_cycle ) {
    // if addr cycle is more than 4, you have to fill 5th cycle addr
    //    *NFI_ADDRM = (row_addr>>(32-column_addr_bits));
    //  }
    // no. of addr cycle
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );


    // read confirm
    if(mtd->writesize > 512) {
        *NFI_CMD = NAND_CMD_READSTART;
        while( *NFI_STA  & STATUS_BUSY);
    }
    if (_fgUsingDMA)
        *NFI_INTR_EN |= AHB_DONE_EN ;
    i = *NFI_INTR;
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif   

    // set burst read by DWORD
    *NFI_CON |= BURST_RD | NOB_DWORD ;
    // wait til DATA_READ is completely issued
    //while( *NFI_STA  & STATUS_DATAR );
    
    // read page data
    if( S_DONE != (ret=NAND_COMMON_FIFO_Read(mt85xx, c_timeout, _fgUsingDMA, p_data32, page_size)) ) {
        goto end;
    }
    
     // read spare data
    if ( AUTO_FMT_EN & (*NFI_CNFG))
    {
        volatile u32* pFDMAddr = NFI_FDM0L;
         volatile u32 p4TmpSpare2[4];        
        
        timeout = c_timeout;
        NFI_Wait( ((*NFI_ADDRCNTR & 0xF000) >> 12) != (page_size/SECTOR_BYTES) , timeout);
        if( 0 == timeout ) {
            ret = S_TIMEOUT;
            goto end;
        }
    
        // read spare data
        for (i = 0; i < (page_size/SECTOR_BYTES) ; i++)
        {
            p4TmpSpare2[0] = *pFDMAddr++;
            p4TmpSpare2[1] = *pFDMAddr++;
            p4TmpSpare2[2] = *pFDMAddr++;
            p4TmpSpare2[3] = *pFDMAddr++;            
            memcpy((void*)p4TmpSpare,(void*) p4TmpSpare2, FDM_BYTES);
            p4TmpSpare = (u32*)((u32)p4TmpSpare + FDM_BYTES);   
        }

    }
    else if ( !(*NFI_CNFG & (AHB_MODE|AUTO_FMT_EN)))
    {
        for(i=0; i<spare_size; i+=4, p4TmpSpare++) 
        {
            // wait for data ready
            // when RD_EMPTY_MASK flag is poll-down, it means data is ready in FIFO at least 4 bytes.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & RD_EMPTY_MASK), timeout);
            if( 0 == timeout ) {
              ret =  S_TIMEOUT;
              goto end;
            }
            *p4TmpSpare = *NFI_DATAR;
        }
    }
    
    if ( *NFI_CNFG & HW_ECC_EN)
        ret = NAND_COMMON_ECCErrDetect(mtd, c_timeout);
    if ( S_ECC_CORRECTABLE_ERR == ret)
        ret = S_DONE;

//    printk("\n read oob 0x%x oob = 0x%x\n", page, *chip->oob_poi);
    

end:
    // disable burst read
    *NFI_CON = 0x0;
    *NFIECC_DECCON = 0x0;
	_fgAUTO_FMT = TRUE;
	return ret;
}
static int mt85xx_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			      int page, int sndcmd)
{
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size = mtd->writesize;    
    u32     spare_size = mtd->oobsize;
    u32    column_addr_bytes, row_addr_bytes, addr_cycle;
    u32* p_data32 = (u32*)mt85xx->buffers->databuf;
    u32 i;
    u32 timeout;
    u32 c_timeout = MTD_NAND_DEFAULT_TIMEOUT;
    STATUS_E ret = S_UNKNOWN_ERR;
     volatile u32* p4TmpSpare = (u32*)(chip->oob_poi);
     u32 p4TmpBuf[128];
     volatile u32* p4TmpSpare2 = p4TmpBuf;          

    memset(chip->oob_poi, 0xFF, mtd->oobsize);

    if (mtd->writesize > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_READ|READ_MODE |HW_ECC_EN;
    if (_fgAUTO_FMT)
        *NFI_CNFG |= AUTO_FMT_EN  ;
    
    if (page_size == 512)
        *NFI_CNFG |= SEL_SEC_512BYTE;
    
#ifdef  ARCH_MT8530
        if ( CS1 == *NFI_CSEL)
            *NFI_CNFG |= RB_CS1;
#endif    
    *NFI_CON = SEC_NUM(page_size/SECTOR_BYTES);
    if (_fgECCSWCorrect)
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_LOCATE) | DEC_NFI_MODE;
    else
        *NFIECC_DECCNFG = DEC_EMPTY_EN |DEC_CON(ECC_DEC_CORRECT) | DEC_NFI_MODE;
    
    if ( spare_size/(page_size/SECTOR_BYTES) >= 52)
        *NFIECC_DECCNFG |= DEC_TNUM(_u4ECC_BIT) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + _u4ECC_BIT*14 );
    else if (page_size == 512)
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_4_BITS) | DEC_CS( ((SECTOR_BYTES + 8) << 3) + ECC_4_BITS*14 );
    else
        *NFIECC_DECCNFG |= DEC_TNUM(ECC_12_BITS) | DEC_CS( ((SECTOR_BYTES + FDM_ECC_BYTES) << 3) + ECC_12_BITS*14 );

    *NFIECC_DECCON = 0; // reset
    *NFIECC_DECCON = DEC_EN;
    //*NFIECC_DECIRQEN = DEC_IRQEN;
    //i = *NFIECC_DECIRQSTA ; //read clear
    if ( AUTO_FMT_EN & (*NFI_CNFG))
        *NFIECC_FDMADDR = (u32)NFI_FDM0L;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE;
        *NFI_STRADDR = ((u32)p_data32)|0xC0000000;
    }

    // read cmd
    *NFI_CMD = NAND_CMD_READ0;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page;
    //  if( 4 < addr_cycle ) {
    // if addr cycle is more than 4, you have to fill 5th cycle addr
    //    *NFI_ADDRM = (row_addr>>(32-column_addr_bits));
    //  }
    // no. of addr cycle
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );


    // read confirm
    if(mtd->writesize > 512) {
        *NFI_CMD = NAND_CMD_READSTART;
        while( *NFI_STA  & STATUS_BUSY);
    }
    if (_fgUsingDMA)
        *NFI_INTR_EN |= AHB_DONE_EN ;
    i = *NFI_INTR;
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif   

    // set burst read by DWORD
    *NFI_CON |= BURST_RD | NOB_DWORD ;
    // wait til DATA_READ is completely issued
    //while( *NFI_STA  & STATUS_DATAR );
    
    // read page data
    if( S_DONE != (ret=NAND_COMMON_FIFO_Read(mt85xx, c_timeout, _fgUsingDMA, p_data32, page_size)) ) {
        goto end;
    }
    
     // read spare data
    if ( AUTO_FMT_EN & (*NFI_CNFG))
    {
        volatile u32* pFDMAddr = NFI_FDM0L;
         volatile u32 p4TmpSpare2[4];        
        
        timeout = c_timeout;
        NFI_Wait( ((*NFI_ADDRCNTR & 0xF000) >> 12) != (page_size/SECTOR_BYTES) , timeout);
        if( 0 == timeout ) {
            ret = S_TIMEOUT;
            goto end;
        }
    
        // read spare data
        for (i = 0; i < (page_size/SECTOR_BYTES) ; i++)
        {
            p4TmpSpare2[0] = *pFDMAddr++;
            p4TmpSpare2[1] = *pFDMAddr++;
            p4TmpSpare2[2] = *pFDMAddr++;
            p4TmpSpare2[3] = *pFDMAddr++;            
            memcpy((void*)p4TmpSpare,(void*) p4TmpSpare2, FDM_BYTES);
            p4TmpSpare = (u32*)((u32)p4TmpSpare + FDM_BYTES);   
        }

    }
    else if ( !(*NFI_CNFG & (AHB_MODE|AUTO_FMT_EN)))
    {
        for(i=0; i<spare_size; i+=4, p4TmpSpare++) 
        {
            // wait for data ready
            // when RD_EMPTY_MASK flag is poll-down, it means data is ready in FIFO at least 4 bytes.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & RD_EMPTY_MASK), timeout);
            if( 0 == timeout ) {
              ret =  S_TIMEOUT;
              goto end;
            }
            *p4TmpSpare = *NFI_DATAR;
        }
    }
    
    if ( *NFI_CNFG & HW_ECC_EN)
        ret = NAND_COMMON_ECCErrDetect(mtd, c_timeout);
    if ( S_ECC_CORRECTABLE_ERR == ret)
        ret = S_DONE;

//    printk("\n read oob 0x%x oob = 0x%x\n", page, *chip->oob_poi);
    

end:
    // disable burst read
    *NFI_CON = 0x0;
    *NFIECC_DECCON = 0x0;
    if (S_DONE != ret)
    {
      ret = mt85xx_nand_read_oob_nonautofmt(mtd, chip, page,sndcmd);
	  return ret;
	  
      /*  for ( i = 0 ; i < FDM_BYTES * page_size/SECTOR_BYTES ; i ++)
        {
            if ( chip->oob_poi[i] != 0xFF)
            {
                memset(chip->oob_poi, 0x00, mtd->oobsize);
                printk("\nread oob page 0x%x, error code = 0x%x\n", *NFI_ROWADDR, ret);
                return -1;            
            }
        }
        ret = S_DONE;  */
    }    
       
    return 0;

}


static int mt85xx_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			       uint8_t *buf)
{
//    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size = mtd->writesize;    
    u32     spare_size = mtd->oobsize;
    u32    column_addr_bytes, row_addr_bytes, addr_cycle;
    //u32* p_data32 = (u32*)chip->buffers->databuf;
    //u32* p_data32 = (u32*)buf;
    u32 i;
    u32 timeout;
    u32 c_timeout = MTD_NAND_DEFAULT_TIMEOUT;
    STATUS_E ret = S_UNKNOWN_ERR;
     volatile u32* p4TmpSpare = (u32*)(chip->oob_poi);
     u32 p4TmpBuf[128];
     volatile u32* p4TmpSpare2 = p4TmpBuf;     

    if (mtd->writesize > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    if (_fgUsingDMA)
    {
        //*NFI_STRADDR = (u32)p_data32|0xC0000000;
        *NFI_STRADDR = (u32)buf|0xC0000000; // the AHB start address must be sat before the *NFI_ADDRNOB for auto ecc correct address
    }

    // no. of addr cycle
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );


    // read confirm
    if(mtd->writesize > 512) {
        *NFI_CMD = NAND_CMD_READSTART;
        while( *NFI_STA  & STATUS_BUSY);
    }
    if (_fgUsingDMA)
        *NFI_INTR_EN |= AHB_DONE_EN ;
    i = *NFI_INTR;
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif       


    // set burst read by DWORD
    *NFI_CON |= BURST_RD | NOB_DWORD ;
    // wait til DATA_READ is completely issued
    //while( *NFI_STA  & STATUS_DATAR );
    
    // read page data
    if( S_DONE != (ret=NAND_COMMON_FIFO_Read(mtd->priv, c_timeout, _fgUsingDMA, buf, page_size)) ) {
        goto end;
    }
    
     // read spare data
    if ( AUTO_FMT_EN & (*NFI_CNFG))
    {
        volatile u32* pFDMAddr = NFI_FDM0L;
         volatile u32 p4TmpSpare2[4];
        
        timeout = c_timeout;
        NFI_Wait( ((*NFI_ADDRCNTR & 0xF000) >> 12) != (page_size/SECTOR_BYTES) , timeout);
        if( 0 == timeout ) {
            ret = S_TIMEOUT;
            goto end;
        }
    
        // read spare data
        for (i = 0; i < (page_size/SECTOR_BYTES) ; i++)
        {
            p4TmpSpare2[0] = *pFDMAddr++;
            p4TmpSpare2[1] = *pFDMAddr++;
            p4TmpSpare2[2] = *pFDMAddr++;
            p4TmpSpare2[3] = *pFDMAddr++;            
            memcpy((void*)p4TmpSpare,(void*) p4TmpSpare2, FDM_BYTES);
            p4TmpSpare = (u32*)((u32)p4TmpSpare + FDM_BYTES);   
        }
        
    }
    else if ( !(*NFI_CNFG & (AHB_MODE|AUTO_FMT_EN)))
    {
        for(i=0; i<spare_size; i+=4, p4TmpSpare++) 
        {
            // wait for data ready
            // when RD_EMPTY_MASK flag is poll-down, it means data is ready in FIFO at least 4 bytes.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & RD_EMPTY_MASK), timeout);
            if( 0 == timeout ) {
              ret =  S_TIMEOUT;
              goto end;
            }
            *p4TmpSpare = *NFI_DATAR;
        }
    }
    
    if ( *NFI_CNFG & HW_ECC_EN)
        ret = NAND_COMMON_ECCErrDetect(mtd, c_timeout);
    if ( S_ECC_CORRECTABLE_ERR == ret)
        ret = S_DONE;

//    memcpy(mt85xx->dmabuf+page_size, chip->oob_poi, spare_size);
//    printk("\n read page 0x%x oob = 0x%x\n", *NFI_ROWADDR, *chip->oob_poi);    

end:
    // disable burst read
    *NFI_CON = 0x0;
    *NFIECC_DECCON = 0x0;

    if (S_DONE != ret)
    {
        for ( i = 0 ; i < FDM_BYTES * page_size/SECTOR_BYTES ; i ++)
        {
            if ( chip->oob_poi[i] != 0xFF)
            {
                memset(chip->oob_poi, 0x00, mtd->oobsize);
                printk("\nread page 0x%x, error code = 0x%x\n", *NFI_ROWADDR, ret);
                return -1;            
            }
        }
        memset(buf, 0xFF, page_size);
        ret = S_DONE;  
        return 0;
    }  

    //memcpy(buf, p_data32, page_size);
  
    return 0;

}

int mt85xx_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf, int page, int cached, int raw)
{
    register struct nand_chip *mt85xx = mtd->priv;
    u32    page_size, spare_size;
    u32    column_addr_bytes,row_addr_bytes;
    u32    addr_cycle;
    u32     timeout;
    u32 c_timeout = MTD_NAND_DEFAULT_TIMEOUT;
    //  uint32    io_interface;
    STATUS_E  ret=S_UNKNOWN_ERR;
    u32 i;
    //u32* p_data32 = (u32*)buf;
    //u32* p_data32 = (u32*)mt85xx->buffers->databuf;
#ifdef _XOS_ISR_ENABLE_
    s32 i4Val;
    EV_GRP_EVENT_T rRecvEvt;
#endif //_XOS_ISR_ENABLE_

    //ASSERT(!((UINT32)p_data32%0x4)); //assert

    //if (_fgDataInvert && _bAUTO_FMT)  //  _bAUTO_FMT to avoid the recursive loop
    //    return NAND_PATCH_PageProgram(nand_info,c_timeout, row_addr, p_data32, p_spare32);
    page_size =  mtd->writesize;
    spare_size = mtd->oobsize;

    if (mtd->writesize > 512)
    {
        addr_cycle = 5;
        column_addr_bytes = 2;
    }
    else
    {
        addr_cycle = 3;
        column_addr_bytes = 1;     
    }
    row_addr_bytes = addr_cycle - column_addr_bytes;

    // reset the NFI core state machine, data FIFO and flushing FIFO
    *NFI_CON = NFI_RST | FIFO_FLUSH;
    *NFI_CNFG = OP_PROGRAM ;
    if (_fgAUTO_FMT)
    *NFI_CNFG |= AUTO_FMT_EN | HW_ECC_EN;
    if (page_size == 512)
        *NFI_CNFG |= SEL_SEC_512BYTE;    
    
    if ( spare_size/(page_size/SECTOR_BYTES) >= 52)
        *NFIECC_ENCCNFG = ENC_TNUM(_u4ECC_BIT) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;
    else if (page_size == 512)
        *NFIECC_ENCCNFG = ENC_TNUM(ECC_4_BITS) | ENC_MS( (SECTOR_BYTES + 8) << 3 ) | ENC_NFI_MODE;
    else
        *NFIECC_ENCCNFG = ENC_TNUM(ECC_12_BITS) | ENC_MS( (SECTOR_BYTES + FDM_ECC_BYTES) << 3 ) | ENC_NFI_MODE;

    *NFIECC_ENCCON = 0;
    *NFIECC_ENCCON = ENC_EN;
    if (_fgUsingDMA)
    {
        *NFI_CNFG |= AHB_MODE ;
        *NFI_STRADDR = ((u32)buf)|0xC0000000;
    }

    // in most 512 page size NAND flash, you have to setup destination pointer to 1st half area
    if(page_size <=512) {
        *NFI_CMD = NAND_CMD_READ0;
        // wait til CMD is completely issued
        while( *NFI_STA  & STATUS_CMD );
        *NFI_CON = NFI_RST;
        while( *NFI_STA  & STATUS_CMD );
    }
    *NFI_CON = SEC_NUM(page_size/SECTOR_BYTES);
    // program cmd
    *NFI_CMD = NAND_CMD_SEQIN;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    // fill 1~4 cycle addr
    *NFI_COLADDR = 0;
    *NFI_ROWADDR = page;
    // no. of addr cycle
    //*NFI_ADDRNOB = addr_cycle;
    *NFI_ADDRNOB = COL_ADDR_NOB(column_addr_bytes) | ROW_ADDR_NOB(row_addr_bytes);
    // wait til ADDR is completely issued
    while( *NFI_STA  & STATUS_ADDR );

    // prepare FDM data
    if ( AUTO_FMT_EN & (*NFI_CNFG))
    {
        volatile u32* pFDMAddr = NFI_FDM0L;
        volatile u32* pTmpAddr = (u32*)(mt85xx->buffers->databuf+ page_size);
        u32 p4TmpBuf[128];

        for (i = 0; i < (page_size/SECTOR_BYTES) ; i++)
        {
            memcpy((void*)p4TmpBuf,(void*) pTmpAddr,FDM_BYTES);
            *pFDMAddr++ = p4TmpBuf[0];
            *pFDMAddr++ = p4TmpBuf[1];
            *pFDMAddr++ = p4TmpBuf[2];
            *pFDMAddr++ = p4TmpBuf[3];            
            pTmpAddr = (u32*)((u32)pTmpAddr + FDM_BYTES);
        }    

    }
    if (_fgUsingDMA)
        *NFI_INTR_EN = AHB_DONE_EN ;
    i = *NFI_INTR;  // read clear
#ifdef INT_WR_CLR
    *NFI_INTR = i;
#endif    
    // set burst program by DWORD
    *NFI_CON |= BURST_WR | NOB_DWORD ;
    // wait til DATA_WRITE is completely issued
    //while( *NFI_STA  & STATUS_DATAW );

    // program page data
    if( S_DONE != (ret=NAND_COMMON_FIFO_Write(mtd->priv, c_timeout, _fgUsingDMA, (u32*)buf, page_size)) ) {
        goto end;
    }

    if ( !(*NFI_CNFG & (AUTO_FMT_EN | AHB_MODE)) ) // NFI will automatically fetch data for spare under AHB_MODE, and NFI will
    {                                                                          // also fetch data from FDM register under AUTO_FMT mode
        volatile u32* p_spare32 = (u32*)(buf + mtd->writesize);
        u32 i;
        for(i=0; i<spare_size; i+=4, p_spare32++)
        {
            // wait for FIFO has space to enqueue
            // when WR_FULL_MASK flag is poll-down, it means there are at least 4 bytes free space in FIFO.
            timeout = c_timeout;
            NFI_Wait( (*NFI_FIFOSTA & WR_FULL_MASK), timeout);
            if( 0 == timeout ) {
                ret = S_TIMEOUT;
                goto end;
            }
            *NFI_DATAW = *p_spare32;
        }
    }

    // <<<<  WARNING!! >>>>
    // 1. You MUST read parity registers before issue program confirm (0x10) command.
    //    Since the parity registers will be clean by NFI after issue program confirm.
    // 2. You MUST wait until the NFI FIFO is empty!
    //    It means all data in the FIFO had been written to NAND flash, and then you can
    //    start to read ECC parity registers.
    //while(!(*NFI_FIFOSTA & WR_EMPTY_MASK));
    //while ( *NFI_ADDRCNTR & 0x3FF);
    timeout = c_timeout;
    NFI_Wait( ((*NFI_ADDRCNTR & 0xF000) >> 12) != (page_size/SECTOR_BYTES) , timeout);
    if( 0 == timeout ) {
        ret = S_TIMEOUT;
        goto end;
    }


    *NFI_INTR_EN |= WR_DONE_EN;
    // program confirm
    *NFI_CMD = NAND_CMD_PAGEPROG;
    // wait til CMD is completely issued
    while( *NFI_STA  & STATUS_CMD );

    timeout = c_timeout;
#ifdef _XOS_ISR_ENABLE_
    if (_bUsingISR)
    {
        i4Val = x_ev_group_wait_event_timeout(_hNFIEvent,
                                      WR_DONE,
                                      &rRecvEvt,
                                      X_EV_OP_OR_CONSUME,
                                      timeout);
        if( i4Val == OSR_TIMEOUT )
        {
            ret = S_TIMEOUT;
            goto end;
        }
    }
    else
#endif //_XOS_ISR_ENABLE_
#ifdef LINUX_ISR_ENABLE
    if (_fgUsingISR)
    {
        struct mt85xx_priv *mt85xx = mtd->priv;
        wait_for_completion((struct completion*)&mt85xx->program_completion);
    }
    else
#endif
    {
        //NFI_Wait( (STATUS_BUSY == (*NFI_STA & STATUS_BUSY)), timeout);
        NFI_Wait( (WR_DONE != (*NFI_INTR&WR_DONE)), timeout); //clear INT status
        *NFI_INTR_EN &= ~WR_DONE; // disable INT first
        //i =  *NFI_INTR; // read clear
        if( 0 == timeout) {
          ret = S_TIMEOUT;
          goto end;
        }
    }

    ret = S_DONE;
end:
    *NFI_CON = 0;
    *NFIECC_ENCCON = 0x0;    
    if ( ret != S_DONE)
    {
        printk("\nmt85xx_nand_write_page, error code = 0x%x\n", ret);
        return -1;
    }

    /////////
    //  test function
    ///
/*
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
        NAND_COMMON_PageRead(mtd, NAND_CMD_READ0, 0 , page);
        if (-1==mt85xx_nand_read_page(mtd, chip,mt85xx->dmabuf))
            printk("read verify failed at page 0x%x\n", page);
#endif
*/
#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
			if (((page * page_size) >= 0x9A00000) && ((page * page_size) <= 0xA600000))
			{
				NAND_COMMON_PageRead(mtd, NAND_CMD_READ0, 0 , page);
				if (-1==mt85xx_nand_read_page(mtd, chip,mt85xx->buffers->databuf))
				{
					printk("read verify read failed at page 0x%x\n", page);
					BUG();
					return -EIO;
				}
				if (memcmp(mt85xx->buffers->databuf, buf, page_size)!=0)
				{
					printk("read verify compare failed at page 0x%x\n", page);
					BUG();
					return -EIO;
				} 
			}
#endif

    return 0;
}

static void mt85xx_nand_read_buf(struct mtd_info *mtd, unsigned char *buf, int len)
{
    register struct nand_chip *this = mtd->priv;

//    printk("\nread buffer, offset= 0x%x\n", mt85xx->datalen);

        memcpy(buf, this->buffers->databuf+_data_buf_index, len);

	_data_buf_index+= len;
    
}

static void mt85xx_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
    register struct nand_chip *this = mtd->priv;

//        printk("\nwrite buffer, offset= 0x%x\n", mt85xx->datalen);
    memcpy(this->buffers->databuf + _data_buf_index, buf, len);

    _data_buf_index += len;

}


static u_char mt85xx_nand_read_byte(struct mtd_info *mtd)
{
	unsigned char d;

	mt85xx_nand_read_buf(mtd, &d, 1);

	return d;
}

static u16 mt85xx_nand_read_word(struct mtd_info *mtd)
{
	u16 d;

	mt85xx_nand_read_buf(mtd, (u_char*)&d, 2);

	return d;
}

int mt85xx_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
    return 0;
}

int mt85xx_nand_correct_data(struct mtd_info *mtd, u_char *dat, u_char *read_ecc, u_char *calc_ecc)
{
    return 0;
}

static void mt85xx_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
}


int board_nand_init(struct nand_chip *nand)
{

    nand->options |= NAND_USE_FLASH_BBT| NAND_NO_SUBPAGE_WRITE;
#if 0
	if (!nand->buffers->databuf) {
//		size_t len;
//		len = mtd->writesize + mtd->oobsize;
		nand->buffers->databuf = kmalloc (4096+1024, GFP_KERNEL);
		if (!nand->buffers->databuf) {
			if (nand->options & NAND_OOBBUF_ALLOC)
				kfree (nand->oob_buf);
			printk (KERN_ERR "nand_scan(): Cannot allocate data_buf\n");
    return  -ENOMEM;
    }
		nand->options |= NAND_DATABUF_ALLOC;
	}
#endif
    *NFI_CLK_SEL |= NFI_CLK;
//    *NFI_CLK_SEL |= 0;
    *NFI_ACCCON = (LCD2NAND| PRECS|C2R | W2R | WH | WST | RLT);


    /* Set address of NAND IO lines (Using Linear Data Access Region) */
    nand->IO_ADDR_R = (void __iomem *) CONFIG_SYS_NAND_BASE;
    nand->IO_ADDR_W = (void __iomem *) CONFIG_SYS_NAND_BASE;
    /* Reference hardware control function */
    //nand->hwcontrol  = sc3_nand_hwcontrol;
    nand->dev_ready  = mt85xx_nand_device_ready;
    nand->select_chip = mt85xx_nand_select_chip;

    //nand->write_byte = nand_write_byte16 : nand_write_byte;
    nand->read_byte = mt85xx_nand_read_byte;
    //nand->write_word = nand_write_word;
    nand->read_word = mt85xx_nand_read_word;
    //nand->write_buf = nand_write_buf16 : nand_write_buf;
    nand->read_buf = mt85xx_nand_read_buf;
    nand->write_buf = mt85xx_nand_write_buf;

    nand->cmdfunc = mt85xx_nand_cmdfunc;
    nand->ecc.mode = NAND_ECC_HW;

    nand->write_page = mt85xx_nand_write_page;

    nand->ecc.calculate = mt85xx_nand_calculate_ecc;
    nand->ecc.correct = mt85xx_nand_correct_data;
    nand->ecc.hwctl = mt85xx_nand_enable_hwecc;
    nand->ecc.read_page = mt85xx_nand_read_page;
    nand->ecc.read_page_raw = mt85xx_nand_read_page;
    nand->ecc.read_oob = mt85xx_nand_read_oob;

    nand->ecc.bytes = 12;
    nand->ecc.size = 1024;

//    nand->bbt_td		= &mt85xx_bbt_main_descr;
//    nand->bbt_md		= &mt85xx_bbt_mirror_descr;


	return 0;
    }

int mt85xx_nand_init(struct mtd_info *mtd)
{
//    unsigned long    page_size = mtd->writesize;
    struct nand_chip *this = mtd->priv;
    u32 spare_size =0;

    *NFI_PAGEFMT = FDM_ECC_NUM(FDM_ECC_BYTES) |FDM_NUM(FDM_BYTES);
      // setup NFI page format and I/O interface

    *NFI_PAGEFMT |= PAGEFMT_8BITS ;
      /*
    if ( NAND_IO_16BITS == io_interface )
      *NFI_PAGEFMT |= PAGEFMT_16BITS ;
    else
      *NFI_PAGEFMT |= PAGEFMT_8BITS ;*/
    
    switch (mtd->writesize)
    {
      case 512:
          *NFI_CNFG |= SEL_SEC_512BYTE;
          *NFI_PAGEFMT |= PAGEFMT_2K_512;
        break;
      case 2048:
          *NFI_PAGEFMT |= PAGEFMT_2K_512;
        break;
      case 4096:
          *NFI_PAGEFMT |= PAGEFMT_4K_2K;
        break;
       case 8192:
           *NFI_PAGEFMT |= PAGEFMT_8K_4K;
        break;       
    }
    
    spare_size = mtd->oobsize /(mtd->writesize/SECTOR_BYTES);
    switch (spare_size)
      {
      case 16:
          *NFI_PAGEFMT |= SPARE_32_16;
        break;
        case 32:
          *NFI_PAGEFMT |= SPARE_32_16;
          break;
          
        default:
          *NFI_PAGEFMT |= SPARE_52_26;
          break;

      }

        /* If no default placement scheme is given, select an
         * appropriate one */
        //if (!mtd->autooob) {
            /* Select the appropriate default oob placement scheme for
             * placement agnostic filesystems */
    switch (mtd->oobsize) {
        case 16:
            this->ecc.layout = &mt85xx_oobinfo_512;
            break;
        case 64:
            this->ecc.layout = &mt85xx_oobinfo_2048;
            break;
        default:
            printk (KERN_WARNING "No oob scheme defined for oobsize %d\n",
            mtd->oobsize);
            /*          BUG(); */
    }
    this->badblock_pattern = (mtd->writesize > 512) ?
			&mt85xx_largepage_bbt_descr : &mt85xx_smallpage_bbt_descr;
    

    return 0;
}


