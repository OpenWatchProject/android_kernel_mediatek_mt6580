#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/upmu_hw.h>
#include <mach/mt_pmic_wrap.h>
#include "pwrap_hal.h"
#include "pwrap_ldvt.h"
#include "register_read_write_test.h"

#define PWRAP_LDVT_NAME	"pwrap_ldvt"

/*-----start-- global variable-------------------------------------------------*/
#ifdef CONFIG_OF
extern void __iomem *pwrap_base;
extern void __iomem *topckgen_base;
extern void __iomem *toprgu_reg_base;
u32 pwrap_irq;
#endif

/* interral API */
static S32 _pwrap_init_dio(U32 dio_en);
static S32 _pwrap_init_cipher(void);
static S32 _pwrap_init_reg_clock(U32 regck_sel);

static S32 _pwrap_wacs2_nochk(U32 write, U32 adr, U32 wdata, U32 *rdata);
static S32 pwrap_write_nochk(U32  adr, U32  wdata);
static S32 pwrap_read_nochk(U32  adr, U32 *rdata);

/*
S32 _pwrap_wacs2_nochk(unsigned int write, unsigned int adr, unsigned int wdata, unsigned int *rdata);
S32 _pwrap_reset_spislv(void);
S32 _pwrap_init_sistrobe(void);
inline unsigned int wait_for_state_ready_init(loop_condition_fp fp,unsigned int timeout_us,void *wacs_register,unsigned int *read_reg);
inline unsigned int wait_for_state_idle_init(loop_condition_fp fp,unsigned int timeout_us,void *wacs_register,unsigned int wacs_vldclr_register,unsigned int *read_reg);
inline unsigned int wait_for_state_idle(loop_condition_fp fp,unsigned int timeout_us,void *wacs_register,unsigned int wacs_vldclr_register,unsigned int *read_reg);
inline unsigned int wait_for_state_ready(loop_condition_fp fp,unsigned int timeout_us,void *wacs_register,unsigned int *read_reg);
inline unsigned int wait_for_fsm_vldclr(unsigned int x);
inline unsigned int wait_for_fsm_idle(unsigned int x);
inline unsigned int wait_for_man_vldclr(unsigned int x);
*/

static DEFINE_SPINLOCK(wacs0_lock);
static DEFINE_SPINLOCK(wacs1_lock);
static DECLARE_COMPLETION(pwrap_done);

#define WACS0_TEST_REG MT6350_DEW_WRITE_TEST
#define WACS1_TEST_REG MT6350_ISINK0_CON1
#define WACS2_TEST_REG MT6350_ISINK1_CON1

/* TODO:
 * 1. implement pwrap_wacs0, pwrap_wacs1
 * 2. add wait_for_state_xxx API
 * 3. add timer API
 * 4. redirect interrupt handler to LDVT driver
 * 5. Both PMIC1 and PMIC2 should be tested
 * 6. Align Linux kernel coding style
 */

static U64 _pwrap_get_current_time(void)
{
	return sched_clock();   ///TODO: fix me
}
//U64 elapse_time=0;

static BOOL _pwrap_timeout_ns (U64 start_time_ns, U64 timeout_time_ns)
{
	U64 cur_time=0;
	U64 elapse_time=0;

	// get current tick
	cur_time = _pwrap_get_current_time();//ns

	//avoid timer over flow exiting in FPGA env
	if(cur_time < start_time_ns){
		PWRAPERR("@@@@Timer overflow! start%lld cur timer%lld\n",start_time_ns,cur_time);
		start_time_ns=cur_time;
		timeout_time_ns=255*1000; //255us
		PWRAPERR("@@@@reset timer! start%lld setting%lld\n",start_time_ns,timeout_time_ns);
	}
		
	elapse_time=cur_time-start_time_ns;

	// check if timeout
	if (timeout_time_ns <= elapse_time)
	{
		// timeout
		PWRAPERR("@@@@Timeout: elapse time%lld,start%lld setting timer%lld\n",
				elapse_time,start_time_ns,timeout_time_ns);
		return TRUE;
	}
	return FALSE;
}

static U64 _pwrap_time2ns (U64 time_us)
{
	return time_us*1000;
}

static inline U32 wait_for_state_ready_init(loop_condition_fp fp,U32 timeout_us,void *wacs_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata=0x0;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait_for_state_ready_init timeout when waiting for idle\n");
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}

static inline U32 wait_for_state_ready(loop_condition_fp fp,U32 timeout_us,void *wacs_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("timeout when waiting for idle\n");
			//pwrap_dump_ap_register();
			//pwrap_trace_wacs2();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);

		if( GET_INIT_DONE0( reg_rdata ) != WACS_INIT_DONE)
		{
			PWRAPERR("initialization isn't finished \n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}

static inline U32 wait_for_state_idle(loop_condition_fp fp,U32 timeout_us,void *wacs_register,void *wacs_vldclr_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait_for_state_idle timeout when waiting for idle\n");
			//pwrap_dump_ap_register();
			//pwrap_trace_wacs2();
			//BUG_ON(1);
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if( GET_INIT_DONE0( reg_rdata ) != WACS_INIT_DONE)
		{
			PWRAPERR("initialization isn't finished \n");
			return E_PWR_NOT_INIT_DONE;
		}
		//if last read command timeout,clear vldclr bit
		//read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle
		switch ( GET_WACS0_FSM( reg_rdata ) )
		{
			case WACS_FSM_WFVLDCLR:
				WRAP_WR32(wacs_vldclr_register , 1);
				PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
				break;
			case WACS_FSM_WFDLE:
				PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
				break;
			case WACS_FSM_REQ:
				PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
				break;
			default:
				break;
		}
	}while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}

static S32 pwrap_read_nochk( U32  adr, U32 *rdata )
{
	return _pwrap_wacs2_nochk( 0, adr,  0, rdata );
}

static S32 pwrap_write_nochk( U32  adr, U32  wdata )
{
	return _pwrap_wacs2_nochk( 1, adr,wdata,0 );
}

static S32 _pwrap_wacs2_nochk( U32 write, U32 adr, U32 wdata, U32 *rdata )
{
	U32 reg_rdata=0x0;
	U32 wacs_write=0x0;
	U32 wacs_adr=0x0;
	U32 wacs_cmd=0x0;
	U32 return_value=0x0;
	//PWRAPFUC();
	// Check argument validation
	if( (write & ~(0x1))    != 0)  return E_PWR_INVALID_RW;
	if( (adr   & ~(0xffff)) != 0)  return E_PWR_INVALID_ADDR;
	if( (wdata & ~(0xffff)) != 0)  return E_PWR_INVALID_WDAT;

	// Check IDLE
	return_value=wait_for_state_ready_init(wait_for_fsm_idle,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("_pwrap_wacs2_nochk write command fail,return_value=%x\n", return_value);
		return return_value;
	}

	wacs_write  = write << 31;
	wacs_adr    = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD,wacs_cmd);

	if( write == 0 )
	{
		if (NULL == rdata)
			return E_PWR_INVALID_ARG;
		// wait for read data ready
		return_value=wait_for_state_ready_init(wait_for_fsm_vldclr,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,&reg_rdata);
		if(return_value!=0)
		{
			PWRAPERR("_pwrap_wacs2_nochk read fail,return_value=%x\n", return_value);
			return return_value;
		}
		*rdata = GET_WACS0_RDATA( reg_rdata );
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR , 1);
	}
	return 0;
}

static S32 _pwrap_reset_spislv( void )
{
	U32 ret=0;
	U32 return_value=0;
	//PWRAPFUC();
	// This driver does not using _pwrap_switch_mux
	// because the remaining requests are expected to fail anyway

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , DISABLE_ALL);
	WRAP_WR32(PMIC_WRAP_WRAP_EN , DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL , MANUAL_MODE);
	WRAP_WR32(PMIC_WRAP_MAN_EN ,ENABLE);
	WRAP_WR32(PMIC_WRAP_DIO_EN ,DISABLE);

	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_CSL  << 8));//0x2100
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8)); //0x2800//to reset counter
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_CSH  << 8));//0x2000
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));

	return_value=wait_for_state_ready_init(wait_for_sync,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("_pwrap_reset_spislv fail,return_value=%x\n", return_value);
		ret=E_PWR_TIMEOUT;
		goto timeout;
	}

	WRAP_WR32(PMIC_WRAP_MAN_EN ,DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL ,WRAPPER_MODE);

timeout:
	WRAP_WR32(PMIC_WRAP_MAN_EN ,DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL ,WRAPPER_MODE);
	return ret;
}

static S32 _pwrap_init_sistrobe( void )
{
	U32 arb_en_backup=0;
	U32 rdata=0;
	U32 i=0;
	S32 ind=0; 
	U32 tmp1=0;
	U32 tmp2=0;
	U32 result_faulty=0;
	U32 result[2]={0,0};
	S32 leading_one[2]={-1,-1};
	S32 tailing_one[2]={-1,-1};

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN ,WACS2); // only WACS2

	//---------------------------------------------------------------------
	// Scan all possible input strobe by READ_TEST
	//---------------------------------------------------------------------
	for( ind=0; ind<24; ind++)  // 24 sampling clock edge
	{
		WRAP_WR32(PMIC_WRAP_SI_CK_CON , (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY ,0x3 - (ind & 0x3));
#ifdef SLV_6350    
			_pwrap_wacs2_nochk(0, MT6350_DEW_READ_TEST, 0, &rdata);
			if( rdata == MT6350_DEFAULT_VALUE_READ_TEST ) {
				 PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6350] pass,index=%d rdata=%x\n", ind,rdata);
     			 result[0] |= (0x1 << ind);
    		}else {
				 PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6350] tuning,index=%d rdata=%x\n", ind,rdata);
			}
#endif  
	 }
#ifndef SLV_6350
	  result[0] = result[1];
#endif
#ifndef SLV_6332
	  result[1] = result[0];
#endif
	 //---------------------------------------------------------------------
  	// Locate the leading one and trailing one of PMIC 1/2
  	//---------------------------------------------------------------------
	for( ind=23 ; ind>=0 ; ind-- )
	{
	  if( (result[0] & (0x1 << ind)) && leading_one[0] == -1){
		  leading_one[0] = ind;
	  }
	  if(leading_one[0] > 0) { break;}
	}
	for( ind=23 ; ind>=0 ; ind-- )
	{
	  if( (result[1] & (0x1 << ind)) && leading_one[1] == -1){
		  leading_one[1] = ind;
	  }
	  if(leading_one[1] > 0) { break;}
	}  
	
	for( ind=0 ; ind<24 ; ind++ )
	{
	  if( (result[0] & (0x1 << ind)) && tailing_one[0] == -1){
		  tailing_one[0] = ind;
	  }
	  if(tailing_one[0] > 0) { break;}
	}
	for( ind=0 ; ind<24 ; ind++ )
	{
	  if( (result[1] & (0x1 << ind)) && tailing_one[1] == -1){
		  tailing_one[1] = ind;
	  }
	  if(tailing_one[1] > 0) { break;}
	}  

  	//---------------------------------------------------------------------
  	// Check the continuity of pass range
  	//---------------------------------------------------------------------
  	for( i=0; i<2; i++)
  	{
    	tmp1 = (0x1 << (leading_one[i]+1)) - 1;
    	tmp2 = (0x1 << tailing_one[i]) - 1;
    	if( (tmp1 - tmp2) != result[i] )
    	{
    		/*TERR = "[DrvPWRAP_InitSiStrobe] Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d"
    	         	 , i+1, result[i], leading_one[i], tailing_one[i]*/
    	    PWRAPERR("_pwrap_init_sistrobe Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d\n", i+1, result[i], leading_one[i], tailing_one[i]);
      		result_faulty = 0x1;
    	}
  	}
	
	//---------------------------------------------------------------------
	// Config SICK and SIDLY to the middle point of pass range
	//---------------------------------------------------------------------
	if( result_faulty == 0 )
    {
  		// choose the best point in the interaction of PMIC1's pass range and PMIC2's pass range
    	ind = ( (leading_one[0] + tailing_one[0])/2 + (leading_one[1] + tailing_one[1])/2 )/2;
        /*TINFO = "The best point in the interaction area is %d, ind"*/ 
		WRAP_WR32(PMIC_WRAP_SI_CK_CON , (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY , 0x3 - (ind & 0x3));		
		//---------------------------------------------------------------------
		// Restore
		//---------------------------------------------------------------------
		WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , arb_en_backup);
		return 0;
	}
	else
	{
		PWRAPERR("_pwrap_init_sistrobe Fail,result_faulty=%x\n", result_faulty);
		return result_faulty;
	}
}

#if 0
static S32 _pwrap_init_reg_clock( U32 regck_sel )
{
	//U32 wdata=0;
	//U32 rdata=0;
	PWRAPFUC();
	
	// Set Dummy cycle 6350 (assume 18MHz)
#ifdef SLV_6350
	  pwrap_write_nochk(MT6350_DEW_RDDMY_NO, 0x8);
#endif
#ifdef SLV_6332  
	  pwrap_write_nochk(MT6332_DEW_RDDMY_NO, 0x8);
#endif  
	WRAP_WR32(PMIC_WRAP_RDDMY ,0x8); /* MT6350 only */

	// Config SPI Waveform according to reg clk
	if( regck_sel == 1 ) { // 16.2MHz in 6350, 33MHz in BBChip
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE , 0x0);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ	 , 0x6);  // wait data written into register => 3T_PMIC: consists of CSLEXT_END(1T) + CSHEXT(6T)
		WRAP_WR32(PMIC_WRAP_CSLEXT_START , 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END   , 0x0);
	}else { //Safe mode
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE , 0xf);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ	 , 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START , 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END   , 0xf);
	}

	return 0;
}
#endif

static void pwrap_delay_us(unsigned int us)
{
	/* FIXME: change to gpt API */
	volatile unsigned int delay = 100 * 1000;
	while(delay--);
}

