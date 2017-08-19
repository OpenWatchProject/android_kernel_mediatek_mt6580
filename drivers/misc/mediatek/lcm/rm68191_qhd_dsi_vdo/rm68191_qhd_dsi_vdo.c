#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  			(540)
#define FRAME_HEIGHT 			(960)

#define REGFLAG_DELAY           	(0XFEF)
#define REGFLAG_END_OF_TABLE    	(0xFFF)	// END OF REGISTERS MARKER

#define LCM_DSI_CMD_MODE		(0)

#define LCM_RM68191_ID 		(0x8191)

// ---------------------------------------------------------------------------
// Local Variables
// 

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// 
// Local Functions
// 

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size) lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};


static struct LCM_setting_table lcm_compare_id_setting[] = {
	// Display off sequence
	{0xf0, 5, {0x55, 0xaa, 0x52, 0x08, 0x01}},
	{REGFLAG_DELAY, 10, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {

{0xF0,5,{0x55,0xAA,0x52,0x08,0x03}},
  
//GOA_VSTV1
{0x90,9,{0x03,0x14,0x05,0x00,0x00,0x00,0x54,0x00,0x00}},
  
//GOA_VSTV2
{0x91,9,{0x00,0x00,0x00,0x00,0xD0,0x00,0x54,0x00,0x00}},
  
//GOA_VCLK1
{0x92,11,{0x40,0x07,0x08,0x09,0x0A,0x00,0x6D,0x00,0x00,0x03,0x08}},
  
//GOA_VCLK_OPT1
{0x94,8,{0x00,0x08,0x07,0x03,0xD1,0x03,0xD3,0x0C}},
  
// GOA_BICLK1  
{0x95,16,{0x40,0x0B,0x00,0x0C,0x00,0x0D,0x00,0x0E,0x00,0x6D,0x00,0x00,0x00,0x03,0x00,0x08}},
  
//GOA_BICLK_OPT1
{0x99,2,{0x00,0x00}},
  
//GOA_BICLK_OPT2
{0x9A,11,{0x80,0x0B,0x03,0xD5,0x03,0xD7,0x00,0x00,0x00,0x00,0x50}},
 
//GOA_GPO1
{0x9B,6,{0x88,0x00,0x00,0x20,0x00,0x20}},
  
//GOA_GPO2
{0x9C,2,{0x80,0x02}},
  
//GOA_EQ 
{0x9D,8,{0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00}},
  
//GOA_CLK_GALLON
{0x9E,2,{0xF3,0x00}},//0xfb
  
//GOA_FS_SEL0  
{0xA0,10,{0x9F,0x1F,0x1F,0x1F,0x01,0x1F,0x1F,0x1F,0x09,0x1F}},
  
//GOA_FS_SEL1  
{0xA1,10,{0x0B,0x1F,0x0D,0x1F,0x0F,0x1F,0x1F,0x1F,0x10,0x1F}},
  
//GOA_FS_SEL2  
{0xA2,10,{0x1F,0x1F,0x11,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
  
//GOA_FS_SEL3  
{0xA3,10,{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
  
//GOA_FS_SEL4  
{0xA4,10,{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x11,0x1F,0x1F}},
  
// GOA_FS_SEL5 
{0xA5,10,{0x1F,0x10,0x1F,0x1F,0x1F,0x0E,0x1F,0x0C,0x1F,0x0A}},
  
//GOA_FS_SEL6  
{0xA6,10,{0x1F,0x08,0x1F,0x1F,0x1F,0x00,0x1F,0x1F,0x1F,0x1F}},
  
// GOA_BS_SEL0 
{0xA7,10,{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x1F,0x0A,0x1F}},
  
// GOA_BS_SEL1 
{0xA8,10,{0x08,0x1F,0x0E,0x1F,0x0C,0x1F,0x1F,0x1F,0x10,0x1F}},
  
//  GOA_BS_SEL2
{0xA9,10,{0x1F,0x1F,0x11,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
  
// GOA_BS_SEL3 
{0xAA,10,{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
  
// GOA_BS_SEL4 
{0xAB,10,{0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x11,0x1F,0x1F}},
  
//GOA_BS_SEL5  
{0xAC,10,{0x1F,0x10,0x1F,0x1F,0x1F,0x0D,0x1F,0x0F,0x1F,0x09}},
  
//GOA_BS_SEL6  
{0xAD,10,{0x1F,0x0B,0x1F,0x01,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},
  
//Enable Page 0
{0xF0,5,{0x55,0xAA,0x52,0x08,0x00}},
  
//Video Mode Enable,0x0x15
{0xB1,2,{0xFC,0x00}},

{0x4C,1,{0x11}},
  
//Column inversion  
{0xBC,3,{0x00,0x00,0x00}},

{0xB8,4,{0x01,0x8F,0xBF,0x80}},
 
//Enable Page 1
{0xF0,5,{0x55,0xAA,0x52,0x08,0x01}},
  
// Gamma setting Red +
{0xD1,16,{0x00,0x0C,0x00,0x12,0x00,0x21,0x00,0x2F,0x00,0x3E,
0x00,0x56,0x00,0x6E,0x00,0x97}},

{0xD2,16,{0x00,0xA2,0x00,0xE0,0x01,0x1C,0x01,0x71,0x01,0xC7,
0x01,0xC1,0x01,0xF9,0x02,0x36}},
  
{0xD3,16,{0x02,0x56,0x02,0x8E,0x02,0xB7,0x02,0xEB,0x03,0x13,
0x03,0x3D,0x03,0x5A,0x03,0x88}},

{0xD4,4,{0x03,0xB4,0x03,0xBD}},
  
//Gamma setting Green +
{0xD5,16,{0x00,0x0C,0x00,0x12,0x00,0x21,0x00,0x2F,0x00,0x3E,
0x00,0x56,0x00,0x6E,0x00,0x97}},

{0xD6,16,{0x00,0xA2,0x00,0xE0,0x01,0x1C,0x01,0x71,0x01,0xC7
,0x01,0xC1,0x01,0xF9,0x02,0x36}},

{0xD7,16,{0x02,0x56,0x02,0x8E,0x02,0xB7,0x02,0xEB,0x03,0x13,
0x03,0x3D,0x03,0x5A,0x03,0x88}},

{0xD8,4,{0x03,0xB4,0x03,0xBD}},
  
//Gamma setting Blue +
{0xD9,16,{0x00,0x0C,0x00,0x12,0x00,0x21,0x00,0x2F,0x00,0x3E,
0x00,0x56,0x00,0x6E,0x00,0x97}},

{0xDD,16,{0x00,0xA2,0x00,0xE0,0x01,0x1C,0x01,0x71,0x01,0xC7,
0x01,0xC1,0x01,0xF9,0x02,0x36}},

{0xDE,16,{0x02,0x56,0x02,0x8E,0x02,0xB7,0x02,0xEB,0x03,0x13,
0x03,0x3D,0x03,0x5A,0x03,0x88}},

{0xDF,4,{0x03,0xB4,0x03,0xBD}},
  
//Gamma setting Red -
{0xE0,16,{0x00,0x0B,0x00,0x11,0x00,0x20,0x00,0x2E,0x00,0x3D,
0x00,0x55,0x00,0x6D,0x00,0x96}},

{0xE1,16,{0x00,0xD0,0x01,0x0E,0x01,0x40,0x01,0x8F,0x01,0xC6,
0x01,0xD0,0x02,0x12,0x02,0x5A}},
{0xE2,16,{0x02,0x7F,0x02,0xB7,0x02,0xE0,0x03,0x14,0x03,0x3B
,0x03,0x65,0x03,0x83,0x03,0xB0}},

{0xE3,4,{0x03,0xDD,0x03,0xE5}},
  
//Gamma setting Green -   
{0xE4,16,{0x00,0x0B,0x00,0x11,0x00,0x20,0x00,0x2E,0x00,0x3D,
0x00,0x55,0x00,0x6D,0x00,0x96}},

{0xE5,16,{0x00,0xD0,0x01,0x0E,0x01,0x40,0x01,0x8F,0x01,0xC6,
0x01,0xD0,0x02,0x12,0x02,0x5A}},
{0xE6,16,{0x02,0x7F,0x02,0xB7,0x02,0xE0,0x03,0x14,0x03,0x3B,
0x03,0x65,0x03,0x83,0x03,0xB0}},

{0xE7,4,{0x03,0xDD,0x03,0xE5}},
  
//Gamma setting Blue -
{0xE8,16,{0x00,0x0B,0x00,0x11,0x00,0x20,0x00,0x2E,0x00,0x3D,
0x00,0x55,0x00,0x6D,0x00,0x96}},
  
{0xE9,16,{0x00,0xD0,0x01,0x0E,0x01,0x40,0x01,0x8F,0x01,0xC6,
0x01,0xD0,0x02,0x12,0x02,0x5A}},
  
{0xEA,16,{0x02,0x7F,0x02,0xB7,0x02,0xE0,0x03,0x14,0x03,0x3B,
0x03,0x65,0x03,0x83,0x03,0xB0}},
  
{0xEB,4,{0x03,0xDD,0x03,0xE5}},
  
//AVDD 6.0V
{0xB0,3,{0x05,0x05,0x05}},
  
//AVEE -6.0V   
{0xB1,3,{0x05,0x05,0x05}},
  
// AVDD boosting times : x2.5
{0xB6,3,{0x44,0x44,0x44}},
  
//AVEE boosting times : x-2.5
{0xB7,3,{0x44,0x44,0x44}},
  
// Setting VGH = 18V
{0xB3,3,{0x14,0x14,0x14}},
  
//Setting VGH boosting time 
{0xB9,3,{0x34,0x34,0x34}},
  
//Setting VGL =-10V 
{0xB4,3,{0x08,0x08,0x08}},
  
//Setting VGL boosting time 
{0xBA,3,{0x14,0x14,0x14}},
  
//VGMP£º4.9V   
{0xBC,3,{0x00,0x98,0x00}},
  
//VGMN£º-4.9V  
{0xBD,3,{0x00,0x98,0x00}},
  
//VCOM   
{0xBE,1,{0x70}},//7D
  
//TE On,0x0x15 
//{0x35,1,{0x00}}, 
  
//Enable Page 2
{0xF0,5,{0x55,0xAA,0x52,0x08,0x02}}, 
  
// HSBIAS
{0xF1,3,{0x22,0x22,0x32}}, 
  
{0xFB,3,{0x09,0x03,0x08}}, 
  
{0xF6,2,{0xCA,0x69}}, 


{0x11,1,{0x00}}, // Standby out
{REGFLAG_DELAY,120,{}},
               
{0x29,1,{0x00}},  //Display On 
{0x2c,1,{0x00}},

};

static struct LCM_setting_table lcm_sleep_out_setting[] = {
	// Sleep Out
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 10, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},

	// Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 50, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		unsigned char force_update)
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
	}
	}

}


// 
// LCM Driver Implementations
// 

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));
	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;	//SYNC_PULSE_VDO_MODE;
#endif

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	// Highly depends on LCD driver capability.
#if (LCM_DSI_CMD_MODE)
	params->dsi.intermediat_buffer_num = 2;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
#else
	params->dsi.intermediat_buffer_num = 0;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
#endif

	// Video mode setting
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = 540 * 3;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 18;
	params->dsi.vertical_frontporch = 18;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 44;
	params->dsi.horizontal_frontporch = 40;	//21;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	// Bit rate calculation
	//1 Every lane speed
	//params->dsi.pll_div1 = 0;	// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	//params->dsi.pll_div2 = 1;	// div2=0,1,2,3;div1_real=1,2,4,4
	//params->dsi.fbk_div = 17 ;//14;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
	
	params->dsi.PLL_CLOCK=230;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(120);

	push_table(lcm_initialization_setting,
			sizeof(lcm_initialization_setting) /
			sizeof(struct LCM_setting_table), 1);
}


static void lcm_suspend(void)
{
	push_table(lcm_sleep_in_setting,
			sizeof(lcm_sleep_in_setting) /
			sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	unsigned int id;
	unsigned char buffer[5];
	unsigned int array[5];
	
	push_table(lcm_sleep_out_setting,
			sizeof(lcm_sleep_out_setting) /
			sizeof(struct LCM_setting_table), 1);
	//lcm_init();
			
	array[0] = 0x00063902;// read id return two byte,version and id
	array[1] = 0x52AA55F0;
	array[2] = 0x00000108;
	dsi_set_cmdq(array, 3, 1);
	MDELAY(10);
	
	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xc5, buffer, 2);
	id = ((buffer[0] << 8) | buffer[1]);
#if defined(BUILD_LK)
printf("%s, [rm68191_ctc50_jhzt]  buffer[0] = [0x%d] buffer[2] = [0x%d] ID = [0x%d]\n",__func__, buffer[0], buffer[1], id);
#else
printk("%s, [rm68191_ctc50_jhzt]  buffer[0] = [0x%d] buffer[2] = [0x%d] ID = [0x%d]\n",__func__, buffer[0], buffer[1], id);
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

	data_array[0] = 0x00053902;
	data_array[1] =
		(x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	data_array[3] = 0x00053902;
	data_array[4] =
		(y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[5] = (y1_LSB);
	data_array[6] = 0x002c3909;

	dsi_set_cmdq(&data_array, 7, 0);

}


static unsigned int lcm_compare_id(void)
{
	unsigned int id;
	unsigned char buffer[5];
	unsigned int array[5];

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(100);
/*
	push_table(lcm_compare_id_setting,
			sizeof(lcm_compare_id_setting) /
			sizeof(struct LCM_setting_table), 1);
*/
	array[0] = 0x00063902;// read id return two byte,version and id
	array[1] = 0x52AA55F0;
	array[2] = 0x00000108;
	dsi_set_cmdq(array, 3, 1);
	MDELAY(10);
	
	array[0] = 0x00023700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xc5, buffer, 2);
	id = ((buffer[0] << 8) | buffer[1]);
#if defined(BUILD_LK)
printf("%s, [rm68191_ctc50_jhzt]  buffer[0] = [0x%d] buffer[2] = [0x%d] ID = [0x%d]\n",__func__, buffer[0], buffer[1], id);
#else
printk("%s, [rm68191_ctc50_jhzt]  buffer[0] = [0x%d] buffer[2] = [0x%d] ID = [0x%d]\n",__func__, buffer[0], buffer[1], id);
#endif

	return ((LCM_RM68191_ID == id)? 1 : 0);
}
//no use
static unsigned int lcm_esd_recover(void)
{
    unsigned char para = 0;
	unsigned int data_array1[16];

#ifndef BUILD_LK
    printk("RM68190 lcm_esd_recover enter\n");
#endif
    

    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(30);
    SET_RESET_PIN(1);
    MDELAY(130);
    #if 0
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
    	MDELAY(10);
    #else
        lcm_init();
    #endif
   
   data_array1[0]= 0x00320500;
   dsi_set_cmdq(&data_array1, 1, 1);
   MDELAY(50);

    return 1;
}
static unsigned int lcm_esd_check(void)
{
    unsigned char buffer[1] ={0};
    //unsigned int data_array[1];
   // data_array[0] = 0x00013700;// read id return two byte,version and id 3 byte 
  // dsi_set_cmdq(&data_array, 1, 1);
   read_reg_v2(0x0a, buffer, 1);
   
#ifndef BUILD_LK
    printk("RM68190 lcm_esd_check enter %x\n",buffer[0]);
#endif
#ifndef BUILD_LK
        if(buffer[0] == 0x9C)
        {
          #ifndef BUILD_LK
          printk("RM68190 lcm_esd_check false \n");
          #endif

            return false;
        }
        else
        {      
           #ifndef BUILD_LK
          printk("RM68190 lcm_esd_check true \n");
          #endif
           //lcm_esd_recover();
            return true;
        }
#endif
}

LCM_DRIVER rm68191_dsi_vdo_lcm_drv =
{
	.name = "rm68191_dsi_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
	//.set_backlight = lcm_setbacklight,
	//.esd_check = lcm_esd_check,
	//.esd_recover = lcm_esd_recover,
	.update = lcm_update,
#endif //wqtao
};




