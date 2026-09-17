#include <common.h>
#include <command.h>
#include <malloc.h>
#include <asm-arm/arch-mt8530/x_typedef.h>
#include <asm-arm/arch-mt8530/x_bim.h>

#include "chip_ver.h"


#include <ir/ir_regs.h> // u_boot ir
#include <ir/drv_ir.h>
#include <ir/ir_struct.h>

#if CONFIG_CMD_IR

typedef struct IR_DATA
{
    UINT32 u4CrystalKey ;
	UINT32 u4Info;
	UINT32 au4IrRxData[MAX_IRRX_DATA];
}IR_DATA_T;

void IRRX_GetData(void);
//UINT32 IRRX_PollIR(void);
void _IRHW_RxClear(void);
void IRHW_RxInit(INT32 i4Config, INT32 i4SaPeriod, INT32 i4Threshold);
void IRHW_RxWrConf(INT32 i4Config, INT32 i4SaPeriod, INT32 i4Threshold);
UINT32 _IRRX_XferMtkToCrystal(UINT32 u4Info, const UINT8 * pu1Data);
void detect_ir(void);
void ir_set_interrupt(int i4_enable_int);
extern int run_command (const char *cmd, int flag); //default ir function
static int ir_debug_mode  = 0;
static int ir_INT_times = 0;
static IR_DATA_T ir_data;

#if 0
#define T64B_GET_LOW()	BIM_READ32(REG_RW_T64b_LO_0) //64 bit timer low
#define TIMER_INTERVAL (189000)	//(189000000 / 1000)

void mdelay(int ms)
{
	unsigned long start = T64B_GET_LOW();
	while( ((T64B_GET_LOW() - start)>>17) < ms)
	{
	 
	}
	
}
#endif

#if (IRRX_RC_PROTOCOL == IRRX_RC_NEC)

static IR_FUNCTION_T ir_fun_t[] =
{
  {BTN_KEY_REPEAT,"BTN_KEY_REPEAT","run_command",run_command,"help", 0},   
};

IR_GLOBAL_T g_glb_ir = 
{ir_fun_t,sizeof(ir_fun_t)/sizeof(ir_fun_t[0]), "NEC"};

#endif

int IRRX_Init(void)
{   
    int i;
    printf("ir init\n");
	ir_data.u4CrystalKey = BTN_NONE;
	ir_data.u4Info = BTN_NONE;
	for (i = 0; i<MAX_IRRX_DATA; i++ )
	{
		ir_data.au4IrRxData[i] = 0;
	}	
    #if (IRRX_RC_PROTOCOL == IRRX_RC_NEC)	 
    IRHW_RxInit(MTK_IRRX_CONFIG, MTK_IRRX_SAPERIOD, MTK_IRRX_THRESHOLD);
	#endif	
	return 0;
}

