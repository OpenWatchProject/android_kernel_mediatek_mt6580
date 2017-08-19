


#include "lcm_drv.h"
#if defined(BUILD_LK)
	#include <platform/mt_typedefs.h>
	#include <platform/mt_pmic.h>
	#include <platform/mt_gpio.h>
	#include <platform/mt_pmic.h>
	#include <printf.h>

	#define LCM_PRINT  printf

#else
	#include <linux/string.h>
#if defined(BUILD_UBOOT)
	#include <asm/arch/mt6577_gpio.h>
    #include <asm/arch/mt6577_pmic6329.h>
	//#include <mach/mt_pm_ldo.h>
    #define LCM_PRINT  printf
#else
	#include <mach/mt_typedefs.h>
	//#include <mach/mt6575_pll.h>
	#include <mach/mt_gpio.h>
    #include <mach/mt_pm_ldo.h>
	#define LCM_PRINT  printk
#endif

	#define LCM_PRINT  printk
#endif




#define LCM_DSI_CMD_MODE	    1
extern unsigned int idle_clock_mode;


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(400)
#define FRAME_HEIGHT 										(400)
#define LCM_ID_RM67160                                      0x8012


#define REGFLAG_DELAY             							0xDE
#define REGFLAG_END_OF_TABLE      							0xDF   // END OF REGISTERS MARKER


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
#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define SET_GPIO_OUT(n, v)  (lcm_util.set_gpio_out((n), (v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))
static void lcm_setbacklight(void* handle,unsigned int level);

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_V2(cmd,buffer,buffer_size)					lcm_util.dsi_dcs_read_lcm_reg_v2(cmd,buffer,buffer_size)       


static struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};



static struct LCM_setting_table lcm_initialization_setting[] = 
{
{0xFE,1,{0x07}},
{0x07,1,{0x4F}},
{0xFE,1,{0x0A}},
{0x1C,1,{0x1B }}, //cmd :0x10
{0xFE,1,{0x00}},  
{0x35,1,{0x00}}, 

{0x51, 1, {0x00}},      //switch off backlight untill system's setting
             

	{0x11,1,{0x00}},// Sleep-Out
	{REGFLAG_DELAY, 120, {}},
	{0x29,1,{0x00}},// Display On                                                                                 
  	{REGFLAG_DELAY, 50, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}	

};



static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 99, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
	{REGFLAG_DELAY, 5, {}},
	// Sleep Out
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	{REGFLAG_END_OF_TABLE, 99, {}}
};


static struct LCM_setting_table lcm_clk_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x39, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},

	{REGFLAG_END_OF_TABLE,99, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},

	// Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	{REGFLAG_END_OF_TABLE,99, {}}
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0x00}},
	{0x53, 1, {0x20}},
	{REGFLAG_END_OF_TABLE,99, {}}
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
				UDELAY(10);
				break;
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

	// enable tearing-free
	params->dbi.te_mode 			= LCM_DBI_TE_MODE_VSYNC_ONLY;//LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
	#else
	params->dsi.mode   = SYNC_EVENT_VDO_MODE;
	//params->dsi.mode   = SYNC_PULSE_VDO_MODE;
	#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_ONE_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 2;
	params->dsi.packet_size=256;

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.compatibility_for_nvk = 0;		// this parameter would be set to 1 if DriverIC is NTK's and when force match DSI clock for NTK's

	params->dsi.vertical_sync_active				= 4;
	params->dsi.vertical_backporch					= 12;
	params->dsi.vertical_frontporch					= 20;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 8;
	params->dsi.horizontal_backporch				= 20;
	params->dsi.horizontal_frontporch				= 20;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	// Bit rate calculation
	//params->dsi.pll_div1=30;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
	//params->dsi.pll_div2=0;			// div2=0~15: fout=fvo/(2*div2)
	
  	params->dsi.PLL_CLOCK = 150;//fps 60
}


static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(120);
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	lcm_setbacklight(NULL,0);   ///shtudown backlight again
}