static int _pwrap_init_partial(void)
{
	S32 sub_return=0;
	S32 sub_return1=0;
  	U32 clk_sel = 0;
	unsigned int rdata= 0x0;
	unsigned int cg_mask = 0;
	unsigned int backup = 0;

	PWRAPFUC();
    
	cg_mask = ((1 << 20) | (1 << 27) | (1 << 28) | (1 << 29));
	backup = (~WRAP_RD32(CLK_SWCG_1)) & cg_mask;
	WRAP_WR32(CLK_SETCG_1, cg_mask);
	/* dummy read to add latency (to wait clock turning off) */
	rdata = WRAP_RD32(PMIC_WRAP_SWRST);

	/* Toggle module reset */
	WRAP_WR32(PMIC_WRAP_SWRST, ENABLE);

	rdata = WRAP_RD32(WDT_FAST_SWSYSRST);
	WRAP_WR32(WDT_FAST_SWSYSRST, (rdata | (0x1 << 11)) | (0x88 << 24));
	WRAP_WR32(WDT_FAST_SWSYSRST, (rdata & (~(0x1 << 11))) | (0x88 << 24));
	WRAP_WR32(PMIC_WRAP_SWRST, DISABLE);

	/* Turn on module clock */
	WRAP_WR32(CLK_CLRCG_1, backup | (1 << 20)); // ensure cg for AP is off;

	/* Turn on module clock dcm (in global_con) */
	// WHQA_00014186: set PMIC bclk DCM default off due to HW issue
	WRAP_WR32(CLK_SETCG_3, (1 << 2) | (1 << 1));
	// WRAP_WR32(CLK_SETCG_3, (1 << 2));

	//###############################
	// Set SPI_CK_freq = 26MHz 
	//###############################
#ifndef CONFIG_MTK_FPGA
	clk_sel = WRAP_RD32(CLK_SEL_0);
	WRAP_WR32(CLK_SEL_0, clk_sel | (0x3 << 24));
#endif

	//###############################
	//Enable DCM
	//###############################
	WRAP_WR32(PMIC_WRAP_DCM_EN, 1);//enable CRC DCM and PMIC_WRAP DCM
	WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, DISABLE); //no debounce

	//###############################
	//Reset SPISLV
	//###############################
	sub_return=_pwrap_reset_spislv();
	if( sub_return != 0 )
	{
		PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_RESET_SPI;
	}
	//###############################
	// Enable WACS2
	//###############################
	WRAP_WR32(PMIC_WRAP_WRAP_EN, ENABLE);//enable wrap
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2); //Only WACS2
	WRAP_WR32(PMIC_WRAP_WACS2_EN, ENABLE);


	//###############################
    // Set Dummy cycle to make the cycle is the same at both AP and PMIC sides
    //###############################
    // (default value of 6320 dummy cycle is already 0x8)
#ifdef SLV_6350
	WRAP_WR32(PMIC_WRAP_RDDMY , 0xF);
#endif

	//###############################
	// Input data calibration flow;
	//###############################
	sub_return = _pwrap_init_sistrobe();
	if( sub_return != 0 )
	{
		PWRAPERR("error,DrvPWRAP_InitSiStrobe fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_SIDLY;
	}
#if 0
	//###############################
	// SPI Waveform Configuration
	//###############################
	//0:safe mode, 1:18MHz
	sub_return = _pwrap_init_reg_clock(2);
	if( sub_return != 0)
	{
		PWRAPERR("error,_pwrap_init_reg_clock fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_REG_CLOCK;
	}
#endif
	return 0;
}

/* dio_en should be 0x0(disable both PMIC) or 0x3(enable both PMIC) */
static int _pwrap_switch_dio(int dio_en)
{
	int arb_en_backup;
	int rdata;
	int sub_return;

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2); /* only WACS2 */
#ifdef SLV_6350
	sub_return = pwrap_write(MT6350_DEW_DIO_EN, dio_en & 0x1);
	if(sub_return != 0) {
		/* TERR="Error: [DrvPWRAP_SwitchDio][Set MT6350_DEW_DIO_EN] DrvPWRAP_WACS2 fail, return=%x, exp=0x0", sub_return*/
		return E_PWR_SWITCH_DIO;
	}
#endif
#if 0
	ret = wait_for_state_ready_init(wait_for_idle_and_sync, 
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (ret != 0) {
		PWRAPERR("_pwrap_init_dio fail,ret=%x\n", ret);
		return ret;
	}
#else
	do {
		/* Wait WACS2_FSM==IDLE */
		rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
	} while ((GET_WACS2_FSM(rdata) != 0x0)
		 || (GET_SYNC_IDLE2(rdata) != 0x1));
#endif
	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);

	/* Read Test */
#ifdef SLV_6350
	sub_return = pwrap_read(MT6350_DEW_READ_TEST,&rdata);
	if((sub_return != 0) || (rdata != MT6350_DEFAULT_VALUE_READ_TEST)) {
		/* TERR="Error: [DrvPWRAP_SwitchDio][Read Test] DrvPWRAP_WACS2() fail, dio_en = %x, return=%x, exp=0, rdata=%x, exp=0x5aa5", dio_en, sub_return, rdata */
		return E_PWR_READ_TEST_FAIL;
	}
#endif

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);

	return 0;
}

static int _pwrap_manual_mode(unsigned int write, unsigned int op,
				unsigned int wdata, unsigned int *rdata)
{
	unsigned int ret;
	unsigned int reg_rdata;
	unsigned int man_write;
	unsigned int man_op;
	unsigned int man_cmd;

	reg_rdata = WRAP_RD32(PMIC_WRAP_MAN_RDATA);
	if (GET_MAN_FSM(reg_rdata) != 0) /* IDLE State */
		return E_PWR_NOT_IDLE_STATE;

/* TODO */
	/* check argument validation */
	if ((write & ~(0x1)) != 0) return E_PWR_INVALID_RW;
	if ((op    & ~(0x1f)) != 0) return E_PWR_INVALID_OP_MANUAL;
	if ((wdata & ~(0xff)) != 0) return E_PWR_INVALID_WDAT;

	man_write = write << 13;
	man_op = op << 8;
	man_cmd = man_write | man_op | wdata;
	WRAP_WR32(PMIC_WRAP_MAN_CMD, man_cmd);

	if (write == 0) {
#if 0
		ret = wait_for_state_ready_init(wait_for_man_vldclr, 
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_MAN_RDATA, &reg_rdata);
		if(ret != 0)
			return ret;
#else
		do {
			reg_rdata = WRAP_RD32(PMIC_WRAP_MAN_RDATA);
		} while (GET_MAN_FSM(reg_rdata) != MAN_FSM_WFVLDCLR);
#endif
		*rdata = GET_MAN_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_MAN_VLDCLR, 1);
	}

	return 0;
}

static int _pwrap_switch_mux(unsigned int mux_sel_new)
{
	unsigned int ret;
	unsigned int mux_sel_old;
	unsigned int rdata;

	/* return if no change is necessary */
	mux_sel_old = WRAP_RD32(PMIC_WRAP_MUX_SEL);
	if (mux_sel_new == mux_sel_old)
		return 0;

	/* disable OLD, wait OLD finish */
	/* switch MUX, then enable NEW */
	if (mux_sel_new == 1) {
		WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);
#if 0
		ret = wait_for_state_ready_init(wait_for_wrap_idle,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WRAP_STA, 0);
		if(ret != 0)
			return ret;
#else
		do {
			/* Wait for WRAP to be in idle state,
			 * and no remaining rdata to be received
			 */
			rdata = WRAP_RD32(PMIC_WRAP_WRAP_STA);
		} while ((GET_WRAP_FSM(rdata) != 0x0)
			 || (GET_WRAP_CH_DLE_RESTCNT(rdata) != 0x0));
#endif
		WRAP_WR32(PMIC_WRAP_MUX_SEL, 1);
		WRAP_WR32(PMIC_WRAP_MAN_EN, 1);
	} else {
		WRAP_WR32(PMIC_WRAP_MAN_EN, 0);
#if 0
		ret = wait_for_state_ready_init(wait_for_man_idle_and_noreq,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_MAN_RDATA, 0);
		if(ret != 0)
			return return_value;
#else
		do {
			/* Wait for MAN to be in idle state,
			 * and no remaining rdata to be received
			 */
			rdata = WRAP_RD32(PMIC_WRAP_MAN_RDATA);
		} while ((GET_MAN_REQ(rdata) != 0x0)
			 || (GET_MAN_FSM(rdata) != 0x0));
#endif
		WRAP_WR32(PMIC_WRAP_MUX_SEL, 0);
		WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);
	}

	return 0;
}

static int _pwrap_manual_modeAccess(unsigned int write, unsigned int adr,
		unsigned int wdata, unsigned int *rdata)
{
	unsigned int man_wdata;
	int man_rdata;

	/* check argument validation */
	if ((write & ~(0x1))    != 0)      return E_PWR_INVALID_RW;
	if ((adr   & ~(0xffff)) != 0)      return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)      return E_PWR_INVALID_WDAT;
	/* not support manual access to PMIC 2 */
	if ((adr   &   0x8000)  == 0x8000) return E_PWR_INVALID_ADDR;

	_pwrap_switch_mux(1);
	_pwrap_manual_mode(OP_WR, OP_CSH, 0, &man_rdata);
	_pwrap_manual_mode(OP_WR, OP_CSL, 0, &man_rdata);
	man_wdata = (adr >> 9) | (write << 7);
	_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);
	man_wdata = adr >> 1;
	_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);

	if (write == 1) {
		man_wdata = wdata >> 8;
		_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);
		man_wdata = wdata;
		_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);
	} else {
		_pwrap_manual_mode(OP_WR, OP_CK, 8, &man_rdata);
		_pwrap_manual_mode(OP_RD, OP_IND, 0, &man_rdata);
		*rdata = GET_MAN_RDATA(man_rdata) << 8;
		_pwrap_manual_mode(OP_RD, OP_IND, 0, &man_rdata);
		*rdata |= GET_MAN_RDATA(man_rdata);
	}
	_pwrap_manual_mode(OP_WR, OP_CSL, 0, &man_rdata);
	_pwrap_manual_mode(OP_WR, OP_CSH, 0, &man_rdata);

	return 0;
}

static void _pwrap_AlignCRC(void)
{
	unsigned int ret;
	int reg_rdata;
	int arb_en_backup;
	int staupd_prd_backup;

	/* Backup Configuration & Set New Ones */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);
	staupd_prd_backup = WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0); /* disable STAUPD */

	/* reset CRC */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_CRC_SWRST, 1);
#endif

	WRAP_WR32(PMIC_WRAP_CRC_EN, 0);
#if 0
	ret = wait_for_state_ready_init(wait_for_wrap_state_idle,
		TIMEOUT_WAIT_IDLE, PMIC_WRAP_WRAP_STA, 0);
	if (ret != 0)
		PWRAPERR("%s fail, ret=0x%X", __FUNCTION__, ret);
#else
	do {
		reg_rdata = WRAP_RD32(PMIC_WRAP_WRAP_STA);
	} while (GET_WRAP_AG_DLE_RESTCNT(reg_rdata) != 0);
#endif

	/* Enable CRC */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_CRC_SWRST, 0);
#endif

	/* Dummy Read start */
	do {
		reg_rdata = WRAP_RD32(PMIC_WRAP_WRAP_STA);
	} while(GET_WRAP_CH_REQ(reg_rdata) != 0);
	/* Dummy Read end */

	WRAP_WR32(PMIC_WRAP_CRC_EN, 1);

	/* restore Configuration */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, staupd_prd_backup);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
}

static int _pwrap_enable_cipher(void)
{
	int arb_en_backup;
	int rdata;
	unsigned int ret;
	unsigned long long start_time_ns = 0, timeout_ns = 0;

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2); /* only WACS2 */

	/* Make sure CIPHER engine is idle */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_CIPHER_EN, 0x0);
#endif

	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL, 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 1);

	/* Config CIPHER @ PMIC */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_CIPHER_SWRST, 0x1);
	pwrap_write(MT6350_DEW_CIPHER_SWRST, 0x0);
	pwrap_write(MT6350_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write(MT6350_DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write(MT6350_DEW_CIPHER_EN, 0x1);
#endif

	/* wait for cipher data ready@AP */
#if 0
	ret = wait_for_state_ready_init(wait_for_cipher_ready, 
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_CIPHER_RDY, 0);
	if(ret != 0) {
		PWRAPERR("wait for cipher data ready@AP fail,ret=0x%x\n", ret);
		return ret;
	}
#else
	while(WRAP_RD32(PMIC_WRAP_CIPHER_RDY) != 1);
#endif

	/* wait for cipher data ready@PMIC */
#ifdef SLV_6350
/* TODO: use timeout mechanism
	start_time_ns = _pwrap_get_current_time();
*/
	do {
		pwrap_read(MT6350_DEW_CIPHER_RDY, &rdata);
	} while (rdata != 0x1); /* cipher0 ready */
	pwrap_write(MT6350_DEW_CIPHER_MODE, 0x1);
#endif

	/* Wait WACS2 and sync IDLE */
#if 0
	ret = wait_for_state_ready_init(wait_for_idle_and_sync, 
		TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (ret != 0) {
		PWRAPERR("wait for cipher mode idle fail,ret=0x%x\n", ret);
		return return_value;
	}
#else
	do {
		rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
	} while ((GET_WACS2_FSM(rdata) != 0x0)
		 || (GET_SYNC_IDLE2(rdata) != 0x1));
#endif
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 1);

	/* Read Test */
#ifdef SLV_6350
	pwrap_read(MT6350_DEW_READ_TEST, &rdata);
	if(rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("%s,read test1 error,rdata=0x%x,exp=0x%x\n",
			 __FUNCTION__, rdata, MT6350_DEFAULT_VALUE_READ_TEST);
		return E_PWR_READ_TEST_FAIL;
	}
#endif

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);

	return 0;
}

static int _pwrap_disable_cipher(void)
{
	int arb_en_backup;
	int rdata;
	unsigned int ret;
	unsigned long long start_time_ns = 0, timeout_ns = 0;

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2); /* only WACS2 */

	/* Disable CIPHER MODE */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_CIPHER_MODE, 0x0);
#endif

	/* Wait WACS2 and sync IDLE */
#if 0
	ret = wait_for_state_ready_init(wait_for_idle_and_sync, 
		TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (ret != 0) {
		PWRAPERR("wait for cipher mode idle fail,ret=0x%x\n", ret);
		return return_value;
	}
#else
	do {
		rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
	} while ((GET_WACS2_FSM(rdata) != 0x0)
		 || (GET_SYNC_IDLE2(rdata) != 0x1));
#endif
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);

	/* Read Test */
#ifdef SLV_6350
	pwrap_read(MT6350_DEW_READ_TEST, &rdata);
	if(rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("%s,read test1 error,rdata=0x%x,exp=0x%x\n",
			 __FUNCTION__, rdata, MT6350_DEFAULT_VALUE_READ_TEST);
		return E_PWR_READ_TEST_FAIL;
	}
#endif

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}


static void pwrap_lisr_normal_test(void);
static void pwrap_lisr_for_wdt_test(void);
static void pwrap_lisr_for_int_test(void);

/*
 * irq init start
 * choose_lisr=0: normal test
 * choose_lisr=1: watch dog test
 * choose_lisr=2: interrupt test
 */