void IRRX_GetData(void)
{
    if( IR_READ32(IRRX_INTSTA) & IR_INT )// ir interrupt
    {
        /* read IRRX IRQ data. */
		ir_INT_times ++;
        ir_data.u4Info= IR_READ32(IRRX_COUNT_HIGH_REG);
        ir_data.au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
        ir_data.au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);
		_IRHW_RxClear();
    }
	
}
#if 0
UINT32 IRRX_PollIR(void)
{
/*
    UINT32 u4CrystalKey = BTN_NONE;
	UINT32 u4Info;
	UINT32 au4IrRxData[MAX_IRRX_DATA];
*/
    if( IR_READ32(IRRX_INTSTA) & IR_INT )// ir interrupt
    {
        /* read IRRX IRQ data. */
		ir_INT_times ++;
        ir_data.u4Info= IR_READ32(IRRX_COUNT_HIGH_REG);
        ir_data.au4IrRxData[0] = IR_READ32(IRRX_COUNT_MID_REG);
        ir_data.au4IrRxData[1] = IR_READ32(IRRX_COUNT_LOW_REG);
 
	  //if(ir_debug_mode) 
	   //{
		 printf("[ir]RxIsr Info:0x%08x data: 0x%08x%08x\n",u4Info, au4IrRxData[1], au4IrRxData[0]);
	  // }
	  if (ir_data.u4Info != 0)
        { 
          #if (IRRX_RC_PROTOCOL == IRRX_RC_NEC )
            ir_data.u4CrystalKey= _IRRX_XferMtkToCrystal(ir_data.u4Info, (UINT8 *)(ir_data.au4IrRxData));
          #endif		 
        }    
        _IRHW_RxClear();
    }
	else
	{
	  if(ir_debug_mode) 
	   {
		 printf("[ir] no ir signal detect!!!");
	   }
	}
    return u4CrystalKey;
}
#endif

 void detect_ir(void)//  do things if ir detect times ok
 {  
	int  i;
    IR_FUNCTION_T *pfun = NULL;
    static int ir_INIT = 0;
	
	if (ir_INT_times == 0) //no ir key detected
	{
		return;
	}
	else if ( (ir_INIT == 0) && (ir_INT_times !=0)) // when function first be called
	{
		ir_INIT = ir_INT_times;
	}
    else if ( (ir_INIT !=0) && (ir_INT_times == ir_INIT) ) // in main_loop, when this function be  called again, no ir key detected
    {
    	return;
    }
	else if (ir_INT_times != ir_INIT) // in main_loop , we get a new ir interrupt
	{
		ir_INIT = ir_INT_times;
	}
	    
	printf("[ir]RxIsr Info:0x%08x data: 0x%08x%08x\n",
		   ir_data.u4Info, 
		   ir_data.au4IrRxData[1], 
		   ir_data.au4IrRxData[0]);

	if (ir_data.u4Info != 0)
	{ 
		#if (IRRX_RC_PROTOCOL == IRRX_RC_NEC )
	    ir_data.u4CrystalKey= _IRRX_XferMtkToCrystal(ir_data.u4Info, (UINT8 *)(ir_data.au4IrRxData));
		#endif 		
	}  

	if (ir_debug_mode) // we only debug ir_key in u_boot cli mode;
	{
		return ;
	}
	
   	for (i = 0; i< g_glb_ir.i4_fun_num ;i++ )
	{
	  pfun = &(g_glb_ir.g_ir_fun_t)[i];
	  
	  if (ir_data.u4CrystalKey == pfun->ui4_key)
	  {     
		  	pfun->handle(pfun->cmd, pfun->flag);
		    break;		 
	  }
	}
 	
 }

 UINT32 _IRRX_XferMtkToCrystal(UINT32 u4Info, const UINT8 * pu1Data)
{
    UINT32 u4GrpId, u4BitCnt;

    u4BitCnt = INFO_TO_BITCNT(u4Info);
    if ((u4BitCnt == 0) || (pu1Data == NULL))
    { 
       return BTN_NONE;
    }

	if (u4BitCnt == MTK_IRRX_BITCNT_REPEAT)
    {
        if (((INFO_TO_1STPULSE(u4Info) == MTK_IRRX_1st_Plus_REPEAT) ||
        	   (INFO_TO_1STPULSE(u4Info) == MTK_IRRX_1st_Plus_REPEAT - 1) ||
             (INFO_TO_1STPULSE(u4Info) == MTK_IRRX_1st_Plus_REPEAT + 1)) &&
            (INFO_TO_2NDPULSE(u4Info) == 0) &&
            (INFO_TO_3RDPULSE(u4Info) == 0))
        {
			return BTN_KEY_REPEAT;               
        }
        else
        {
            if (ir_debug_mode)
            printf("u4BitCnt != %d\n",MTK_IRRX_BITCNT_NORMAL);
            return BTN_NONE;
        }
    }
    /* Check invalid pulse. */
    if (u4BitCnt != MTK_IRRX_BITCNT_NORMAL)
    {    
         if (ir_debug_mode)
         printf("u4BitCnt != %d\n",MTK_IRRX_BITCNT_NORMAL);
         return BTN_NONE;
    }

    /* Check GroupId. */
    u4GrpId = pu1Data[1];
    u4GrpId = (u4GrpId << 8) + pu1Data[0];
    if (u4GrpId != MTK_IRRX_GRPID_DVD)
    {  
        if (ir_debug_mode)
       	printf("u4GrpId != %d\n",MTK_IRRX_GRPID_DVD);
        return BTN_NONE;
    }

    /* Check invalid key. */
    if ((pu1Data[2] + pu1Data[3]) != MTK_IRRX_BIT8_VERIFY)
    {
        if (ir_debug_mode)
		printf("0x%2x + 0x%2x != 0x%2x\n",pu1Data[2],pu1Data[3], MTK_IRRX_BIT8_VERIFY);
        return BTN_NONE;
    }

    /* Here, pu1Data[2] is the key of MTKDVD remote controller. */
    if (pu1Data[2] >= MTK_NEC_MAX_MAP_ENTRY)
    {
        if (ir_debug_mode)
		printf("0x%2x > 0x%2x \n",pu1Data[2], MTK_IRRX_BIT8_VERIFY);
        return BTN_NONE;
    }

    return BTN_KEY_REPEAT;//_au4MtkCrystalKeyMap[pu1Data[2]];
}


 void _IRHW_RxClear(void)
{
   IR_WRITE32(IRRX_IRCLR, (IR_READ32(IRRX_IRCLR) | IRCLR));
   IR_WRITE32(IRRX_INTCLR, (IR_READ32(IRRX_INTCLR) | IR_INTCLR));
    
}

 void IRHW_RxWrConf(INT32 i4Config, INT32 i4SaPeriod, INT32 i4Threshold)
{
    /* Config IRRX registers */
    IR_WRITE32(IRRX_CONFIG_HIGH_REG, (UINT32) i4Config);
    IR_WRITE32(IRRX_CONFIG_LOW_REG, (UINT32) i4SaPeriod);
    IR_WRITE32(IRRX_THRESHOLD_REG, (UINT32) i4Threshold);
}
 void IRHW_RxInit(INT32 i4Config, INT32 i4SaPeriod, INT32 i4Threshold)
{ 

    
	IR_WRITE32(IRRX_INTEN, (IR_READ32(IRRX_INTEN) & (~IR_INTEN)));

	IR_WRITE32(IRRX_IREXP_EN, 0);	
	IR_WRITE32(IRRX_WAKEN, (IR_READ32(IRRX_WAKEN) & (~IR_WAKEN)));
	
    IRHW_RxWrConf(i4Config, i4SaPeriod, i4Threshold);	 

    IR_WRITE32(IRRX_CLKPDN, (IR_READ32(IRRX_CLKPDN) & (~IRRXPD)));
	
   #if (IRRX_USE_27M)
    //set to 27M 
    IR_WRITE32(IRRX_UP_CFG, (IR_READ32(IRRX_UP_CFG) | FAST_CK_EN));
    IR_WRITE32(IRRX_IRCKSEL, (IR_READ32(IRRX_IRCKSEL) & (~IRCLKSEL_MASK)) | CLK_SEL_IR_DIV_1_256);
   #else
   IR_WRITE32(IRRX_UP_CFG, (IR_READ32(IRRX_UP_CFG) & (~FAST_CK_EN))); // set clock
 
   #if (IRRX_RC_PROTOCOL == IRRX_RC_RC5)
   IR_WRITE32(IRRX_IRCKSEL, (IR_READ32(IRRX_IRCKSEL) & (~IRCLKSEL_MASK)) | CLK_SEL_IR_DIV_1_16); 
   #else
   IR_WRITE32(IRRX_IRCKSEL, (IR_READ32(IRRX_IRCKSEL) & (~IRCLKSEL_MASK)) | CLK_SEL_IR_DIV_1_8);
   #endif
   #endif

   _IRHW_RxClear();   
	//enable IR interrupt
    IR_WRITE32(IRRX_INTEN, (IR_READ32(IRRX_INTEN) | IR_INTEN));
}


int do_ir(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    
	int i;

	printf("[ir]: config protocol = %s\n",g_glb_ir.c_proname);
	for (i = 0; i<g_glb_ir.i4_fun_num; i++)
	{
		printf("[ir]:0x%08x(\"%s\")---%s(\"%s\",%d)\n", 
				(g_glb_ir.g_ir_fun_t)[i].ui4_key,
				(g_glb_ir.g_ir_fun_t)[i].key_name,
				(g_glb_ir.g_ir_fun_t)[i].handle_name,
				(g_glb_ir.g_ir_fun_t)[i].cmd,
				(g_glb_ir.g_ir_fun_t)[i].flag);
	}
	if (argc == 1)
	{
		printf("usage:\n""ir_debug --- exec fun when detect key\n""ir_debug 1 ---only show key \n");
		
	}
	else if (argc == 2)
	{
	   if (strcmp(argv[1], "1") == 0)
		{			
		   ir_debug_mode = 1;		
		}
	}		
	return 1;   
}

U_BOOT_CMD(
    ir_debug,    2,  1,  do_ir,
    "debug ir key \n",
);

#endif /* (CONFIG_CMD_OTA) */

