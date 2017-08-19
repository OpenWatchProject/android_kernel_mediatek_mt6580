/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/


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

#if defined(BUILD_LK)
#else

#include <linux/proc_fs.h>   //proc file use 
#endif


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  										(540)
#define FRAME_HEIGHT 										(960)

#define REGFLAG_DELAY             							0XFE
#define REGFLAG_END_OF_TABLE      							0xFFF   // END OF REGISTERS MARKER

#define LCM_ID_OTM8018B	0x8009

#define LCM_DSI_CMD_MODE									0

#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif
static char lcm_check_id_by_adc = 0xFF;
extern  int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
static void get_adc_vol(void);

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

static int lcd_id_test  = 0;  


#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 											(lcm_util.udelay(n))
#define MDELAY(n) 											(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg											lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

 struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting_ykl[] = {

		
		{0x00,1,{0x00}},
		{0xFF,3,{0x96,0x05,0x01}},
		
		{0x00,1,{0x80}},
		{0xFF,2,{0x96,0x05}},
		
		
		
		{0x00,1,{0x83}},
		{0xB2,1,{0x80}},
		
		
		
		//{0x00,1,{0xA2}}, //0xC0A2 PL
		//{0xC0,3,{0x08,0x0D,0x09}},
		
		{0x00,1,{0xB4}},
		{0xC0,1,{0x50}},//50
		
		{0x00,1,{0x00}},
		{0xD9,1,{0x52}},//66
		{0x00,1,{0x00}},
		{0xD8,2,{0x67,0x67}},
		
		
		//{0x00,1,{0xA6}},
		//{0xC1,3,{0x01,0x00,0x01}},
		
		
		
		{0x00,1,{0x80}},
		{0xC4,1,{0x9C}},
		{REGFLAG_DELAY,10,{}},
		
		{0x00,1,{0x87}},//add
		{0xC4,1,{0x40}},
		{REGFLAG_DELAY,10,{}},
		
		{0x00,1,{0xB1}},
		{0xC5,1,{0x28}}, // vdd18=1.9V
		
		{0x00,1,{0xC0}},
		{0xC5,1,{0x00}},
		
		{0x00,1,{0xB2}},
		{0xF5,4,{0x15,0x00,0x15,0x00}},
		
		{0x00,1,{0x88}},
		{0xC4,1,{0x01}},
		
		{0x00,1,{0x90}},
		{0xC5,7,{0x96,0xB7,0x01,0x03,0x55,0x55,0x34}},
		
		{0x00,1,{0x80}},
		{0xC1,2,{0x36,0x77}},
		
		
		{0x00,1,{0x89}},
		{0xC0,1,{0x01}},
		
		{0x00,1,{0xA0}},
		{0xC1,1,{0x00}},
		
		{0x00,1,{0xC5}},
		{0xB0,1,{0x03}},
		
		{0x00,1,{0xD2}},//add
		{0xB0,1,{0x04}},
		
		{0x00,1,{0x80}},
		{0xCE,12,{0x8B,0x03,0x00,0x8A,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0x90}},
		{0xCE,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xA0}},
		{0xCE,14,{0x38,0x09,0x03,0xC2,0x00,0x17,0x00,0x38,0x08,0x03,0xC3,0x00,0x17,0x00}},
		{0x00,1,{0xB0}},
		{0xCE,14,{0x38,0x07,0x03,0xC4,0x00,0x17,0x00,0x38,0x06,0x03,0xCA,0x00,0x17,0x00}},
		{0x00,1,{0xC0}},
		{0xCE,14,{0x38,0x05,0x03,0xC6,0x00,0x17,0x00,0x38,0x04,0x03,0xC7,0x00,0x17,0x00}},
		{0x00,1,{0xD0}},
		{0xCE,14,{0x38,0x03,0x03,0xC8,0x00,0x17,0x00,0x38,0x02,0x03,0xC9,0x00,0x17,0x00}},
		
		{0x00,1,{0x80}},
		{0xCC,10,{0x00,0x00,0x00,0x02,0x00,0x0A,0x0C,0x0E,0x10,0x00}},
		{0x00,1,{0x90}},
		{0xCC,15,{0x21,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00}},
		{0x00,1,{0xA0}},
		{0xCC,15,{0x09,0x0B,0x0D,0x0F,0x00,0x21,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xB0}},
		{0xCC,10,{0x00,0x00,0x00,0x00,0x01,0x0B,0x09,0x0f,0x0d,0x00}},
		{0x00,1,{0xC0}},
		{0xCC,15,{0x21,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02}},
		{0x00,1,{0xD0}},
		{0xCC,15,{0x0c,0x0a,0x10,0x0e,0x00,0x21,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		
		{0x00,1,{0x90}},
		{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xA0}},
		{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xB0}},
		{0xCB,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xC0}},
		{0xCB,15,{0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00,0x04,0x00,0x00}},
		{0x00,1,{0xD0}},
		{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x00}},
		{0x00,1,{0xE0}},
		{0xCB,10,{0x04,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0x00,1,{0xF0}},
		{0xCB,10,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},
		
		{0x00,1,{0xC0}},
		{0xCF,10,{0x09,0x02,0x4a,0x4a,0x00,0x00,0x00,0x00,0x20,0x02}},
		{0x00,1,{0xD0}},
		{0xCF,2,{0x02,0x89}},  
		
		//{0x00,1,{0xC9}},
		//{0xCF,1,{0x02}},	//GOA gnd period
		
{0x00,1,{0x00}},
{0xE1,16,{0x03,0x07,0x0A,0x0C,0x04,0x0C,0x0A,0x09,0x04,0x07,0x0F,0x08,0x0F,0x13,0x0B,0x00}},
{0x00,1,{0x00}},
{0xE2,16,{0x03,0x07,0x0A,0x0C,0x04,0x0C,0x0A,0x09,0x04,0x07,0x0F,0x08,0x0F,0x13,0x0B,0x00}},

		{0x00,1,{0x00}},
		{0xFF,3,{0xFF,0xFF,0xFF}},
		
		 {REGFLAG_DELAY,10,{}},
		{0x11,1,{0x00}},//SLEEP OUT
		{REGFLAG_DELAY,120,{}},
	
		{0x29,1,{0x00}},//Display ON
		{REGFLAG_DELAY,20,{}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}
		
		
};