#define NORMAL_TEST	0
#define WDT_TEST	1
#define INT_TEST	2

static unsigned int choose_lisr = NORMAL_TEST;

static unsigned int wrapper_lisr_count_cpu0=0;
static unsigned int wrapper_lisr_count_cpu1=0;

/* global variables for int test */
static unsigned int int_test_bit = 0;
static unsigned int wait_int_flag = 0;
static unsigned int int_test_fail_count = 0;

/* global variables for wdt test */
static unsigned int wdt_test_bit = 0;
static unsigned int wait_for_wdt_flag = 0;
static unsigned int wdt_test_fail_count = 0;

static irqreturn_t pwrap_ldvt_interrupt(int irq, void *dev)
{
	switch(choose_lisr)
	{
	case NORMAL_TEST:
		pwrap_lisr_normal_test();
		break;
	case WDT_TEST:
		pwrap_lisr_for_wdt_test();
		break;
	case INT_TEST:
		pwrap_lisr_for_int_test();
		break;
	}

	PWRAPLOG("before complete\n");
	complete(&pwrap_done);
	PWRAPLOG("after complete\n");

	return IRQ_HANDLED;
}

static void pwrap_lisr_normal_test(void)
{
	unsigned int reg_int_flg = 0;
	unsigned int reg_wdt_flg = 0;
	PWRAPFUC();

//#ifndef ldvt_follow_up
	if (raw_smp_processor_id() == 0)
		wrapper_lisr_count_cpu0++;
	else if (raw_smp_processor_id() == 1)
		wrapper_lisr_count_cpu1++;
//#endif

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x\n", reg_wdt_flg);

	/* clear watchdog irq status */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_INT_EN, 0X7FFFFFF9);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
}

static void pwrap_lisr_for_wdt_test(void)
{
	unsigned int reg_int_flg = 0;
	unsigned int reg_wdt_flg = 0;
	PWRAPFUC();

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x\n", reg_wdt_flg);

	PWRAPLOG("wdt_test_bit=%d\n", wdt_test_bit);
	if ((reg_int_flg & 0x1) != 0) {
		if((reg_wdt_flg & (1 << wdt_test_bit)) != 0) {
			PWRAPLOG("watch dog test:recieve the right wdt\n");
			wait_for_wdt_flag = 1;
			/* clear watchdog irq status */
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
			/* WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0xffffff); */
		} else {
			PWRAPLOG("watchdog test fail:recieve the wrong wdt\n");
			wdt_test_fail_count++;
			/* clear the unexpected watchdog irq status */
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
		}
	}

	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x\n", reg_wdt_flg);
}

static void pwrap_lisr_for_int_test(void)
{
	unsigned int reg_int_flg = 0;
	unsigned int reg_wdt_flg = 0;
	PWRAPFUC();

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x\n", reg_wdt_flg);

	PWRAPLOG("int_test_bit=%d\n", int_test_bit);
	if ((reg_int_flg & (1 << int_test_bit)) != 0) {
		PWRAPLOG(" int test:recieve the right pwrap interrupt\n");
		wait_int_flag = 1;
	} else {
		PWRAPLOG(" int test fail:recieve the wrong pwrap interrupt\n");
		int_test_fail_count++;
	}

	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x\n", reg_wdt_flg);

	/* for int test bit[1] */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0);
}

#if 1
static int pwrap_wacs0(unsigned int write, unsigned int adr, 
			unsigned int wdata, unsigned int *rdata)
{
	/* unsigned long long wrap_access_time = 0x0; */
	unsigned int reg_rdata = 0;
	unsigned int wacs_write = 0;
	unsigned int wacs_adr = 0;
	unsigned int wacs_cmd=0;
	unsigned int ret = 0;
	unsigned long flags = 0;

#if 0
	PWRAPLOG("wrapper access,write=%x,add=%x,wdata=%x,rdata=%x\n",write,adr,wdata,rdata);
#endif
	/* Check argument validation */
	if( (write & ~(0x1))    != 0)  return E_PWR_INVALID_RW;
	if( (adr   & ~(0xffff)) != 0)  return E_PWR_INVALID_ADDR;
	if( (wdata & ~(0xffff)) != 0)  return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&wacs0_lock, flags);
	/* Check IDLE & INIT_DONE in advance */
	ret = wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE,
			 PMIC_WRAP_WACS0_RDATA, PMIC_WRAP_WACS0_VLDCLR, 0);
	if (ret != 0)
	{
		PWRAPERR("wait_for_fsm_idle fail, ret=%d\n", ret);
		goto FAIL;
	}
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS0_CMD, wacs_cmd);
	if (write == 0)
	{
		if (NULL == rdata)
		{
			PWRAPERR("rdata is a NULL pointer\n");
			ret = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		ret = wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ,
				PMIC_WRAP_WACS0_RDATA, &reg_rdata);
		if (ret != 0)
		{
			PWRAPERR("wait_for_fsm_vldclr fail,ret=%d\n",ret);
			ret += 1;
			/* E_PWR_NOT_INIT_DONE_READ or 
			 * E_PWR_WAIT_IDLE_TIMEOUT_READ
			 */
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR ,1);
	}

FAIL:
	spin_unlock_irqrestore(&wacs0_lock, flags);
	if (ret != 0)
	{
		PWRAPERR("pwrap_wacs0 fail, ret=%d\n", ret);
		/* BUG_ON(1); */
	}
	#if 0
	wrap_access_time=sched_clock();
	pwrap_trace(wrap_access_time,ret,write, adr, wdata,(unsigned int)rdata);
	#endif
	return ret;
}

static int pwrap_wacs1(unsigned int write, unsigned int adr, 
			unsigned int wdata, unsigned int *rdata)
{
	/* unsigned long long wrap_access_time = 0x0; */
	unsigned int reg_rdata = 0;
	unsigned int wacs_write = 0;
	unsigned int wacs_adr = 0;
	unsigned int wacs_cmd=0;
	unsigned int ret = 0;
	unsigned long flags = 0;

#if 0
	PWRAPLOG("wrapper access,write=%x,add=%x,wdata=%x,rdata=%x\n",write,adr,wdata,rdata);
#endif
	/* Check argument validation */
	if( (write & ~(0x1))    != 0)  return E_PWR_INVALID_RW;
	if( (adr   & ~(0xffff)) != 0)  return E_PWR_INVALID_ADDR;
	if( (wdata & ~(0xffff)) != 0)  return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&wacs1_lock, flags);
	/* Check IDLE & INIT_DONE in advance */
	ret = wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE,
			 PMIC_WRAP_WACS1_RDATA, PMIC_WRAP_WACS1_VLDCLR, 0);
	if (ret != 0)
	{
		PWRAPERR("wait_for_fsm_idle fail, ret=%d\n", ret);
		goto FAIL;
	}
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS1_CMD, wacs_cmd);
	if (write == 0)
	{
		if (NULL == rdata)
		{
			PWRAPERR("rdata is a NULL pointer\n");
			ret = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		ret = wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ,
				PMIC_WRAP_WACS1_RDATA, &reg_rdata);
		if (ret != 0)
		{
			PWRAPERR("wait_for_fsm_vldclr fail,ret=%d\n",ret);
			ret += 1;
			/* E_PWR_NOT_INIT_DONE_READ or 
			 * E_PWR_WAIT_IDLE_TIMEOUT_READ
			 */
			goto FAIL;
		}
		*rdata = GET_WACS1_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR ,1);
	}

FAIL:
	spin_unlock_irqrestore(&wacs1_lock, flags);
	if (ret != 0)
	{
		PWRAPERR("pwrap_wacs1 fail, ret=%d\n", ret);
		/* BUG_ON(1); */
	}
	#if 0
	wrap_access_time=sched_clock();
	pwrap_trace(wrap_access_time,ret,write, adr, wdata,(unsigned int)rdata);
	#endif
	return ret;
}

#endif

static int _pwrap_wrap_access_test(void)
{
	unsigned int ret;
	int err = 0;
	unsigned int rdata = 0;
	unsigned int reg_value_backup = 0;
	PWRAPFUC();

	reg_value_backup = WRAP_RD32(PMIC_WRAP_INT_EN);
	/* clear sig_error interrupt test */
	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup & ~(0x2));

	/* Read/Write test using WACS0 */
	PWRAPLOG("start test WACS0\n");
#ifdef SLV_6350
	ret = pwrap_wacs0(0, MT6350_DEW_READ_TEST, 0, &rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read_test1 error, ret=0x%x, rdata=0x%x\n",
			ret, rdata);
		err += 1;
	}

	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
	ret = pwrap_wacs0(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write_test0 error, ret=0x%x, rdata0x=%x\n",
			ret, rdata);
		err += 1;
	}
#endif
	/* Read/Write test using WACS1 */
	PWRAPLOG("start test WACS1\n");
#ifdef SLV_6350
	ret = pwrap_wacs1(0, MT6350_DEW_READ_TEST, 0, &rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read_test1 error, ret=0x%x, rdata=0x%x\n",
			ret, rdata);
		err += 1;
	}

	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x5678, &rdata);
	ret = pwrap_wacs1(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x5678) {
		PWRAPERR("write_test1 error, ret=0x%x, rdata0x=%x\n",
			ret, rdata);
		err += 1;
	}
#endif
	/* Read/Write test using WACS2 */
	PWRAPLOG("start test WACS2\n");
#ifdef SLV_6350
	ret = pwrap_read(MT6350_DEW_READ_TEST,&rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read_test1 error, ret=0x%x, rdata=0x%x\n",
			ret, rdata);
		err += 1;
	}

	pwrap_write(WRAP_ACCESS_TEST_REG, 0xABCD);
	ret = pwrap_read(WRAP_ACCESS_TEST_REG, &rdata);
	if (rdata != 0xABCD) {
		PWRAPERR("write_test1 error, ret=0x%x, rdata0x=%x\n",
			ret, rdata);
		err += 1;
	}
#endif

	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup);
	return err;
}

static int _pwrap_status_update_test(void)
{
	unsigned int rdata;
	PWRAPFUC();

	/* disable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0X7FFFFFF9);

	/* change to SIG_VALUE mode */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_WRITE_TEST, MT6350_WRITE_TEST_VALUE);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | MT6350_DEW_WRITE_TEST);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_VALUE) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, rdata | 0xAA55);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_MODE) & 0x2;
	WRAP_WR32(PMIC_WRAP_SIG_MODE, rdata | 0x1);
#endif

	/* delay for 5 seconds? */
	pwrap_delay_us(5000);
#ifdef SLV_6350
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL) & 0xFFFF;
	if (rdata != MT6350_WRITE_TEST_VALUE) {
		PWRAPERR("status update test1 error, rdata=%x\n", rdata);
		return -1;
	}
#endif
#ifdef SLV_6332
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL) >> 16;
	if (rdata != MT6332_WRITE_TEST_VALUE) {
		PWRAPERR("status update test2 error, rdata=%x\n", rdata);
		return -1;
	}
#endif
#if 0
	/* the same as write test */
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, WRITE_TEST_VALUE);
#endif
	/* clear SIGERR interrupt flag bit */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 1 << 1);

	/* enable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7fffffff);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);
#ifdef SLV_6331
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | MT6331_DEW_CRC_VAL);
#endif
#ifdef SLV_6350
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | MT6350_DEW_CRC_VAL);
#endif
#ifdef SLV_6332
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & 0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | (MT6332_DEW_CRC_VAL << 16));
#endif

	return 0;
}

static int _pwrap_status_update_scheme_test(void)
{
	unsigned int rdata;
	PWRAPFUC();

/*
[31]: DEBUG_INT: Selected DEBUG source is asserted.
[30:24]: RESERVED
[23]: STAUPD_DLE_CNT_OVF: STAUPD DLE counter is overflowed.
[22]: STAUPD_ALE_CNT_OVF: STAUPD ALE counter is overflowed.
[21]: Modem request error occurs
[20]: WACS2_CMD_MISS: A WACS2 CMD is written while WACS2 is disabled.
[19]: WACS2_UNEXP_DLE: WACS2 unexpected DLE
[18]: WACS2_UNEXP_VLDCLR: WACS2 unexpected VLDCLR
[17]: WACS1_CMD_MISS: A WACS1 CMD is written while WACS1 is disabled.
[16]: WACS1_UNEXP_DLE: WACS1 unexpected DLE
[15]: WACS1_UNEXP_VLDCLR: WACS1 unexpected VLDCLR
[14]: WACS0_CMD_MISS: A WACS0 CMD is written while WACS0 is disabled.
[13]: WACS0_UNEXP_DLE: WACS0 unexpected DLE
[12]: WACS0_UNEXP_VLDCLR: WACS0 unexpected VLDCLR
[11]: HARB_DLE_OVF: High priority aribiter DLE is overflowed.
[10]: HARB_DLE_UNF: High priority aribiter DLE is underflowed.
[9]:  WRAP_AGDLE_OVF: WRAP agent DLE counter is overflowed.
[8]:  WRAP_AGDLE_UNF: WRAP agent DLE counter is underflowed.
[7]:  WRAP_CHDLE_OVF: WRAP channel DLE counter is overflowed.
[6]:  WRAP_CHDLE_UNF: WRAP channel DLE counter is underflowed.
[5]:  MAN_CMD_MISS: A MAN CMD is written while MAN is disabled.
[4]:  MAN_UNEXP_DLE: MAN unexpected DLE
[3]:  MAN_UNEXP_VLDCLR: MAN unexpected VLDCLR
[2]:  RESERVED
[1]:  SIG_ERR: Signature Checking failed.
[0]:  WDT_INT: WatchDog Timeout"
*/

	/* disable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0X7FFFFFF9);

	/* change to SIG_VALUE mode */
#ifdef SLV_6350
	pwrap_write(MT6350_DEW_WRITE_TEST, MT6350_WRITE_TEST_VALUE);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | MT6350_DEW_WRITE_TEST);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_VALUE) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, rdata | 0xAA55);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_MODE) & 0x1;
	WRAP_WR32(PMIC_WRAP_SIG_MODE, rdata | 0x1);
    rdata = WRAP_RD32(PMIC_WRAP_STAUPD_PRD) & 0xFFFF;
    PWRAPLOG("PMIC_WRAP_STAUPD_PRD = %x\n", rdata);    
#endif

	/* delay for 5 seconds? */
	pwrap_delay_us(5000);
#ifdef SLV_6350
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL) & 0xFFFF;
	if (rdata != MT6350_WRITE_TEST_VALUE) {
		PWRAPERR("status update test1 error, rdata=%x\n", rdata);
		return -1;
	}
    PWRAPLOG("Status update SIG_ERRVAL = %x, SIG_VALUE = %x\n", rdata, WRAP_RD32(PMIC_WRAP_SIG_VALUE));
