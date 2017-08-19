#ifdef BUILD_LK
#else
#include <linux/string.h>
#if defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#endif
#endif
#include "lcm_drv.h"


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(540)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0x00   // END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE									0

#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))

//static kal_bool IsFirstBoot = KAL_TRUE;

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[120];
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

    // Sleep Mode On
	{0x10, 1, {0x00}},

	{REGFLAG_DELAY, 50, {}},
	{0x4F, 1, {0x01}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};



static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 0, {0x00}},
    {REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    // Sleep Mode On
	{0x10, 0, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};





static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned cmd;
        cmd = table[i].cmd;
		
        switch (cmd) {
			
            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
				//MDELAY(10);//soso add or it will fail to send register
       	}
    }
	
}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;


		params->dsi.mode   = 1;//CMD_MODE;
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 20;////3;////3;//2       
		params->dsi.vertical_backporch					= 50;//50;//50
		params->dsi.vertical_frontporch					= 50;//20;//20
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 10;//10
		params->dsi.horizontal_backporch				= 100;//80;
		params->dsi.horizontal_frontporch				= 100;//80;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
        params->dsi.TA_GO =5;
		//params->dsi.compatibility_for_nvk = 1;

		// Bit rate calculation
		//params->dsi.pll_div1=37;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		//params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
		
		// Bit rate calculation
		//params->dsi.pll_div1=0;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		//params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
		
		//params->dsi.fbk_div =18;//20; // fref=26MHz, fvco=fref*(fbk_div+1)*fbk_sel_real/(div1_real*div2_real) 
		params->dsi.PLL_CLOCK=230;		
}

static void lcm_init(void)
{
	unsigned int data_array[64];

    SET_RESET_PIN(1);
	 MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);//Must > 10ms
    SET_RESET_PIN(1);
    MDELAY(150);//Must > 120ms
    /*
            #ifdef BUILD_LK
        DSI_clk_HS_mode(1);
        #endif
        MDELAY(10);
	  data_array[0] = 0x00110508;
	  dsi_set_cmdq(data_array, 1, 1);
	  MDELAY(50);
        #ifdef BUILD_LK
        DSI_clk_HS_mode(0);
        #endif 
		MDELAY(10);*/
	//IsFirstBoot = KAL_TRUE;
	
//nt35517_t,4.5_mipi_20130703
//#Page 0 
data_array[0]= 0x00063902; 
data_array[1]= 0x52aa55f0;
data_array[2]= 0x00000008;
dsi_set_cmdq(data_array, 3, 1);

data_array[0]=0x00033902;
data_array[1]=0x0000fcb1;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00023902;
data_array[1]=0x000003b6;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00033902;
data_array[1]=0x007272b7;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00053902;
data_array[1]=0x005601b8;
data_array[2]= 0x00000004;
dsi_set_cmdq(data_array, 3, 1);


data_array[0]=0x00023902;
data_array[1]=0x000055bb;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x000000bc;
dsi_set_cmdq(data_array, 2, 1);


data_array[0]=0x00063902;
data_array[1]=0x104e01bd;
data_array[2]=0x00000120;
dsi_set_cmdq(data_array, 3, 1);


data_array[0]= 0x00073902;
data_array[1]= 0x0d0661c9;
data_array[2]= 0x00001717;
dsi_set_cmdq(data_array, 3, 1);

data_array[0]= 0x00063902;
data_array[1]= 0x52aa55f0;
data_array[2]= 0x00000108;
dsi_set_cmdq(data_array, 3, 1);


data_array[0]=0x00043902;
data_array[1]=0x0c0c0cb0;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x0c0c0cb1;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x020202b2;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x101010b3;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x060606b4;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x444444b6;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x343434b7;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x343434b8;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x343434b9;
dsi_set_cmdq(data_array, 2, 1);


data_array[0]=0x00043902;
data_array[1]=0x141414ba;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x009800bc;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00043902;
data_array[1]=0x009800bd;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00023902;
data_array[1]=0x000044bf;
dsi_set_cmdq(data_array, 2, 1);