static struct LCM_setting_table lcm_initialization_setting[] = {

#if 1
 {0x00,1,{0x00}},
    {0xFF,3,{0x96,0x05,0x01}},
 

    {0x00,1,{0x80}},
    {0xFF,2,{0x96,0x05}},


	{0x00,1,{0x92}}, // mipi 2 lane
	{0xFF,2,{0x10,0x02}},


	{0x00,1,{0xB4}},	
    {0xC0,1,{0x50}},//inversion  50


    {0x00,1,{0x80}},
    {0xC1,2,{0x36,0x66}}, //70Hz


    {0x00,1,{0x89}},
    {0xC0,1,{0x01}},// TCON OSC turbo mode


 {0x00,1,{0xA0}},
  {0xC1,1,{0x00}},//02


	{0x00,1,{0x00}},
	 {0xa0,1,{0x00}},//02


/*

	// DC voltage for LGD 4.5"
	{0x00,1,{0x80}},
	{0xC5,4,{0x08,0x00,0xA0,0x11}},
	{REGFLAG_DELAY,20,{}},



	{0x00,1,{0xB0}},
	{0xC5,2,{0x05,0xac}}, //05 28
		
	//Inrush Current Test
	{0x00,1,{0xA6}},
	{0xC1,1,{0x01}},



	{0x00,1,{0x80}},//************ 
	{0xC4,1,{0x9C}},

	{0x00,1,{0x87}},//**************** 
	{0xC4,1,{0x40}},

*/

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////		