#endif

#if 0
	/* the same as write test */
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, WRITE_TEST_VALUE);
#endif
    // Enable Signature from MT6350
    WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 1);

	/* Enable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7fffffff);

    rdata = WRAP_RD32(PMIC_WRAP_INT_FLG) & 0xFFFF;
    PWRAPLOG("PMIC_WRAP_INT_FLG before GRPEN Enable = %x\n", rdata);
    /* Disable Signature from MT6350 */
    WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0);
    
	/* enable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7fffffff);
    /* clear signature error flag */
  	WRAP_WR32(PMIC_WRAP_INT_CLR, 0x2);
    rdata = WRAP_RD32(PMIC_WRAP_INT_FLG) & 0xFFFF;
    PWRAPLOG("PMIC_WRAP_INT_FLG after GRPEN Disable = %x\n", rdata);
    
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);

#ifdef SLV_6350
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ADR) & ~0xFFFF;
	WRAP_WR32(PMIC_WRAP_SIG_ADR, rdata | MT6350_DEW_CRC_VAL);
#endif

	return 0;
}

static int tc_dual_io_test(void)
{
	unsigned int ret = 0;

	/* switch to dual IO mode */
	PWRAPLOG("enable dual IO mode\n");
	/* XXX: Don't we disable WRAP_EN first? */
	_pwrap_switch_dio(1);

	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail, ret=%d\n", ret);

	/* switch to single IO mode */
	PWRAPLOG("disable dual IO mode\n");
	_pwrap_switch_dio(0);

	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail, ret=%d\n", ret);

	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

/*  Must be tested in Dual IO mode, or the test will fail */
static int _pwrap_man_access_test(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	unsigned int return_value = 0;
	unsigned int reg_value_backup;
	int dio = 0;
	PWRAPFUC();

	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail, ret=%d\n", ret);

	dio = WRAP_RD32(PMIC_WRAP_DIO_EN);
	if (dio == 0)
		_pwrap_switch_dio(1);

	/* TODO: add PMIC2 */

	/* Read/Write test using manual mode */
#if 1
	/* Disable SIGERR and WDT, and shorten STAUPD period */
	reg_value_backup = WRAP_RD32(PMIC_WRAP_INT_EN);
	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup & ~(0x7));
#else
	reg_value_backup = WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, reg_value_backup & (~(0x1)));
#endif
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x1); /* 20us */

#ifdef SLV_6350
	ret = _pwrap_manual_modeAccess(0, MT6350_DEW_READ_TEST, 0, &rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		/* TERR="Error: [ReadTest] fail, rdata=%x, exp=0x5aa5", rdata */
		PWRAPERR("read test error, ret=%x, rdata=0x%x\n", ret, rdata);
		return_value += 1;
	}
	PWRAPLOG("read test data: 0x%X\n", rdata);

	ret = _pwrap_manual_modeAccess(0, MT6350_CID, 0, &rdata);
	PWRAPLOG("CID: 0x%X\n", rdata);    

	_pwrap_manual_modeAccess(1, WRAP_ACCESS_TEST_REG, 0x5432, &rdata);
	ret = _pwrap_manual_modeAccess(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if(rdata != 0x5432) {
		/* TERR="Error: [WriteTest] fail, rdata=%x, exp=0x1234", rdata*/
		PWRAPERR("write test error(using manual mode), return_value=%x, rdata=%x\n",
			return_value, rdata);
		ret += 1;
	}
	PWRAPLOG("write test data: 0x%x\n", rdata);

	_pwrap_switch_mux(0); /* switch to wrap mode */

	/* reset CRC */
	//_pwrap_AlignCRC();
#endif

#if 1
	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup);
#else
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, reg_value_backup);
#endif

	return ret;
}

static int tc_wrap_init_test(void)
{
	unsigned int ret;

	ret = pwrap_init();
	if (ret == 0)
		PWRAPLOG("pwrap init success\n");
	else
		PWRAPLOG("pwrap init fail\n");

	return ret;
}

static int tc_wrap_access_test(void)
{
	unsigned int ret;

	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("WRAP_UVVF_WACS_TEST pass\n");
	else
		PWRAPLOG("WRAP_UVVF_WACS_TEST fail, ret=%d\n", ret);

	return ret;
}

static int tc_status_update_test(void)
{
	unsigned int ret = 0;

	ret = _pwrap_status_update_test();
	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

static int tc_status_update_scheme_test(void)
{
	unsigned int ret = 0;

	ret = _pwrap_status_update_scheme_test();
	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

static int tc_reg_rw_test(void)
{
	unsigned int i, j;
	unsigned int pwrap_reg_size = 0;
	unsigned int DEW_reg_size = 0;
	unsigned int reg_write_size = 0;
	unsigned int test_result = 0;
	unsigned int ret = 0;
	unsigned int reg_data = 0;
	unsigned int reg_write_value[] = 
			{0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};

	PWRAPFUC();

	pwrap_reg_size = ARRAY_SIZE(mt6580_pwrap_reg_table);
/*
	DEW_reg_size = sizeof(DEW_reg_tbl_6350) / sizeof(DEW_reg_tbl_6350[0]);
	DEW_reg_size = ARRAY_SIZE(DEW_reg_tbl_6350);
*/
	reg_write_size = ARRAY_SIZE(reg_write_value);

	PWRAPLOG("pwrap_reg_size=%d\n", pwrap_reg_size);
	PWRAPLOG("DEW_reg_size=%d\n", DEW_reg_size);

	PWRAPLOG("start test mt6580_pwrap_reg_table:default value test\n");
	/* toggle SWRST */
	/* TODO: Need to do full reset? */
	WRAP_WR32(PMIC_WRAP_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_SWRST, 0);

	for (i = 0; i < pwrap_reg_size; i++) {
		/* Only RW or RO type should do default value test */
		if (REG_TYPE_WO != mt6580_pwrap_reg_table[i][3]) {
#if 1
			PWRAPLOG("Offset 0x%03x default value: 0x%x, i=%d\n",
				mt6580_pwrap_reg_table[i][0],
				mt6580_pwrap_reg_table[i][1], i);
#endif

			if ((WRAP_RD32(PMIC_WRAP_BASE
			     + mt6580_pwrap_reg_table[i][0])
			     != mt6580_pwrap_reg_table[i][1])) {
				PWRAPLOG("Offset 0x%x default value test fail, DEF:0x%x, read:0x%x\n",
					mt6580_pwrap_reg_table[i][0], 
					mt6580_pwrap_reg_table[i][1], 
					WRAP_RD32(PMIC_WRAP_BASE
					+ mt6580_pwrap_reg_table[i][0]));
				test_result++;
			}
		}
	}

	PWRAPLOG("start test mt6580_pwrap_reg_table:RW test\n");
	for (i = 0; i < pwrap_reg_size; i++) {
		if (REG_TYPE_RW == mt6580_pwrap_reg_table[i][3]) {
			for (j = 0; j < reg_write_size; j++) {
#if 0
				PWRAPLOG("Offset 0x%x value: 0x%x\n", 
					mt6752_pwrap_reg_table[i][0], 
					reg_write_value[j]);
#endif
				WRAP_WR32((PMIC_WRAP_BASE + mt6580_pwrap_reg_table[i][0]), 
					reg_write_value[j] & mt6580_pwrap_reg_table[i][2]);
				if ((WRAP_RD32(PMIC_WRAP_BASE + mt6580_pwrap_reg_table[i][0]))
					!= (reg_write_value[j] & mt6580_pwrap_reg_table[i][2])) {
					PWRAPLOG("Offset 0x%x RW test fail, write %x, read %x\n", 
					mt6580_pwrap_reg_table[i][0],
					reg_write_value[j] & mt6580_pwrap_reg_table[i][2],
					WRAP_RD32(PMIC_WRAP_BASE + mt6580_pwrap_reg_table[i][0]));
					test_result++;
				}
			}
		}
	}
#if 0
	PWRAPLOG("start test DEW_reg_tbl_6350:default value test\n");
	/* reset spislv register */
	ret = _pwrap_init_partial();

	for (i = 0; i < DEW_reg_size; i++)
	{
		//Only R/W or RO should do default value test
		if (REG_TYPE_WO != DEW_reg_tbl_6350[i][3])
		{
			_pwrap_wacs2_nochk(0, DEW_reg_tbl_6350[i][0], 0, &reg_data);

			if ((reg_data != DEW_reg_tbl_6350[i][1]))
			{
				PWRAPLOG("Register %x Default Value test fail. DEF %x, read %x \r\n",
					DEW_reg_tbl_6350[i][0],  DEW_reg_tbl_6350[i][1], reg_data);
				test_result++;
			}
		}
	}

	PWRAPLOG("start test DEW_reg_tbl_6350:R/W test\n");
	for (i = 0; i < DEW_reg_size; i++)
	{
		/* ignore registers which change R/W behavior */
		if ((DEW_reg_tbl_6350[i][0] == DEW_DIO_EN) ||
			(DEW_reg_tbl_6350[i][0] == DEW_CIPHER_MODE) ||
			(DEW_reg_tbl_6350[i][0] == DEW_RDDMY_NO))
			continue;

		if (REG_TYPE_RW == DEW_reg_tbl_6350[i][3])
		{
			for (j = 0; j < reg_write_size; j++)
			{
				reg_data = 0;

				_pwrap_wacs2_nochk(1, DEW_reg_tbl_6350[i][0],
					 reg_write_value[j] & DEW_reg_tbl_6350[i][2], 0);

				_pwrap_wacs2_nochk(0, DEW_reg_tbl_6350[i][0], 0, &reg_data);

				if (reg_data != (reg_write_value[j] & DEW_reg_tbl_6350[i][2]))
				{
					PWRAPLOG("Register %x R/W test fail. write %x, read %x \r\n",
						DEW_reg_tbl_6350[i][0],
						(reg_write_value[j] & DEW_reg_tbl_6350[i][2]), 
						reg_data);
					test_result++;
				}
			}
		}
	}
#endif
	if(test_result == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, test_result);

	return test_result;
}

static int tc_mux_switch_test(void)
{
	unsigned int ret = 0;

	ret = _pwrap_man_access_test();
	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

static int tc_reset_pattern_test(void)
{
	unsigned int ret = 0;
	unsigned int rdata = 0;

#if 1
	ret = pwrap_init();
	ret = _pwrap_wrap_access_test();
#endif

#ifdef SLV_6350
	pwrap_write(MT6350_DEW_WRITE_TEST, 0x1234);
	pwrap_read(MT6350_DEW_WRITE_TEST, &rdata);
#endif
	PWRAPLOG("DEW_WRITE_TEST before reset: 0x%X\n", rdata);

#ifdef SLV_6332
#endif

	/* reset spislv register */
#if 1
	ret = _pwrap_init_partial();
	if (ret != 0)
		PWRAPLOG("init_partial fail\n");
#else
	ret = _pwrap_reset_spislv();
	if (ret != 0)
		PWRAPLOG("reset_spislv\n");
#endif    

#ifdef SLV_6350
	rdata = _pwrap_wacs2_nochk(0, MT6350_DEW_WRITE_TEST, 0, &rdata);
	PWRAPLOG("DEW_WRITE_TEST after reset: 0x%X\n", rdata);
	if (rdata != 0)
		ret = -1;
#endif

	if(ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

static int tc_soft_reset_test(void)
{
	unsigned int ret = 0;
	unsigned int rdata = 0;

#if 0
	/* do wrap init and wrap access test */
	ret = pwrap_init();
	ret = _pwrap_wrap_access_test();
#endif
	/* reset wrap */
	rdata = WRAP_RD32(PMIC_WRAP_DIO_EN);
	PWRAPLOG("PMIC_WRAP_DIO_EN before reset: 0x%x\n", rdata);

	PWRAPLOG("start reset wrapper\n");

	/* toggle SWRST bit */
	WRAP_WR32(PMIC_WRAP_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_SWRST, 0);

	rdata = WRAP_RD32(PMIC_WRAP_DIO_EN);
	PWRAPLOG("PMIC_WRAP_DIO_EN after reset: 0x%x\n", rdata);

	PWRAPLOG("the wrap access test should be fail after reset,before init\n");
	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail, ret=%d\n", ret);

	PWRAPLOG("the wrap access test should be pass after reset and wrap init\n");

	/* do wrap init and wrap access test */
	ret = pwrap_init();
	if (ret != 0)
		goto end;

	ret = _pwrap_wrap_access_test();
	if (ret == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail, ret=%d\n", ret);

end:
	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}

static int tc_high_pri_test(void)
{
	unsigned int ret = 0;
	unsigned int rdata = 0;
	unsigned long long pre_time = 0;
	unsigned long long post_time = 0;
	unsigned long long enable_staupd_time = 0;
	unsigned long long disable_staupd_time = 0;

/* TODO: figure out what the following code is doing */

	/* enable status updata and do wacs0 */
	PWRAPLOG("enable status updata and do wacs0, record the cycle\n");
	/* 0x1:20us, for concurrence test; 0x5:100us, for MP */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x1);
#if 0
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xff);
#endif

	/* Read/Write test using WACS0 */
	/* perfmon_start(); */
	/* record time start, ldvt_follow_up */
	pre_time = sched_clock();
#ifdef SLV_6350
	pwrap_wacs0(0, MT6350_DEW_READ_TEST, 0, &rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS0), rdata=%x\n", rdata);
		ret += 1;
	}
#endif

	/* perfmon_end(); */
	post_time = sched_clock();
	enable_staupd_time = post_time - pre_time;
	PWRAPLOG("pre_time=%lld, post_time=%lld\n", pre_time, post_time);
	PWRAPLOG("pwrap_wacs0 enable_staupd_time=%lld\n", enable_staupd_time);

	/* disable status updata and do wacs0 */
	PWRAPLOG("disable status updata and do wacs0, record the cycle\n");
	/* 0x1:20us, for concurrence test; 0x5:100us, for MP */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,0x00);

	/* Read/Write test using WACS0 */
	/* perfmon_start(); */
	pre_time = sched_clock();
#ifdef SLV_6350
	pwrap_wacs0(0, MT6350_DEW_READ_TEST, 0, &rdata);
	if (rdata != MT6350_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS0), rdata=%x\n", rdata);
		ret += 1;
	}
#endif

	/* perfmon_end(); */
	post_time = sched_clock();
	disable_staupd_time = post_time - pre_time;
	PWRAPLOG("pre_time=%lld, post_time=%lld\n", pre_time, post_time);
	PWRAPLOG("pwrap_wacs0 disable_staupd_time=%lld\n", disable_staupd_time);
	if (disable_staupd_time <= enable_staupd_time)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret += 1;
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);
	}

	return ret;
}