data_array[0]=0x00023902;
data_array[1]=0x00004abe;
dsi_set_cmdq(data_array, 2, 1);


data_array[0]=0x00023902;
data_array[1]=0x000001c1;
dsi_set_cmdq(data_array, 2, 1);


data_array[0]=0x00023902;
data_array[1]=0x000000c2;
dsi_set_cmdq(data_array, 2, 1);


/*data_array[0]= 0x00053902;
data_array[1]= 0x100f0fd0;
data_array[2]= 0x00000010;
dsi_set_cmdq(data_array, 3, 1);



//#Gamma 
data_array[0]=0x00113902; 
data_array[1]=0x000000d1;
data_array[2]=0x004f0020;
data_array[3]=0x008b0071;
data_array[4]=0x01d000b2;
data_array[5]=0x00000003; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x012a01d2;
data_array[2]=0x01950166;
data_array[3]=0x021cb02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402d3;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00053902;
data_array[1]=0x03e603d4;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1);  

data_array[0]=0x00113902; 
data_array[1]=0x000000d5;
data_array[2]=0x004f0020;
data_array[3]=0x008b0071;
data_array[4]=0x01d000b2;
data_array[5]=0x00000003; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x012a01d6;
data_array[2]=0x01950166;
data_array[3]=0x021c02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402d7;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00053902;
data_array[1]=0x03e603d8;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1); 


data_array[0]=0x00113902; 
data_array[1]=0x000000d9;
data_array[2]=0x004f0020;
data_array[3]=0x008b0071;
data_array[4]=0x01d000b2;
data_array[5]=0x00000003; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x012a01dD;
data_array[2]=0x01950166;
data_array[3]=0x021c02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402dE;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00053902;
data_array[1]=0x03e603dF;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1); 

data_array[0]=0x00113902; 
data_array[1]=0x000000E0;
data_array[2]=0x004f0020;
data_array[3]=0x008b0071;
data_array[4]=0x01d000b2;
data_array[5]=0x00000003; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x012a01E1;
data_array[2]=0x01950166;
data_array[3]=0x021c02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402E2;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00053902;
data_array[1]=0x03e603E3;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1); 
 
 data_array[0]=0x00113902; 
 data_array[1]=0x000000E4;
 data_array[2]=0x004f0020;
 data_array[3]=0x008b0071;
 data_array[4]=0x01d000b2;
 data_array[5]=0x00000003; 
 dsi_set_cmdq(data_array, 6, 1);


data_array[0]=0x00113902;
data_array[1]=0x012a01E5;
data_array[2]=0x01950166;
data_array[3]=0x021c02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402E6;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);


data_array[0]=0x00053902;
data_array[1]=0x03e603E7;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1); 


 data_array[0]=0x00113902; 
 data_array[1]=0x000000E8;
 data_array[2]=0x004f0020;
 data_array[3]=0x008b0071;
 data_array[4]=0x01d000b2;
 data_array[5]=0x00000003; 
 dsi_set_cmdq(data_array, 6, 1);


data_array[0]=0x00113902;
data_array[1]=0x012a01E9;
data_array[2]=0x01950166;
data_array[3]=0x021c02df;
data_array[4]=0x0258021d;
data_array[5]=0x0000009a; 
dsi_set_cmdq(data_array, 6, 1);

data_array[0]=0x00113902;
data_array[1]=0x03c402Ea;
data_array[2]=0x03280301;
data_array[3]=0x038c036a;
data_array[4]=0x03c003a6;
data_array[5]=0x000000d2; 
dsi_set_cmdq(data_array, 6, 1);


data_array[0]=0x00053902;
data_array[1]=0x03e603Eb;
data_array[2]=0x000000ff;
dsi_set_cmdq(data_array, 3, 1); 
*/
//#Gamma
	data_array[0]=0x00053902;
	data_array[1]=0x100f0fd0;
	data_array[2]=0x00000010;
	dsi_set_cmdq(data_array, 3, 1);   


	data_array[0]=0x00113902;
	data_array[1]=0x000000d1;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01d2;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402d3;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603d4;
	data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1);   

	data_array[0]=0x00113902;
	data_array[1]=0x000000d5;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01d6;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402d7;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603d8;
	data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1);    
		                                        
	data_array[0]=0x00113902;
	data_array[1]=0x000000d9;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01dd;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402de;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603df;
        data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1);     

	data_array[0]=0x00113902;
	data_array[1]=0x000000e0;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01e1;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402e2;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603e3;
        data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1); 
		                                       
	data_array[0]=0x00113902;
	data_array[1]=0x000000e4;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01e5;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402e6;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603e7;
        data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1); 	                                       
		
        data_array[0]=0x00113902;
	data_array[1]=0x000000e8;
	data_array[2]=0x004f0020;
	data_array[3]=0x008b0071;
	data_array[4]=0x01d000b2;
	data_array[5]=0x00000003;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x012a01e9;
	data_array[2]=0x01950166;
	data_array[3]=0x021c02df;
	data_array[4]=0x0258021d;
	data_array[5]=0x0000009a;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00113902;
	data_array[1]=0x03c402ea;
	data_array[2]=0x03280301;
	data_array[3]=0x038c036a;
	data_array[4]=0x03c003a6;
	data_array[5]=0x000000d2;
	dsi_set_cmdq(data_array, 6, 1);

	data_array[0]=0x00053902;
	data_array[1]=0x03e603eb;
  data_array[2]=0x000000ff;
	dsi_set_cmdq(data_array, 3, 1);   
 