	{0x00,1,{0x80}},	//Vcom setting
	{0xD6,1,{00}},//47  64  5a  56//
	
	{0x00,1,{0x00}},	//Vcom setting
	{0xD7,1,{00}},//47  64  5a  56
	
	{0x00,1,{0x00}},	//GVDD=5.2V/NGVDD=-5.2V
	{0xD8,2,{0x6f,0x6f}},//85  85
	
	{0x00,1,{0x00}},	//Vcom setting
	{0xD9,1,{0x32}},//47  64  5a  56//30     //32 //38 //3e

	{0x00,1,{0xA2}}, // pl_width, pch_dri_pch_nop
	{0xC0,3,{0x01,0x10,0x00}},//01
	
	//{0x00,1,{0xA5}}, 
	//{0xC0,3,{0x01,0x3c,0x08}},//01 10 00
	
	
	{0x00,1,{0x91}},
	{0xC5,1,{0x77}},	//VGH=14V, VGL=-11V
	
		{0x00,1,{0x94}},
	{0xC5,3,{0x55,0x55,0x55}},	

	//GOA mapping
	{0x00,1,{0x80}},	//GOA mapping
	{0xCB,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},															
	
	{0x00,1,{0x90}},	//GOA mapping
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},    
	
	{0x00,1,{0xA0}},																																					
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},    
	
	{0x00,1,{0xB0}},	
	{0xCB,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},															

	{0x00,1,{0xC0}},
	{0xCB,15,{0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},   

	{0x00,1,{0xD0}},	
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00}},    
	
	{0x00,1,{0xE0}}, 
	{0xCB,10,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xF0}}, 
	{0xCB,10,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},



	
	{0x00,1,{0x80}},
	{0xCC,10,{0x00,0x00,0x09,0x0B,0x01,0x25,0x26,0x00,0x00,0x00}},															

	{0x00,1,{0x90}},
	{0xCC,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x0C,0x02}},    

	{0x00,1,{0xA0}},
	{0xCC,15,{0x25,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    

	
	{0x00,1,{0x80}},//GOA VST Setting																																				
	{0xCE,12,{0x86,0x01,0x06,0x85,0x01,0x06,0x0F,0x00,0x00,0x0F,0x00,0x00}},   								
	
	{0x00,1,{0x90}}, //GOA VEND and Group Setting
	{0xCE,14,{0xF0,0x00,0x00,0xF0,0x00,0x00,0xF0,0x00,0x00,0xF0,0x00,0x00,0x00,0x00}},    			
	
	{0x00,1,{0xA0}},	//GOA CLK1 and GOA CLK2 Setting
	{0xCE,14,{0x18,0x05,0x03,0xC0,0x00,0x06,0x00,0x18,0x04,0x03,0xC1,0x00,0x06,0x00}},   			
	
	{0x00,1,{0xB0}},	//GOA CLK3 and GOA CLK4 Setting																																					//Address Shift
	{0xCE,14,{0x18,0x03,0x03,0xC2,0x00,0x06,0x00,0x18,0x02,0x03,0xC3,0x00,0x06,0x00}},    			
	
	{0x00,1,{0xC0}},	//GOA CLKB1 and GOA CLKB2 Setting																																				
	{0xCE,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},   			
	
	{0x00,1,{0xD0}},	//GOA CLKB3 and GOA CLKB4 Setting																																				
	{0xCE,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},    			

	{0x00,1,{0x80}},	//GOA CLKC1 and GOA CLKC2 Setting																																				
	{0xCF,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},   			
	
	{0x00,1,{0x90}},	//GOA CLKC3 and GOA CLKC4 Setting																																					
	{0xCF,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},    			
	
	{0x00,1,{0xA0}},	//GOA CLKD1 and GOA CLKD2 Setting
	{0xCF,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},    			
	
	{0x00,1,{0xB0}},	//GOA CLKD3 and GOA CLKD4 Setting
	{0xCF,14,{0xF0,0x00,0x00,0x10,0x00,0x00,0x00,0xF0,0x00,0x00,0x10,0x00,0x00,0x00}},    			
	
	{0x00,1,{0xC0}},	//GOA ECLK Setting and GOA Other Options1 and GOA Signal Toggle Option Setting																																				
	{0xCF,10,{0x02,0x02,0x10,0x10,0x00,0x00,0x01,0x81,0x00,0x08}},


	{0x00,1,{0x00}},																					
	{0xE1,8,{0x00,0x0F,0x15,0x0E,0x07,0x0E,0x0B,0x09,0x04,0x08,0x0F,0x0A,0x11,0x12,0x0A,0x03}},//G2.2 POS				
																			

	{0x00,1,{0x00}},
	{0xE2,8,{0x00,0x0F,0x15,0x0E,0x07,0x0E,0x0B,0x09,0x04,0x08,0x0F,0x0A,0x11,0x12,0x0A,0x03}},//G2.2 POS					


///////////////////////////////////////////////////////////////////////////////////////////

//{0x00,1,{0x80}}, 
//{0xC1,2,{0x36,0x66}},//initial osc 
/*
{0x00,1,{0xb1}}, 
{0xC5,1,{0x28}},//VDD18 

{0x00,1,{0xB2}},
{0xF5,4,{0x15,0x00,0x15,0x00}},//VRGH disable 

{0x00,1,{0xC0}},
{0xC5,1,{0x00}},//thermo disable 

//{0x00,1,{0x88}},//CR enable 
//{0xC4,1,{0x01}}, 

//{0x00,1,{0xA2}}, 
//{0xC0,3,{0x20,0x18,0x09}},//cr, pl, pcg 

{0x00,1,{0x80}},//Source output levels during porch and non-display area 
{0xC4,1,{0x9C}}, 
///////////////////////////////////////////////////////////////////////////////////////////

*/
{0x00,1,{0x00}}, 
{0xff,3,{0xff,0xff,0xff}},

 
{0x11,1,{0x00}},  
	{REGFLAG_DELAY,120,{}},

	
	{0x29,1,{0x00}},//Display ON 
	{REGFLAG_DELAY,120,{}},	
	{REGFLAG_END_OF_TABLE, 0x00, {}}



#endif
};


static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

    // Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
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
static unsigned int lcm_compare_id(void)
{
	int array[4];
	char buffer[5];
	char id_high=0;
	char id_low=0;
	int id=0;

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(200);

	array[0] = 0x00053700;	// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xa1, buffer, 5);
	id = ((buffer[2] << 8) | buffer[3]);	//we only need ID
#if defined(BUILD_LK)
		printf("otm9605a %s, id1 = 0x%x,id2 = 0x%x,id3 = 0x%x,id4 = 0x%x,id5 = 0x%x\n", __func__, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
#endif
	
	return (0x9605 == id) ? 1 : 0;


}



static void lcm_get_params(LCM_PARAMS *params)
	{
		
	        memset(params, 0, sizeof(LCM_PARAMS));
		
		params->type   = LCM_TYPE_DSI;
		
		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;
		
		// enable tearing-free
		params->dbi.te_mode 			= LCM_DBI_TE_MODE_DISABLED;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
		
		params->dsi.mode   = SYNC_EVENT_VDO_MODE;
		
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding 	= LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format	  = LCM_DSI_FORMAT_RGB888;
		
		// Video mode setting		
		params->dsi.intermediat_buffer_num = 2;
		
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		
		params->dsi.word_count=540*3;	//DSI CMD mode need set these two bellow params, different to 6577
		//params->dsi.vertical_active_line=854;
	
		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 16;
		params->dsi.vertical_frontporch 				= 15;
		params->dsi.vertical_active_line				= FRAME_HEIGHT;
		
		params->dsi.horizontal_sync_active				= 10;
		params->dsi.horizontal_backporch				= 32;//37  32
		params->dsi.horizontal_frontporch				= 32;//37  32
		//params->dsi.horizontal_blanking_pixel 			        = 60;
		params->dsi.horizontal_active_pixel 			= FRAME_WIDTH;
		
		// Bit rate calculation

		//	params->dsi.pll_div1=0; 	// div1=0,1,2,3;div1_real=1,2,4,4
		//	params->dsi.pll_div2=1; 	// div2=0,1,2,3;div2_real=1,2,4,4
			//params->dsi.fbk_sel=1;		 // fbk_sel=0,1,2,3;fbk_sel_real=1,2,4,4
		//	params->dsi.fbk_div =15;		// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	 32
		//		params->dsi.pll_select=1;	//0: MIPI_PLL; 1: LVDS_PLL
   		//	params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_429;//this value must be in MTK suggested table
			
		params->dsi.PLL_CLOCK=230;
		

		}



static void get_adc_vol(void)
{
  int tmpVol =0;
  int tempdata[4] ={77,77,77,77};   
  int res = 0;
  int vol = 0;

  res = IMM_GetOneChannelValue(1,tempdata,&tmpVol);
 MDELAY(2);

 // if(res != 0)
  	//return;
  #ifdef BUILD_LK
		printf("karlanz-->lk-->vol=%d ;tempdata[0-3]=%d %d %d %d \n",vol,tempdata[0],tempdata[1],tempdata[2],tempdata[3]);
  #else
		printk("karlanz-->ker-->vol=%d ;tempdata[0-3]=%d %d %d %d \n",vol,tempdata[0],tempdata[1],tempdata[2],tempdata[3]);
  #endif

  
  vol = tempdata[0]*1000+tempdata[1]*10; 

   if(vol > 1200)
     lcm_check_id_by_adc = 0; 
  else if(vol>800) 
     lcm_check_id_by_adc = 1; 
  else if(vol>350) 
     lcm_check_id_by_adc = 2; // 易快来，100K,33K ,0.45v 
  else if(vol>10) 	
  	 lcm_check_id_by_adc = 3; 
  else
     lcm_check_id_by_adc = 4; 
	
#ifndef BUILD_LK
	printk(KERN_ERR  "########get onchannel : vol = %d ,lcm_check_id_by_adc=%d \n",vol,lcm_check_id_by_adc);
#else
  printf("#######get onchannel : vol = %d ,lcm_check_id_by_adc=%d \n",vol,lcm_check_id_by_adc);
#endif

}



static void lcm_init(void)
{
    SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(50);
    SET_RESET_PIN(1);
    MDELAY(120);
    //lcm_compare_id();
    get_adc_vol();
	if(lcm_check_id_by_adc == 2)  //易快来，
	{
		
		push_table(lcm_initialization_setting_ykl, sizeof(lcm_initialization_setting_ykl) / sizeof(struct LCM_setting_table), 1);
	}
	else                          //德利泰
	{
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	//	push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
	}
}


static void lcm_suspend(void)
{
    SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
   // MDELAY(50);
   // SET_RESET_PIN(1);
   // MDELAY(120);
	//push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	//push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}



static void lcm_resume(void)
{
	//lcm_compare_id();
lcm_init();	
//SET_RESET_PIN(1);
//MDELAY(120);
//push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
//push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);
}

/*
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
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(data_array, 7, 0);

}


static void lcm_setbacklight(unsigned int level)
{
	unsigned int default_level = 145;
	unsigned int mapped_level = 0;

	//for LGE backlight IC mapping table
	if(level > 255) 
			level = 255;

	if(level >0) 
			mapped_level = default_level+(level)*(255-default_level)/(255);
	else
			mapped_level=0;

	// Refresh value of backlight level.
	lcm_backlight_level_setting[0].para_list[0] = mapped_level;

	push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
}

*/




LCM_DRIVER otm9605_dsi_vdo_lcm_drv = 
{
    .name			= "otm9605_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,	
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};