/*
 *  tested in the following sequence
 *  1. SIO, PLAIN TEXT
 *  2. SIO, ENCRYPTION
 *  1. DIO, PLAIN TEXT
 *  2. DIO, ENCRYPTION
 */
static int tc_spi_encryption_test(void)
{
	unsigned int ret = 0;

	/* mask WDT interrupt */
//	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffffe);
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffff8);

	/* disable status update and check the waveform on oscilloscope */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0); /* 0x0:disable */

	/* change to SIO mode */
	/* XXX: Don't we disable WRAP_EN first? */
	_pwrap_switch_dio(0);

	/* disable Encryption */
	ret = _pwrap_disable_cipher(); /* set breakpoint here */
	if (ret != 0) {
		PWRAPERR("disable Encryption error, ret=%x", ret);
		goto end;
	}
	ret = _pwrap_wrap_access_test();

	/* enable Encryption */
	ret = _pwrap_enable_cipher();
	if (ret != 0) {
		PWRAPERR("Enable Encryption error, ret=%x", ret);
		goto end;
	}
	ret = _pwrap_wrap_access_test();

	/* change to DIO mode */
	_pwrap_switch_dio(1);

	/* disable Encryption */
	ret = _pwrap_disable_cipher(); /* set breakpoint here */
	if (ret != 0) {
		PWRAPERR("Disable Encryption error, ret=%x", ret);
		goto end;
	}
	ret = _pwrap_wrap_access_test();

	/* enable Encryption */
	ret = _pwrap_enable_cipher();
	if (ret != 0) {
		PWRAPERR("Enable Encryption error, ret=%x", ret);
		goto end;
	}
	ret = _pwrap_wrap_access_test();

end:
	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}


/* watch dog test start */
#ifdef SLV_6331
static unsigned int watch_dog_test_reg = MT6331_DEW_WRITE_TEST;
#endif
#ifdef SLV_6350
static unsigned int watch_dog_test_reg = MT6350_DEW_WRITE_TEST;
#endif
/* FIXME: How about PMIC2?? */

static int _wdt_test_disable_other_int(void)
{
	/* disable other interrupt bit */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x1); // [0]: WDT_INT: WatchDog Timeout

	return 0;
}

/*
 * [1]: HARB_WACS0_ALE: HARB to WACS0 ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN, and 
 * send a WACS0 write command
 */
static int _wdt_test_bit1(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 1;
#if 1
	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xff);
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);

	pwrap_wacs0(1, watch_dog_test_reg, 0x1234, 0);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}
#endif
	return ret;
}

/*
 * [2]: HARB_WACS1_ALE: HARB to WACS1 ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS1 write command
 */
static int _wdt_test_bit2(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 2;
#if 1
	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* FIXME: WACS1 uses bit3 in HIPRIO_ARB_EN??? */
	/* disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xff);
	WRAP_CLR_BIT(1 << 3, PMIC_WRAP_HIPRIO_ARB_EN);

	pwrap_wacs1(1, watch_dog_test_reg, 0x1234, 0);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}
#endif
	return ret;
}

/*
 * [3]: HARB_WACS2_ALE: HARB to WACS2 ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS2 write command
 */
static int _wdt_test_bit3(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 3;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* FIXME: WACS2 uses bit4 in HIPRIO_ARB_EN */
	/* disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xff);
#if 0
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);
#else
	WRAP_CLR_BIT(1 << 4, PMIC_WRAP_HIPRIO_ARB_EN);
#endif

	pwrap_write(watch_dog_test_reg, 0x1234);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [5]: HARB_WACS_P2P_ALE: HARB to WACS_P2P ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN, and send 
 * a WACS_P2P write command
 */
static int _wdt_test_bit5(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 5;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* FIXME: WACS_P2P uses bit7 in HIPRIO_ARB_EN */
	/* disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xff);
#if 0
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);
#else
	WRAP_CLR_BIT(1 << 7, PMIC_WRAP_HIPRIO_ARB_EN);
#endif

	pwrap_write(watch_dog_test_reg, 0x1234);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [6]: HARB_STAUPD_ALE: HARB to STAUPD ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN,and do status update test
 */
static int _wdt_test_bit6(void)
{
	//unsigned int rdata = 0;
	unsigned int ret = 0;
	unsigned int reg_rdata = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 6;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* disable other wdt bit */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=0x%x\n", reg_rdata);
	
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0); // disable auto status update

	//disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(1 << 5, PMIC_WRAP_HIPRIO_ARB_EN);

	//similar to status updata test case
	//pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
	//WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
	//WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);

	// manually trigger status update.
	WRAP_WR32(PMIC_WRAP_STAUPD_MAN_TRIG, 0x1);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	//WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x55AA);//the same as write test
	return ret;
}
#if 0
/*
 * [7]: PWRAP_PERI_ALE: HARB to PWRAP_PERI_BRIDGE ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS3 write command
 */
static int _wdt_test_bit7( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=7;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  //do wacs3
  //pwrap_wacs3(1, watch_dog_test_reg, 0x55AA, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  return 0;
}

/*
 * [8]: HARB_EINTBUF_ALE: HARB to EINTBUF ALE timeout monitor
 * disable the corresponding bit in HIPRIO_ARB_EN,and send a eint interrupt
 */
static int _wdt_test_bit8( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=8;
  wait_for_wdt_flag=0;
  #ifdef ENABLE_KEYPAD_ON_LDVT
    //kepadcommand
    *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
    kpd_init();
    initKpdTest();
  #endif
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  return 0;
}

/*
 * [9]: WRAP_HARB_ALE: WRAP to HARB ALE timeout monitor
 * disable RRARB_EN[0],and do eint test
 */
static int _wdt_test_bit9( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0,i=0;
  PWRAPFUC();
  wdt_test_bit=9;
  wait_for_wdt_flag=0;



#ifdef ENABLE_EINT_ON_LDVT
  //eint_init();
  //_concurrence_eint_test_code(eint_in_cpu0);
  //eint_unmask(eint_in_cpu0);
  //Delay(500);
#endif
  //disable wrap_en
  //WRITE_REG(0xFFFFFFFF, EINT_MASK_CLR);



  //Delay(1000);
   for (i=0;i<(300*20);i++);
   wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  //WRAP_WR32(PMIC_WRAP_WRAP_EN ,1);//recover
  PWRAPLOG("%s pass\n", __FUNCTION__);
  return 0;
}

/*
 * [10]: PWRAP_AG_ALE#1: PWRAP to AG#1 ALE timeout monitor
 * disable RRARB_EN[1],and do keypad test
 */
static int _wdt_test_bit10( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=10;
  wait_for_wdt_flag=0;
#ifdef ENABLE_KEYPAD_ON_LDVT
  //kepad command
  *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
  kpd_init();
  initKpdTest();
#endif
  //disable wrap_en
  //WRAP_CLR_BIT(1<<1 ,PMIC_WRAP_RRARB_EN);
  wait_for_completion(&pwrap_done);
  //push keypad key
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  return 0;
}

/*
 * [11]: PWRAP_AG_ALE#2: PWRAP to AG#2 ALE timeout monitor
 * disable RRARB_EN[0],and do eint test
 */
static int _wdt_test_bit11( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=11;
  wait_for_wdt_flag=0;
  //kepadcommand
#ifdef ENABLE_EINT_ON_LDVT
  //eint_init();
  //_concurrence_eint_test_code(eint_in_cpu0);
  //eint_unmask(eint_in_cpu0);
#endif

  //disable wrap_en
  //WRAP_CLR_BIT(1<<1 ,PMIC_WRAP_RRARB_EN);
  wait_for_completion(&pwrap_done);
  //push keypad key
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  return 0;
}
#endif
/*
 * [12]: WRAP_HARB_ALE: WRAP to HARB ALE timeout monitor
 *   Disable WRAP_EN and set a WACS0 read command
 */
static int _wdt_test_bit12(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 12;
#if 0
	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	/* switch to manual mode */
	//_pwrap_switch_mux(1);
	//WRAP_WR32(PMIC_WRAP_MUX_SEL , 0);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);//disble WRAP_EN
	//WRAP_WR32(PMIC_WRAP_MAN_EN , 1);//enable manual

	pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	//_pwrap_switch_mux(0);// recover
#endif
	return ret;
}

/*
 * [13]: MUX_WRAP_ALE: MUX to WRAP ALE timeout monitor
 *  set MUX to manual mode ,enable WRAP_EN and set a WACS0 write command
 */
static int _wdt_test_bit13(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 13;
#if 0
	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	/* switch to manual mode */
	_pwrap_switch_mux(1);
	//WRAP_WR32(PMIC_WRAP_MUX_SEL , 0);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);//enable wrap
	//WRAP_WR32(PMIC_WRAP_MAN_EN , 1);//enable manual

	pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(0);// recover
#endif
	return ret;
}

/*
 * [14]: MUX_MAN_ALE: MUX to MAN ALE timeout monitor
 * MUX to MAN ALE:set MUX to wrap mode and send manual command
 */
static int _wdt_test_bit14(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 14;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	_pwrap_switch_mux(0);/* switch to wrap mode */
	//WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);//enable wrap
	/* enable manual */
	WRAP_WR32(PMIC_WRAP_MAN_EN, 1);

	_pwrap_manual_mode(0,  OP_IND, 0, &rdata);
	//_pwrap_manual_modeAccess(1, WRAP_ACCESS_TEST_REG, 0x1414, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(1);//
	return ret;
}

/*
 * [16]: HARB_WACS0_DLE: HARB to WACS0 DLE timeout monitor
 * Disable MUX, and send a read command with WACS0
 */
static int _wdt_test_bit16(void)
{
	unsigned int rdata = 0;
	unsigned int reg_rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 16;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	/* disable other wdt bit */
	//WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
	//WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x\n", reg_rdata);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 1 << 1);//enable wrap
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x\n", reg_rdata);
	//set status update period to the max value,or disable status update
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

	_pwrap_switch_mux(1);//manual mode
	WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap

	//read command
	pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(0);//recover

	return ret;
}

/*
 * [17]: HARB_WACS1_DLE: HARB to WACS1 DLE timeout monitor
 * Disable MUX,and send a read command with WACS1
 */
static int _wdt_test_bit17(void)
{
	unsigned int rdata = 0;
	unsigned int reg_rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 17;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 1 << 3);//enable wrap
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x\n", reg_rdata);
	//set status update period to the max value,or disable status update
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);

	_pwrap_switch_mux(1);//manual mode
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);//enable wrap

	//read command
	pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(0);//recover

	return ret;
}

/*
 * [18]: HARB_WACS2_DLE: HARB to WACS2 DLE timeout monitor
 * Disable MUX,and send a read command with WACS2
 */
static int _wdt_test_bit18(void)
{
	unsigned int rdata = 0;
	unsigned int reg_rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 18;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	/* disable other wdt bit */
	//WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	//WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);

	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x\n", reg_rdata);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 1 << 4);
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x\n", reg_rdata);
	//set status update period to the max value,or disable status update
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

	reg_rdata = WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
	PWRAPLOG("PMIC_WRAP_STAUPD_PRD=%x\n", reg_rdata);

	_pwrap_switch_mux(1);//manual mode
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);//enable wrap
	//clear INT
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xFFFFFFFF);

	//read command
	pwrap_read(watch_dog_test_reg, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(0);//recover
	return ret;
}
#if 0
//[19]: HARB_ERC_DLE: HARB to ERC DLE timeout monitor
//HARB to staupda DLE:disable event,write de_wrap event test,then swith mux to manual mode ,enable wrap_en enable event
//similar to bit5
static int _wdt_test_bit19( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=19;
  wait_for_wdt_flag=0;
  //disable event
  //WRAP_WR32(PMIC_WRAP_EVENT_IN_EN , 0);

  //do event test
  //WRAP_WR32(PMIC_WRAP_EVENT_STACLR , 0xffff);
#if 0
  ret=pwrap_wacs0(1, DEW_EVENT_TEST, 0x1, &rdata);
#endif
  //disable mux
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //enable event
  //WRAP_WR32(PMIC_WRAP_EVENT_IN_EN , 1);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[20]: HARB_STAUPD_DLE: HARB to STAUPD DLE timeout monitor
//  HARB to staupda DLE:disable MUX,then send a read commnad ,and do status update test
//similar to bit6
static int _wdt_test_bit20( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=20;
  wait_for_wdt_flag=0;
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //similar to status updata test case
  pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
  WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  _pwrap_switch_mux(0);//recover
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0x55AA);//tha same as write test

  return 0;
}
/*
 * [21]: HARB_RRARB_DLE: HARB to RRARB DLE timeout monitor HARB to RRARB DLE
 * :disable MUX,do keypad test
 */
static int _wdt_test_bit21( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  unsigned int reg_backup;
  PWRAPFUC();
  wdt_test_bit=21;
  wait_for_wdt_flag=0;
#ifdef ENABLE_KEYPAD_ON_LDVT
  //kepad command
  *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
  kpd_init();
  initKpdTest();
#endif
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0 );
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
  reg_backup=WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 0x80);//only enable keypad
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  WRAP_WR32(0x10016020 , 0);//write keypad register,to send a keypad read request
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , reg_backup);

  _pwrap_switch_mux(0);//recover
  return 0;
}


/*
 * [22]: MUX_WRAP_DLE: MUX to WRAP DLE timeout monitor
 * MUX to WRAP DLE:disable MUX,then send a read commnad ,and do WACS0
 */
static int _wdt_test_bit22( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=22;
  wait_for_wdt_flag=0;
  //WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  _pwrap_switch_mux(0);//recover
  return 0;
}
#endif
/*
 * [23]: MUX_MAN_DLE: MUX to MAN DLE timeout monitor
 * Disable MUX,then send a read command in manual mode
 */
static int _wdt_test_bit23(void)
{
	unsigned int rdata = 0;
	unsigned int reg_rdata = 0;
	unsigned int ret = 0;
	unsigned int return_value = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 23;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

// TODO: figure out...

	/* disable other wdt bit */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x\n", reg_rdata);

	/* switch to manual mode */
	return_value = _pwrap_switch_mux(1);
	PWRAPLOG("_pwrap_switch_mux return_value=%x\n", return_value);

	WRAP_WR32(PMIC_WRAP_SI_CK_CON, 0x6);
	reg_rdata = WRAP_RD32(PMIC_WRAP_SI_CK_CON);
	PWRAPLOG("PMIC_WRAP_SI_CK_CON=%x\n", reg_rdata);

	return_value = _pwrap_manual_mode(0,  OP_IND, 0, &rdata);
	PWRAPLOG("_pwrap_manual_mode return_value=%x\n", return_value);

	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	_pwrap_switch_mux(0);//recover
	return ret;
}
#if 0
/*
 * [24]: MSTCTL_SYNC_DLE: MSTCTL to SYNC DLE timeout monitor
 * MSTCTL to SYNC  DLE:disable sync,then send a read commnad with wacs0
 */