/*data_array[0]=0x00053902;
data_array[1]=0x2555aaff;
data_array[2]=0x00000001;
dsi_set_cmdq(data_array, 3, 1); 

 data_array[0]=0x00083902;
data_array[1]=0x070302f3;
data_array[2]=0x0dd18844;
dsi_set_cmdq(data_array, 3, 1);

data_array[0]=0x00023902;
data_array[1]=0x00000035;
dsi_set_cmdq(data_array, 2, 1);*/



data_array[0] = 0x00110500;
dsi_set_cmdq(data_array, 1, 1);

MDELAY(120);

data_array[0]= 0x00290500;
dsi_set_cmdq(data_array, 1, 1);

MDELAY(10);


}

#if 1//luminmin
static void lcm_suspend(void)
{
#if 1
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / sizeof(struct LCM_setting_table), 1);
#else
		unsigned int data_array[2];

//below BTA for can not sleep in
//	data_array[0]=0x00000504; // BTA
//	dsi_set_cmdq(&data_array, 1, 1);

	data_array[0] = 0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(100); 
	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(200);
#endif	
}

#else
fgfh
static void lcm_suspend(void)
{
    SET_RESET_PIN(1);
	MDELAY(20);
    SET_RESET_PIN(0);
    MDELAY(20);//Must > 10ms
    SET_RESET_PIN(1);
    MDELAY(150);//Must > 120ms

	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}
#endif
 

static unsigned int lcm_compare_id(void)
        {
	unsigned int id = 0, id2 = 0;
	unsigned char buffer[2];

	unsigned int data_array[16];
	
	SET_RESET_PIN(1);  //NOTE:should reset LCM firstly
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);	

/*	
	data_array[0] = 0x00110500;		// Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
*/
		
//*************Enable CMD2 Page1  *******************//
	data_array[0]=0x00063902;
	data_array[1]=0x52AA55F0;
	data_array[2]=0x00000108;
	dsi_set_cmdq(data_array, 3, 1);
	MDELAY(10); 

	data_array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(10); 
	
	read_reg_v2(0xC5, buffer, 2);
	id = buffer[0]; //we only need ID
	id2= buffer[1]; //we test buffer 1
	
	#ifdef BUILD_LK
	printf("NT35517 %s, id = 0x%x \n", __func__, id);
	#endif

        return (0x55 == id)?1:0;
}



static void lcm_resume(void)
{
	lcm_init();
}




LCM_DRIVER nt35517_dsi_vdo_lcm_drv = 
{
	.name			= "nt35517_dsi_vedio",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,	
};