static void lcm_suspend(void)
{
	printk("RM67160 lcm_suspend,idle_clock_mode=%d\n",idle_clock_mode);
	if(idle_clock_mode == 1)
	{
    	push_table(lcm_clk_sleep_mode_in_setting, sizeof(lcm_clk_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	}
	else
	{
		
    	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
		MDELAY(20);
	}
	LCM_PRINT(" =========== %s, %d \n", "rm67160_480x480 lcm_suspend", __LINE__);
}


static void lcm_resume(void)
{
#if 0
	//MDELAY(500);
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
	lcm_setbacklight(NULL,0);   ///shtudown backlight again
	LCM_PRINT(" =========== %s, %d \n", "rm67160_480x480 lcm_resume", __LINE__);
#else
	if(idle_clock_mode == 1)
	{
		//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
		lcm_init();
	}
	else
	{
		lcm_init();
	}
#endif
}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;
    
    unsigned char x0_MSB = ((x0>>8)&0xFF);
    unsigned char x0_LSB = (x0&0xFF);
    unsigned char x1_MSB = ((x1>>8)&0xFF);
    unsigned char x1_LSB = (x1&0xFF);
    unsigned char y0_MSB = ((y0>>8)&0xFF);
    unsigned char y0_LSB = (y0&0xFF);
    unsigned char y1_MSB = ((y1>>8)&0xFF);
    unsigned char y1_LSB = (y1&0xFF);
    
    unsigned int data_array[16];
    

    data_array[0]= 0x00053902;
    data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
    data_array[2]= (x1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0]= 0x00053902;
    data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
    data_array[2]= (y1_LSB);
    dsi_set_cmdq(data_array, 3, 1);
    
    data_array[0]= 0x002c3909;
    dsi_set_cmdq(data_array, 1, 0);

}


static void lcm_setbacklight(void* handle,unsigned int level)
{
	unsigned int default_level = 45;
	unsigned int mapped_level = 0;
	printk("rm67160 lcm_setbacklight\n");
	//for LGE backlight IC mapping table
	if(level > 255) 
			level = 255;

	if(level >0) 
	{
			mapped_level = default_level+(level)*(255-default_level)/(255);
	}
	else
	{
		   if(idle_clock_mode == 1)
		   	{
		   		mapped_level=45;
		   	}else
		   	{
				mapped_level=0;
		   	}
	}
	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = mapped_level;
	LCM_PRINT(" lcm_setbacklight setting level is %d,mapped_level=%d \r\n", level,mapped_level);
	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_setpwm(unsigned int divider)
{
	// TBD
}


static unsigned int lcm_getpwm(unsigned int divider)
{
	// ref freq = 15MHz, B0h setting 0x80, so 80.6% * freq is pwm_clk;
	// pwm_clk / 255 / 2(lcm_setpwm() 6th params) = pwm_duration = 23706
	unsigned int pwm_clk = 23706 / (1<<divider);	
	return pwm_clk;
}


static unsigned int lcm_cmp_id(void)
{

	unsigned int id1=0, id2 = 0;
	unsigned char buffer[4];
	unsigned int array[16];  

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(25);
	SET_RESET_PIN(1);
	MDELAY(50);
	array[0]=0x00043902;
	array[1]=0x010980ff; 
	array[2]=0x80001500;
	array[3]=0x00033902;
	array[4]=0x000980ff;
	dsi_set_cmdq(array, 5, 1);
	MDELAY(10);
	array[0] = 0x00043700;// set return byte number
	dsi_set_cmdq(array, 1, 1);
	
	array[0] = 0x02001500;
	dsi_set_cmdq(array, 1, 1);
	
	read_reg_V2(0xA1, &buffer, 4);

	id1 = buffer[2]<<8 |buffer[3]; 
	#ifdef BUILD_LK
	LCM_PRINT("[rm67160_cmp_id] -- 0x%x , 0x%x \n",id1, id2);
	#endif

	//if(LCM_ID_RM67160 == id1)
		return 1;
	//else
		//return 0;

}



LCM_DRIVER rm67160_400x400_dsi_cmd_oled_drv = 
{
    .name			= "rm67160_400x400_dsi_cmd_oled",
	.set_backlight_cmdq = lcm_setbacklight,
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_cmp_id,
#if defined(LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif	
};