static int _wdt_test_bit24( )
{
  unsigned int rdata=0;
  unsigned int reg_rdata=0;
  unsigned int ret=0;
  PWRAPFUC();
  wdt_test_bit=24;
  wait_for_wdt_flag=0;
  _pwrap_switch_mux(1);//manual mode
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("%s pass\n", __FUNCTION__);
  _pwrap_switch_mux(0);//recover
  return 0;
}
#endif
/*
 * [25]: STAUPD_TRIG: STAUPD trigger signal timeout monitor
 * After init, set period = 0
 */
static int _wdt_test_bit25(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 25;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	//0x1:20us,for concurrence test,MP:0x5;  //100us
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [26]: PREADY: APB PREADY timeout monitor
 * disable WRAP_EN and write wacs0 6 times
 */
static int _wdt_test_bit26(void)
{
	//unsigned int rdata = 0;
	unsigned int wdata = 0;
	unsigned int ret = 0;
	unsigned int i = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 26;

	ret = pwrap_init();
	if (ret != 0) {
		wdt_test_fail_count++;
		return wdt_test_bit;
	}

	_wdt_test_disable_other_int();
	init_completion(&pwrap_done);

	/* disable other wdt bit */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);

	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);//disable wrap
	for (i = 0; i < 10; i++) {
		wdata += 0x20;
		//pwrap_wacs0(1, watch_dog_test_reg, wdata, &rdata);
		WRAP_WR32(PMIC_WRAP_WACS0_CMD, 0x80000000);
		PWRAPLOG("send %d command \n",i);
	}
	wait_for_completion(&pwrap_done);

	if (wait_for_wdt_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = wdt_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

static int tc_wdt_test(void)
{
	unsigned int ret = 0;
	unsigned int return_value = 0;
	unsigned int reg_data = 0;

	wdt_test_fail_count = 0;
	/* switch IRQ handler */
	choose_lisr = WDT_TEST;

	/* enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffff);

	ret = _wdt_test_bit1();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit2();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit3();
	if (ret != 0)
		return_value |= (1 << ret);
#if 0
	ret=_wdt_test_bit5();
#endif

#if 1 /* use codevisor to toggle */
	ret = _wdt_test_bit6();//fail
	if (ret != 0)
		return_value |= (1 << ret);
#endif
#if 0
	ret = _wdt_test_bit7();

	ret = _wdt_test_bit8();

	ret = _wdt_test_bit9();//eint

	ret = _wdt_test_bit10();//eint

	ret = _wdt_test_bit11();//eint

	reg_data=WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x\n",reg_data);
#endif

#if 1
	ret = _wdt_test_bit12(); /* need to add timeout */
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit13();
	if (ret != 0)
		return_value |= (1 << ret);

	reg_data = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("wrap_int_flg=%x\n", reg_data);
	reg_data = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=%x\n", reg_data);

	ret = _wdt_test_bit14();
	if (ret != 0)
		return_value |= (1 << ret);

	//ret=_wdt_test_bit15();
#endif

	ret = _wdt_test_bit16();//pass
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit17();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit18();
	if (ret != 0)
		return_value |= (1 << ret);
#if 0
	ret = _wdt_test_bit19();

	ret = _wdt_test_bit20();

	ret = _wdt_test_bit21();
#endif
	ret = _wdt_test_bit23(); //pass
	if (ret != 0)
		return_value |= (1 << ret);

	//ret = _wdt_test_bit24();

	ret = _wdt_test_bit25();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _wdt_test_bit26();//cann't test
	if (ret != 0)
		return_value |= (1 << ret);

	/* switch back IRQ handler */
	choose_lisr = NORMAL_TEST;

	if (wdt_test_fail_count == 0) {
		PWRAPLOG("%s pass, ret=0x%x\n", __FUNCTION__, return_value);
		return 0;
	} else {
		PWRAPLOG("%s fail, fail_count=%d, ret=0x%x\n",
			__FUNCTION__, wdt_test_fail_count, return_value);
		return return_value;
	}
}
/* watch dog test end */

/* interrupt test start */
#ifdef SLV_6331
unsigned int interrupt_test_reg = MT6331_DEW_WRITE_TEST;
#endif
#ifdef SLV_6350
unsigned int interrupt_test_reg = MT6350_DEW_WRITE_TEST;
#endif

static int _int_test_disable_watch_dog(void)
{
	/* disable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);

	return 0;
}

/* [1]:  SIG_ERR: Signature Checking failed. */
static int _int_test_bit1(void)
{
	//unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 1;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

#if 1 
	/* CRC mode */
	/* If we write odd address into address for STAUPD, it will 
	 * causes SIGERR. Maybe it's due to the way PMIC_WRAP calculates 
	 * CRC value. (PMIC side calculates CRC value using 15-bit value
	 * of PMIC address shifted right by 1, while AP side calculates 
	 * CRC value using 16-bit value of PMIC address. This causes
	 * difference in bit[0] of the PMIC address.
	 */
	WRAP_WR32(PMIC_WRAP_ADC_RDY_ADDR, 0x1);
#else
	/* sig_value mode */
	pwrap_write(DEW_WRITE_TEST, 0x55AA);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_WRITE_TEST);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);

	/* delay for 5 seconds? */
	pwrap_delay_us(5000);
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL) & 0xFFFF;
	if (rdata != 0x55AA) {
		PWRAPERR("status update test error, rdata=%x", rdata);
		return 1;
	}
	/* the same as write test */
#if 0
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x55AA);
#endif
#endif
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/* [2]:  SIG_ERR1: Signature1 Checking failed. */
static int _int_test_bit2(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 2;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	WRAP_WR32(PMIC_WRAP_ADC_RDY_ADDR, 0x8001);

	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/* 
 * [5]:  MAN_CMD_MISS: A MAN CMD is written while MAN is disabled.
 * Disable MAN, send a manual command
 */
static int _int_test_bit5(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	unsigned int return_value = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 5;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	/* disable MAN */
	WRAP_WR32(PMIC_WRAP_MAN_EN, 0);

	return_value = _pwrap_manual_mode(OP_WR, OP_CSH, 0, &rdata);
	PWRAPLOG("return_value of _pwrap_manual_mode=%x\n", return_value);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [14]: WACS0_CMD_MISS: A WACS0 CMD is written while WACS0 is disabled.
 * Disable WACS0_EN, send a wacs0 command
 */
static int _int_test_bit14(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 14;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	/* disable MAN */
	WRAP_WR32(PMIC_WRAP_WACS0_EN, 0);

	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [17]: WACS1_CMD_MISS: A WACS1 CMD is written while WACS1 is disabled.
 * Disable WACS1_EN, send a wacs1 command
 */
static int _int_test_bit17(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 17;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	/* disable MAN */
	WRAP_WR32(PMIC_WRAP_WACS1_EN, 0);

	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [20]: WACS2_CMD_MISS: A WACS2 CMD is written while WACS2 is disabled.
 * Disable WACS2_EN, send a wacs2 command
 */
static int _int_test_bit20(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 20;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	/* disable MAN */
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0);

	pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [3]:  MAN_UNEXP_VLDCLR: MAN unexpected VLDCLR
 * Send a manual write command,and clear valid big
 */
static int _int_test_bit3(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	unsigned int return_value = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 3;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	_pwrap_switch_mux(1);
	return_value = _pwrap_manual_mode(OP_WR, OP_CSH, 0, &rdata);
	PWRAPLOG("return_value of _pwrap_manual_mode=%x\n", return_value);
	WRAP_WR32(PMIC_WRAP_MAN_VLDCLR, 1);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [12]: WACS0_UNEXP_VLDCLR: WACS0 unexpected VLDCLR
 * Send a wacs0 write command,and clear valid big
 */
static int _int_test_bit12(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 12;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR, 1);
	wait_for_completion(&pwrap_done);

	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/*
 * [15]: WACS1_UNEXP_VLDCLR: WACS1 unexpected VLDCLR
 * Send a wacs1 write command,and clear valid big
 */
static int _int_test_bit15(void)
{
	unsigned int rdata = 0;
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 15;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR, 1);

	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}

/* 
 * [18]: WACS2_UNEXP_VLDCLR: WACS2 unexpected VLDCLR
 * Send a wacs2 write command,and clear valid big
 */
static int _int_test_bit18(void)
{
	unsigned int ret = 0;
	PWRAPFUC();
	wait_int_flag = 0;
	int_test_bit = 18;

	ret = pwrap_init();
	if (ret != 0) {
		int_test_fail_count++;
		return int_test_bit;
	}

	_int_test_disable_watch_dog();
	init_completion(&pwrap_done);

	pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
	WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);

	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else {
		ret = int_test_bit;
		PWRAPLOG("%s fail\n", __FUNCTION__);
	}

	return ret;
}
#if 0
/*
 * [21]: PERI_WRAP_INT: PERI_PWRAP_BRIDGE interrupt is asserted.
 * send a wacs3 write command,and clear valid big
 */
static int _int_test_bit21( )
{
  //unsigned int rdata=0;
  //unsigned int ret=0;
  PWRAPFUC();
  int_test_bit=21;

  wait_int_flag=0;
  //pwrap_wacs3(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  //WRAP_WR32(PERI_PWRAP_BRIDGE_WACS3_VLDCLR , 1);

  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("%s pass\n", __FUNCTION__);
  else
    PWRAPLOG("%s fail\n", __FUNCTION__);
  return 0;
}
#endif

static int tc_int_test(void)
{
	unsigned int ret = 0;
	unsigned int return_value = 0;

	int_test_fail_count = 0;

	/* switch IRQ handler */
	choose_lisr = INT_TEST; 

	ret = _int_test_bit1();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit5();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit14();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit17();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit20();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit3();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit12();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit15();
	if (ret != 0)
		return_value |= (1 << ret);

	ret = _int_test_bit18();
	if (ret != 0)
		return_value |= (1 << ret);
/*
	ret = _int_test_bit21();
	ret = pwrap_init();
*/
	/* switch back IRQ handler */
	choose_lisr = NORMAL_TEST; 

	if (int_test_fail_count == 0) {
		PWRAPLOG("%s pass. ret=0x%x\n", __FUNCTION__, return_value);
		return 0;
	} else {
		PWRAPLOG("%s fail. fail_count=%d ret=0x%x\n",
			__FUNCTION__, int_test_fail_count, return_value);
		return return_value;
	}
}
/* interrupt test end */

/* concurrence test start */
static U16 wacs0_test_value = 0x10;
static U16 wacs1_test_value = 0x20;
static U16 wacs2_test_value = 0x30;

U32 g_spm_pass_count0 = 0;
U32 g_spm_fail_count0 = 0;
U32 g_spm_pass_count1 = 0;
U32 g_spm_fail_count1 = 0;

U32 g_wacs0_pass_count0 = 0;
U32 g_wacs0_fail_count0 = 0;
U32 g_wacs0_pass_count1 = 0;
U32 g_wacs0_fail_count1 = 0;

U32 g_wacs1_pass_count0 = 0;
U32 g_wacs1_fail_count0 = 0;
U32 g_wacs1_pass_count1 = 0;
U32 g_wacs1_fail_count1 = 0;

U32 g_wacs2_pass_count0 = 0;
U32 g_wacs2_fail_count0 = 0;
U32 g_wacs2_pass_count1 = 0;
U32 g_wacs2_fail_count1 = 0;

U32 g_stress0_cpu0_count=0;
U32 g_stress1_cpu0_count=0;
U32 g_stress2_cpu0_count=0;
U32 g_stress3_cpu0_count=0;
U32 g_stress4_cpu0_count=0;
//U32 g_stress5_cpu0_count=0;
U32 g_stress0_cpu1_count=0;
U32 g_stress1_cpu1_count=0;
U32 g_stress2_cpu1_count=0;
U32 g_stress3_cpu1_count=0;
U32 g_stress4_cpu1_count=0;
U32 g_stress5_cpu1_count=0;

U32 g_stress0_cpu0_count0=0;
U32 g_stress1_cpu0_count0=0;
U32 g_stress0_cpu1_count0=0;

U32 g_stress0_cpu0_count1=0;
U32 g_stress1_cpu0_count1=0;
U32 g_stress0_cpu1_count1=0;

U32 g_stress2_cpu0_count1=0;
U32 g_stress3_cpu0_count1=0;

U32 g_random_count0=0;
U32 g_random_count1=0;
U32 g_wacs0_pass_cpu0=0;
U32 g_wacs0_pass_cpu1=0;
U32 g_wacs0_pass_cpu2=0;
U32 g_wacs0_pass_cpu3=0;

U32 g_wacs0_fail_cpu0=0;
U32 g_wacs0_fail_cpu1=0;
U32 g_wacs0_fail_cpu2=0;
U32 g_wacs0_fail_cpu3=0;

U32 g_wacs1_pass_cpu0=0;
U32 g_wacs1_pass_cpu1=0;
U32 g_wacs1_pass_cpu2=0;
U32 g_wacs1_pass_cpu3=0;

U32 g_wacs1_fail_cpu0=0;
U32 g_wacs1_fail_cpu1=0;
U32 g_wacs1_fail_cpu2=0;
U32 g_wacs1_fail_cpu3=0;

U32 g_wacs2_pass_cpu0=0;
U32 g_wacs2_pass_cpu1=0;
U32 g_wacs2_pass_cpu2=0;
U32 g_wacs2_pass_cpu3=0;

U32 g_wacs2_fail_cpu0=0;
U32 g_wacs2_fail_cpu1=0;
U32 g_wacs2_fail_cpu2=0;
U32 g_wacs2_fail_cpu3=0;

struct task_struct *spm_task;
U32 spm_cpu_id = 1;

static struct task_struct *wacs0_task;
static struct task_struct *wacs1_task;
static struct task_struct *wacs2_task;
unsigned int wacs0_cpu_id = 0;
unsigned int wacs1_cpu_id = 0;
unsigned int wacs2_cpu_id = 0;

static struct task_struct *log0_task;
unsigned int log0_cpu_id = 0;

unsigned int log1_task = 0;
unsigned int log1_cpu_id = 1;

static struct task_struct *kthread_stress0_cpu0;
unsigned int stress0_cpu_id = 0;

static struct task_struct *kthread_stress1_cpu0;
unsigned int stress1_cpu_id = 0;

static struct task_struct *kthread_stress2_cpu0;
unsigned int stress2_cpu_id = 0;

static struct task_struct *kthread_stress3_cpu0;
unsigned int stress3_cpu_id = 0;

static struct task_struct *kthread_stress4_cpu0;
unsigned int stress4_cpu_id=0;

static struct task_struct *kthread_stress0_cpu1;
unsigned int stress01_cpu_id=0;

static struct task_struct *kthread_stress1_cpu1;
static struct task_struct *kthread_stress2_cpu1;
static struct task_struct *kthread_stress3_cpu1;
static struct task_struct *kthread_stress4_cpu1;
static struct task_struct *kthread_stress5_cpu1;

void _concurrence_wacs0_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)prandom_u32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
	{
		pwrap_wacs0(1, WACS0_TEST_REG, wacs0_test_value, &rdata);
      //printk("write (using WACS0),value=%x\n", wacs0_test_value);
		pwrap_wacs0(0, WACS0_TEST_REG, wacs0_test_value, &rdata);
      //printk("read (using WACS0),rdata=%x\n", rdata);

		if( rdata != wacs0_test_value )
		{
			g_wacs0_fail_count0++;
			PWRAPERR("read test error(using WACS0),wacs0_test_value=%x, rdata=%x\n", wacs0_test_value, rdata);
			switch ( raw_smp_processor_id())
			{
			  case 0:
				g_wacs0_fail_cpu0++;
				break;
			  case 1:
				g_wacs0_fail_cpu1++;
				break;
			  case 2:
				g_wacs0_fail_cpu2++;
				break;
			  case 3:
				g_wacs0_fail_cpu3++;
				break;
			  default:
				break;
			}
			  //PWRAPERR("concurrence_fail_count_cpu2=%d", ++concurrence_fail_count_cpu0);
		}
		else
		{
			g_wacs0_pass_count0++;
		  //PWRAPLOG("WACS0 concurrence_test pass,rdata=%x.\n",rdata);
		  //PWRAPLOG("WACS0 concurrence_test pass,concurrence_pass_count_cpu0=%d\n",++concurrence_pass_count_cpu0);

			switch ( raw_smp_processor_id())
			{
			  case 0:
				g_wacs0_pass_cpu0++;
				break;
			  case 1:
				g_wacs0_pass_cpu1++;
				break;
			  case 2:
				g_wacs0_pass_cpu2++;
				break;
			  case 3:
				g_wacs0_pass_cpu3++;
				break;
			  default:
				break;
			}
		}
		  wacs0_test_value+=0x1;

    }
  }//end of while(1)
}
void _concurrence_wacs1_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)prandom_u32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
	{
      pwrap_wacs1(1, WACS1_TEST_REG, wacs1_test_value, &rdata);
      //printk("write (using WACS1),value=%x\n", wacs1_test_value);
      pwrap_wacs1(0, WACS1_TEST_REG, wacs1_test_value, &rdata);
      //printk("read  (using WACS1),rdata=%x\n", rdata);
      if( rdata != wacs1_test_value )
      {
		g_wacs1_fail_count0++;
        PWRAPERR("read test error(using WACS1),wacs1_test_value=%x, rdata=%x\n", wacs1_test_value, rdata);
        switch ( raw_smp_processor_id())
        {
          case 0:
            g_wacs1_fail_cpu0++;
            break;
          case 1:
            g_wacs1_fail_cpu1++;
            break;
          case 2:
            g_wacs1_fail_cpu2++;
            break;
          case 3:
            g_wacs1_fail_cpu3++;
            break;
          default:
            break;
        }
            // PWRAPERR("concurrence_fail_count_cpu1=%d", ++concurrence_fail_count_cpu1);
	  }
	  else
	  {
		g_wacs1_pass_count0++;
        switch ( raw_smp_processor_id())
        {
          case 0:
            g_wacs1_pass_cpu0++;
            break;
          case 1:
            g_wacs1_pass_cpu1++;
            break;
          case 2:
            g_wacs1_pass_cpu2++;
            break;
          case 3:
            g_wacs1_pass_cpu3++;
            break;
          default:
            break;
        }
      }
    wacs1_test_value+=0x3;
  }
  }//end of while(1)
}
void _concurrence_wacs2_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)prandom_u32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
    {

		pwrap_write(WACS2_TEST_REG, wacs2_test_value);
      //printk("write (using WACS2),value=%x\n", wacs2_test_value);
      pwrap_read(WACS2_TEST_REG,  &rdata);
      if( rdata != wacs2_test_value )
      {
		g_wacs2_fail_count0++;
        switch ( raw_smp_processor_id())
        {
          case 0:
            g_wacs2_fail_cpu0++;
            break;
          case 1:
            g_wacs2_fail_cpu1++;
            break;
          case 2:
            g_wacs2_fail_cpu2++;
            break;
          case 3:
            g_wacs2_fail_cpu3++;
            break;
          default:
            break;
        }
        PWRAPERR("read test error(using WACS2),wacs2_test_value=%x, rdata=%x\n", wacs2_test_value, rdata);
      }
      else
      {
		g_wacs2_pass_count0++;
        switch ( raw_smp_processor_id())
        {
          case 0:
            g_wacs2_pass_cpu0++;
            break;
          case 1:
            g_wacs2_pass_cpu1++;
            break;
          case 2:
            g_wacs2_pass_cpu2++;
            break;
          case 3:
            g_wacs2_pass_cpu3++;
            break;
          default:
            break;
        }
      }////end of if( rdata != wacs2_test_value )
      wacs2_test_value+=0x2;
	}
  }//end of while(1)
}

void _concurrence_spm_test_code(unsigned int spm)
{
  PWRAPFUC();
#ifdef ENABLE_SPM_ON_LDVT
  U32 i=0;
  //while(i<20)
  while(1)
  {
    //mtk_pmic_dvfs_wrapper_test(10);
    //i--;
  }
#endif
}

static void _concurrence_log0(unsigned int spm)
{
	PWRAPFUC();
	unsigned int i = 1;
	unsigned int index = 0;
	unsigned int cpu_id = 0;
	unsigned int rand_number = 0;
	unsigned int reg_value = 0;
	while (1) {
		if ((i % 1000000) == 0) {
		 // PWRAPLOG("spm,pass count=%d,fail count=%d\n",g_spm_pass_count0,g_spm_fail_count0);

			PWRAPLOG("wacs0,cup0,pass count=%d,fail count=%d\n",g_wacs0_pass_cpu0,g_wacs0_fail_cpu0);
			PWRAPLOG("wacs1,cup0,pass count=%d,fail count=%d\n",g_wacs1_pass_cpu1,g_wacs1_fail_cpu1);
			PWRAPLOG("wacs2,cup0,pass count=%d,fail count=%d\n",g_wacs2_pass_cpu2,g_wacs2_fail_cpu2);
			PWRAPLOG("\n");
			//PWRAPLOG("wacs4,pass count=%d,fail count=%d\n",g_wacs4_pass_count0,g_wacs4_fail_count0);
#if 0
			PWRAPLOG("g_stress0_cpu0_count0=%d\n",g_stress0_cpu0_count0);
			PWRAPLOG("g_stress1_cpu0_count0=%d\n",g_stress1_cpu0_count0);
			PWRAPLOG("g_stress0_cpu1_count0=%d\n",g_stress0_cpu1_count0);
			PWRAPLOG("g_random_count0=%d\n",g_random_count0);
			PWRAPLOG("g_random_count1=%d\n",g_random_count1);
#endif
			reg_value = WRAP_RD32(PMIC_WRAP_HARB_STA1);
			PWRAPLOG("PMIC_WRAP_HARB_STA1=%d\n",reg_value);
			//reg_value = WRAP_RD32(PMIC_WRAP_RRARB_STA1);
			//PWRAPLOG("PMIC_WRAP_RRARB_STA1=%d\n",reg_value);
			//reg_value = WRAP_RD32(PERI_PWRAP_BRIDGE_IARB_STA1);
			//PWRAPLOG("PERI_PWRAP_BRIDGE_IARB_STA1=%d\n",reg_value);

		//}
		//if((i%1000000)==0)
		//if(0)
		//{
//			rand_number=(unsigned int)prandom_u32();
//			if((rand_number%2)==1)
//			{
//				cpu_id=((spm_cpu_id++)%2);
//				if (wait_task_inactive(spm_task, TASK_UNINTERRUPTIBLE))
//				{
//					PWRAPLOG("spm_cpu_id=%d\n",cpu_id);
//					kthread_bind(spm_task, cpu_id);
//				}
//				else
//				 spm_cpu_id--;
//			}


			rand_number = (unsigned int)prandom_u32();
			if ((rand_number % 2) == 1) {
				cpu_id=(wacs0_cpu_id++)%2;
				if (wait_task_inactive(wacs0_task, TASK_UNINTERRUPTIBLE))
				{
					PWRAPLOG("wacs0_cpu_id=%d\n",cpu_id);
					kthread_bind(wacs0_task, cpu_id);
				}
				else
				 wacs0_cpu_id--;
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(wacs1_cpu_id++)%2;
				if (wait_task_inactive(wacs1_task, TASK_UNINTERRUPTIBLE))
				{
					PWRAPLOG("wacs1_cpu_id=%d\n",cpu_id);
					kthread_bind(wacs1_task, cpu_id);
				}
				else
				 wacs1_cpu_id--;
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(wacs2_cpu_id++)%2;
				if (wait_task_inactive(wacs2_task, TASK_UNINTERRUPTIBLE))
				{
					PWRAPLOG("wacs2_cpu_id=%d\n",cpu_id);
					kthread_bind(wacs2_task, cpu_id);
				}
				else
				 wacs2_cpu_id--;
			}

#if 0
			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress0_cpu_id++)%2;
				//kthread_bind(kthread_stress0_cpu0, cpu_id);
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress1_cpu_id++)%2;
				//kthread_bind(kthread_stress1_cpu0, cpu_id);
			}
			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress2_cpu_id++)%2;
				//kthread_bind(kthread_stress2_cpu0, cpu_id);
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress3_cpu_id++)%2;
				//kthread_bind(kthread_stress3_cpu0, cpu_id);
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress4_cpu_id++)%2;
				//kthread_bind(kthread_stress4_cpu0, cpu_id);
			}

			rand_number=(unsigned int)prandom_u32();
			if((rand_number%2)==1)
			{
				cpu_id=(stress01_cpu_id++)%2;
				//kthread_bind(kthread_stress0_cpu1, cpu_id);
			}
#endif
		}
		i++;
	 }

}

static void _concurrence_log1(unsigned int spm)
{
	PWRAPFUC();
	unsigned int i = 0;
	//while(i<20)
	while(1) {
		//log---------------------------------------------------------------
		//if((test_count0%10000)==0)
		if((i % 100) == 0) {
			//PWRAPLOG("spm,pass count=%d,fail count=%d\n", g_spm_pass_count0, g_spm_fail_count0);
			PWRAPLOG("wacs0,pass count=%d,fail count=%d\n",g_wacs0_pass_count0,g_wacs0_fail_count0);
			PWRAPLOG("wacs1,pass count=%d,fail count=%d\n",g_wacs1_pass_count0,g_wacs1_fail_count0);
			PWRAPLOG("wacs2,pass count=%d,fail count=%d\n",g_wacs2_pass_count0,g_wacs2_fail_count0);
			PWRAPLOG("\n");
			//PWRAPLOG("g_stress0_cpu1_count=%d\n",g_stress0_cpu1_count);
#if 0
			PWRAPLOG("g_stress0_cpu0_count1=%d\n",g_stress0_cpu0_count1);
			PWRAPLOG("g_stress1_cpu0_count1=%d\n",g_stress1_cpu0_count1);
			PWRAPLOG("g_stress0_cpu1_count1=%d\n",g_stress0_cpu1_count1);
#endif

		}
		i++;
	 }

}

static int tc_concurrence_test(void)
{
	unsigned int res = 0;
	unsigned int rdata = 0;
	unsigned int i = 0;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	PWRAPFUC();
#if 0
	spm_task = kthread_create(_concurrence_spm_test_code,0,"spm_concurrence");
	if (IS_ERR(spm_task)) {
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(spm_task, spm_cpu_id);
	wake_up_process(spm_task);
#endif

	wacs0_task = kthread_create(_concurrence_wacs0_test,0,"wacs0_concurrence");
	if(IS_ERR(wacs0_task)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(wacs0_task, wacs0_cpu_id);
	wake_up_process(wacs0_task);

	wacs1_task = kthread_create(_concurrence_wacs1_test,0,"wacs1_concurrence");
	if(IS_ERR(wacs1_task)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(wacs1_task, wacs1_cpu_id);
	wake_up_process(wacs1_task);

	wacs2_task = kthread_create(_concurrence_wacs2_test,0,"wacs2_concurrence");
	if(IS_ERR(wacs2_task)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(wacs2_task, wacs2_cpu_id);
	wake_up_process(wacs2_task);
   
	log0_task = kthread_create(_concurrence_log0,0,"log0_concurrence");
	if(IS_ERR(log0_task)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//sched_setscheduler(log0_task, SCHED_FIFO, &param);
	kthread_bind(log0_task, log0_cpu_id);
	wake_up_process(log0_task);

	log1_task = kthread_create(_concurrence_log1,0,"log1_concurrence");
	if(IS_ERR(log1_task)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//sched_setscheduler(log1_task, SCHED_FIFO, &param);
	kthread_bind(log1_task, log1_cpu_id);
	wake_up_process(log1_task);
#ifdef stress_test_on_concurrence
	//increase cpu load
	kthread_stress0_cpu0 = kthread_create(_concurrence_stress0_cpu0,0,"stress0_cpu0_concurrence");
	if(IS_ERR(kthread_stress0_cpu0)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	kthread_bind(kthread_stress0_cpu0, 0);
	wake_up_process(kthread_stress0_cpu0);

	kthread_stress1_cpu0 = kthread_create(_concurrence_stress1_cpu0,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress1_cpu0)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	kthread_bind(kthread_stress1_cpu0, 0);
	wake_up_process(kthread_stress1_cpu0);

	kthread_stress2_cpu0 = kthread_create(_concurrence_stress2_cpu0,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress2_cpu0)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress2_cpu0, 0);
	wake_up_process(kthread_stress2_cpu0);

	kthread_stress3_cpu0 = kthread_create(_concurrence_stress3_cpu0,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress3_cpu0)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress3_cpu0, 0);
	wake_up_process(kthread_stress3_cpu0);

	//kthread_stress4_cpu0 = kthread_create(_concurrence_stress4_cpu0,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress4_cpu0)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress4_cpu0, 1);
	//wake_up_process(kthread_stress4_cpu0);

	kthread_stress0_cpu1 = kthread_create(_concurrence_stress0_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress0_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	kthread_bind(kthread_stress0_cpu1, 1);
	wake_up_process(kthread_stress0_cpu1);

	kthread_stress1_cpu1 = kthread_create(_concurrence_stress1_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress1_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress1_cpu1, 1);
	wake_up_process(kthread_stress1_cpu1);

	kthread_stress2_cpu1 = kthread_create(_concurrence_stress2_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress2_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress2_cpu1, 0);
	wake_up_process(kthread_stress2_cpu1);

	kthread_stress3_cpu1 = kthread_create(_concurrence_stress3_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress3_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress3_cpu1, 1);
	wake_up_process(kthread_stress3_cpu1);

	kthread_stress4_cpu1 = kthread_create(_concurrence_stress4_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress3_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress4_cpu1, 1);
	wake_up_process(kthread_stress4_cpu1);

	kthread_stress5_cpu1 = kthread_create(_concurrence_stress5_cpu1,0,"stress0_cpu1_concurrence");
	if(IS_ERR(kthread_stress3_cpu1)){
		PWRAPERR("Unable to start kernelthread \n");
		res = -5;
	}
	//kthread_bind(kthread_stress5_cpu1, 1);
	wake_up_process(kthread_stress5_cpu1);

#endif //stress test
	if(res == 0)
	{
		//delay 1 hour
		unsigned int i,j;
		for(i=0;i<1;i++)
			for(j=0;j<60;j++)
				msleep(60000);
		PWRAPLOG("stop concurrent thread \n");

		//kthread_stop(spm_task);
		kthread_stop(wacs0_task);
		kthread_stop(wacs1_task);
		kthread_stop(wacs2_task);
		kthread_stop(log0_task);
		kthread_stop(log1_task);
	}

	if(res == 0) {
		unsigned int count = g_wacs0_fail_count0 + g_wacs1_fail_count0 + g_wacs2_fail_count0;
		if (count = 0) {
			PWRAPLOG("%s pass\n", __FUNCTION__);
		} else {
			PWRAPLOG("%s fail, count=%d\n", __FUNCTION__, count);
		}
	} else {
		PWRAPLOG("%s build environment fail, ret=%d\n",
			__FUNCTION__, res);
	}
	return res;
}
/* concurrence test end */

/* throughput test start */
unsigned int index_wacs0 = 0;
unsigned int index_wacs1 = 0;
unsigned int index_wacs2 = 0;
unsigned long long start_time_wacs0 = 0;
unsigned long long start_time_wacs1 = 0;
unsigned long long start_time_wacs2 = 0;
unsigned long long end_time_wacs0 = 0;
unsigned long long end_time_wacs1 = 0;
unsigned long long end_time_wacs2 = 0;

static void _throughput_wacs0_test(void)
{
	unsigned int rdata = 0;
	PWRAPFUC();

	start_time_wacs0 = sched_clock();
	for (index_wacs0 = 0; index_wacs0 < 10000; index_wacs0++)
		pwrap_wacs0(0, WACS0_TEST_REG, 0, &rdata);

	end_time_wacs0 = sched_clock();
	PWRAPLOG("_throughput_wacs0_test send 10000 read command:average time(ns)=%lld\n", end_time_wacs0 - start_time_wacs0);
	PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n",index_wacs0,index_wacs1,index_wacs2);
	PWRAPLOG("start_time_wacs0=%lld start_time_wacs1=%lld start_time_wacs2=%lld\n", start_time_wacs0, start_time_wacs1, start_time_wacs2);
	PWRAPLOG("end_time_wacs0=%lld end_time_wacs1=%lld end_time_wacs2=%lld\n", end_time_wacs0, end_time_wacs1, end_time_wacs2);
}

static int tc_throughput_test(void)
{
	unsigned int ret = 0;
	unsigned long long start_time = 0;
	unsigned long long end_time = 0;

	struct task_struct *wacs0_throughput_task = 0;
	struct task_struct *wacs1_throughput_task = 0;
	struct task_struct *wacs2_throughput_task = 0;

	unsigned int wacs0_throughput_cpu_id = 0;
	unsigned int wacs1_throughput_cpu_id = 1;
	unsigned int wacs2_throughput_cpu_id = 2;

	PWRAPFUC();

	/*disable WDT interrupt */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	/* except for [31] debug_int, [1]: SIGERR, [0]: WDT */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffff8);
#if 0
	PWRAPLOG("write throughput,start.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,8); //Only WACS2
	start_time = sched_clock();
	for (i = 0; i < 10000; i++)
		pwrap_write(WACS2_TEST_REG, 0x30);

	end_time = sched_clock();
	PWRAPLOG("send 100 write command:average time(ns)=%llx.\n",(end_time-start_time));//100000=100*1000
	PWRAPLOG("write throughput,end.\n");
#endif

#if 1
	dsb(); /* why? */
	PWRAPLOG("1-core read throughput start\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 1 << 1); /* Only WACS0 */
	wacs0_throughput_task = 
		kthread_create(_throughput_wacs0_test, 0, "wacs0_throughput");
	if (!IS_ERR(wacs0_throughput_task)) {
		//kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
		wake_up_process(wacs0_throughput_task);
	}
	pwrap_delay_us(5000);
	//kthread_stop(wacs0_throughput_task);
	PWRAPLOG("stop wacs0_throughput_task\n");
	PWRAPLOG("1-core read throughput end\n");
#endif

#if 0
	dsb();
	PWRAPLOG("2-core read throughput start\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 6); //Only WACS0 and WACS1
	wacs0_throughput_task = kthread_create(_throughput_wacs0_test, 0, 
						"wacs0_concurrence");
	if (IS_ERR(wacs0_throughput_task))
    		PWRAPERR("Unable to start kernelthread \n");
	kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
	wake_up_process(wacs0_throughput_task);

	wacs1_throughput_task = kthread_create(_throughput_wacs1_test, 0, 
						"wacs1_concurrence");
	if (IS_ERR(wacs1_throughput_task))
    		PWRAPERR("Unable to start kernelthread \n");
	kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
	wake_up_process(wacs1_throughput_task);

	pwrap_delay_us(50000);
	//kthread_stop(wacs0_throughput_task);
	//kthread_stop(wacs1_throughput_task);
	PWRAPLOG("stop wacs0_throughput_task and wacs1_throughput_task.\n");
	PWRAPLOG("2-core read throughput,end.\n");
#endif

#if 0
	dsb();
	PWRAPLOG("3-core read throughput start\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0xE); //Only WACS0 and WACS1
	wacs0_throughput_task = kthread_create(_throughput_wacs0_test,0,"wacs0_concurrence");
	kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
	wake_up_process(wacs0_throughput_task);

	wacs1_throughput_task = kthread_create(_throughput_wacs1_test,0,"wacs1_concurrence");
	kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
	wake_up_process(wacs1_throughput_task);

	wacs2_throughput_task = kthread_create(_throughput_wacs2_test,0,"wacs2_concurrence");
	kthread_bind(wacs2_throughput_task, wacs2_throughput_cpu_id);
	wake_up_process(wacs2_throughput_task);
	pwrap_delay_us(50000);
	//kthread_stop(wacs0_throughput_task);
	//kthread_stop(wacs1_throughput_task);
	//kthread_stop(wacs2_throughput_task);
	//PWRAPLOG("stop wacs0_throughput_task /wacs1_throughput_task/wacs2_throughput_task.\n");
	PWRAPLOG("3-core read throughput,end.\n");
#endif

	if (ret == 0)
		PWRAPLOG("%s pass\n", __FUNCTION__);
	else
		PWRAPLOG("%s fail, ret=%d\n", __FUNCTION__, ret);

	return ret;
}
/* throughput test end */

#ifdef CONFIG_OF
static int pwrap_of_iomap(void)
{
	struct device_node *toprgu_node;
	struct device_node *topckgen_node;

	toprgu_node =
		of_find_compatible_node(NULL, NULL, "mediatek,TOPRGU");
	if (!toprgu_node) {
		pr_info("get TOPRGU failed\n");
		return -ENODEV;
	}

	toprgu_reg_base= of_iomap(toprgu_node, 0);
	if (!toprgu_reg_base) {
		pr_info("TOPRGU iomap failed\n");
		return -ENOMEM;
	}

	topckgen_node =
		of_find_compatible_node(NULL, NULL, "mediatek,TOPCKGEN");
	if (!topckgen_node) {
		pr_info("get TOPCKGEN failed\n");
		return -ENODEV;
	}

	topckgen_base = of_iomap(topckgen_node, 0);
	if (!topckgen_base) {
		pr_info("TOPCKGEN iomap failed\n");
		return -ENOMEM;
	}

	pr_info("TOPRGU reg: 0x%p\n", toprgu_reg_base);
	pr_info("TOPCKGEN reg: 0x%p\n", topckgen_base);
	return 0;
	return 0;
}

static void pwrap_of_unmap(void)
{
	iounmap(toprgu_reg_base);
	iounmap(topckgen_base);
}
#endif

static long pwrap_ldvt_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
/*
	int __user *argp = (int __user *)arg;
*/
	int ret = 0;

	switch (cmd) {
	case WRAP_UVVF_INIT:
		PWRAPLOG("WRAP_UVVF_INIT test start\n");
		ret = tc_wrap_init_test();
		break;

	case WRAP_UVVF_WACS_TEST:
		PWRAPLOG("WRAP_UVVF_WACS_TEST test\n");
		ret = tc_wrap_access_test();
		break;

	case WRAP_UVVF_STATUS_UPDATE:
		PWRAPLOG("WRAP_UVVF_STATUS_UPDATE test\n");
		ret = tc_status_update_test();
		break;

	case WRAP_UVVF_DUAL_IO:
		PWRAPLOG("WRAP_UVVF_DUAL_IO test start\n");
		ret = tc_dual_io_test();
		break;

	case WRAP_UVVF_REG_RW:
		PWRAPLOG("WRAP_UVVF_REG_RW test\n");
		ret = tc_reg_rw_test();
		break;

	case WRAP_UVVF_MUX_SWITCH:
		PWRAPLOG("WRAP_UVVF_MUX_SWITCH test\n");
		ret = tc_mux_switch_test();
		break;

	case WRAP_UVVF_RESET_PATTERN:
		PWRAPLOG("WRAP_UVVF_RESET_PATTERN test\n");
		ret = tc_reset_pattern_test();
		break;

	case WRAP_UVVF_SOFT_RESET:
		PWRAPLOG("WRAP_UVVF_SOFT_RESET test\n");
		ret = tc_soft_reset_test();
		break;
#if 1
	case WRAP_UVVF_HIGH_PRI:
		PWRAPLOG("WRAP_UVVF_HIGH_PRI test\n");
		ret = tc_high_pri_test();
		break;
#endif
	case WRAP_UVVF_SPI_ENCRYPTION_TEST:
		PWRAPLOG("WRAP_UVVF_SPI_ENCRYPTION_TEST test\n");
		ret = tc_spi_encryption_test();
		break;


	case WRAP_UVVF_WDT_TEST:
		PWRAPLOG("WRAP_UVVF_WDT_TEST test\n");
		ret = tc_wdt_test();
		break;

	case WRAP_UVVF_INT_TEST:
		PWRAPLOG("WRAP_UVVF_INT_TEST test\n");
		ret = tc_int_test();
		break;

	case WRAP_UVVF_CONCURRENCE_TEST:
		PWRAPLOG("WRAP_UVVF_CONCURRENCE_TEST test\n");
		ret = tc_concurrence_test();
		//ret = tc_eint_normal_test();
		break;
#if 0
	case WRAP_UVVF_CLOCK_GATING:
		PWRAPLOG("WRAP_UVVF_CLOCK_GATING test\n");
		ret = tc_clock_gating_test();
		break;
#endif
	case WRAP_UVVF_THROUGHPUT:
		PWRAPLOG("WRAP_UVVF_THROUGHPUT test\n");
		ret = tc_throughput_test();
		break;

	case WRAP_UVVF_STATUS_UPDATE_SCHEME:
		PWRAPLOG("WRAP_UVVF_STATUS_UPDATE_SCHEME test\n");
		ret = tc_status_update_scheme_test();
		break;
        
	default:
		PWRAPLOG("default test\n");
		break;
	}

	return ret;
}

static const struct file_operations pwrap_ldvt_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= pwrap_ldvt_ioctl,
};

static struct miscdevice pwrap_ldvt_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= PWRAP_LDVT_NAME,
	.fops		= &pwrap_ldvt_fops,
};

static int __init pwrap_ldvt_init(void)
{
	int ret;
#ifdef CONFIG_OF
	struct device_node *pwrap_node;

	pwrap_node = of_find_compatible_node(NULL, NULL, "mediatek,PWRAP");
	if (!pwrap_node) {
		pr_info("PWRAP get node failed\n");
		return -ENODEV;
	}

	pwrap_irq = irq_of_parse_and_map(pwrap_node, 0);
	if (!pwrap_irq) {
		pr_info("PWRAP get irq fail\n");
		return -ENODEV;
	}

	ret = pwrap_of_iomap();
	if (ret)
		return ret;
#endif

	PWRAPLOG("LDVT init\n");

	ret = misc_register(&pwrap_ldvt_miscdev);
	if (ret) {
		PWRAPERR("register miscdev failed\n");
		return ret;
	}

#ifndef CONFIG_OF
	free_irq(MT_PMIC_WRAP_IRQ_ID, NULL);
	ret = request_irq(MT_PMIC_WRAP_IRQ_ID, pwrap_ldvt_interrupt,
			IRQF_TRIGGER_HIGH, PWRAP_LDVT_NAME, NULL);
#else
	free_irq(pwrap_irq, NULL);
	ret = request_irq(pwrap_irq, pwrap_ldvt_interrupt,
				IRQF_TRIGGER_NONE, PWRAP_LDVT_NAME, NULL);
#endif
	if (ret) {
		PWRAPERR("register IRQ hander failed\n");
		misc_deregister(&pwrap_ldvt_miscdev);
		return ret;
	}

	return ret;
}

static void __exit pwrap_ldvt_exit(void)
{
#ifndef CONFIG_OF
	free_irq(MT_PMIC_WRAP_IRQ_ID, NULL);
#else
	free_irq(pwrap_irq, NULL);
#endif
	misc_deregister(&pwrap_ldvt_miscdev);
}

module_init(pwrap_ldvt_init);
module_exit(pwrap_ldvt_exit);
