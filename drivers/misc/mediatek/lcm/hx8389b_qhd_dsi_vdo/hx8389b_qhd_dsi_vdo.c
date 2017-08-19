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
    unsigned char para_list[64*4];
};

static unsigned int vcomadj=0x7f;

static void init_lcm_registers(void)
	{
	unsigned int data_array[40];

#if 0
    //HX8389B_HSD5.0IPS_APEX_131011
    data_array[0]=0x00043902;//Enable external Command
    data_array[1]=0x8983FFB9; 
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);//3000
    
    data_array[0]=0x00063902;
    data_array[1]=0x009341ba; 
    data_array[2]=0x1810A416; 
    dsi_set_cmdq(&data_array, 3, 1);
    MDELAY(1);//3000    
    
    data_array[0]=0x00143902;
    data_array[1]=0x060000B1;
    data_array[2]=0x111059E8;
    data_array[3]=0x453DF1D1;  
    data_array[4]=0x01422E2E; 
    data_array[5]=0xE600F758;  
    dsi_set_cmdq(&data_array, 6, 1);


    data_array[0]=0x00083902;
    data_array[1]=0x780000b2; //  
    data_array[2]=0x303F070C;
    dsi_set_cmdq(&data_array, 3, 1); 
    MDELAY(1); 


    data_array[0]=0x00183902;
    data_array[1]=0x000882b4;   
    data_array[2]=0x32041032; 
    data_array[3]=0x10320010; 
    data_array[4]=0x400A3700; 
    data_array[5]=0x480C3708;    //4002
    data_array[6]=0x0A585014;  
    dsi_set_cmdq(&data_array, 7, 1);
    MDELAY(1);
   
    data_array[0]=0x003D3902;//Enable external Command//3
    data_array[1]=0x000000d5; 
    data_array[2]=0x00000100; 
    data_array[3]=0x99006000; 
    data_array[4]=0x88BBAA88; 
    data_array[5]=0x88018823; 
    data_array[6]=0x01458867; 
    data_array[7]=0x88888823; 
    data_array[8]=0x99888888; 
    data_array[9]=0x5488AABB; 
    data_array[10]=0x10887688; 
    data_array[11]=0x10323288; 
    data_array[12]=0x88888888; 
	data_array[13]=0x00040088; 
    data_array[14]=0x00000000; 
    data_array[15]=0x00000000; 	
    dsi_set_cmdq(&data_array, 16, 1);
    MDELAY(1);//3000

    data_array[0]=0x00053902;//Enable external Command
    data_array[1]=0x007A00b6;   //0x6a
    data_array[2]=0x0000007A; 
    dsi_set_cmdq(&data_array, 3, 1);
    MDELAY(1);//3000 
    
    data_array[0]=0x00243902;
    data_array[1]=0x252300e0; 
    data_array[2]=0x2D3F3E3C; 
    data_array[3]=0x0E0E0747; 
    data_array[4]=0x110F1311; 
    data_array[5]=0x23001911; 
    data_array[6]=0x3F3E3C25; 
    data_array[7]=0x0E07472D; 
    data_array[8]=0x0F13110E; 
    data_array[9]=0x00191111; 
    dsi_set_cmdq(&data_array, 10, 1);
    MDELAY(1);//3000
    
    data_array[0]=0x00023902;
    data_array[1]=0x000002cc;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);
        
    data_array[0] = 0x00110500; 
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(200);

	data_array[0]=0x00023902;
    data_array[1]=0x0000E0C6;
    dsi_set_cmdq(&data_array, 2, 1);
    MDELAY(1);
    
    data_array[0] = 0x00290500;
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(120); 
    
#else	// 4041????5.0QHD?那?HX8389B?那????“∟2?那“⊿?足?那????∫?“a??那?芍“|
    data_array[0]=0x00043902;	//PacketHeader[04],// Set EXTC,
	data_array[1]=0x8983FFB9;	//Payload[B9 FF 83 89],
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set MIPI
	data_array[1]=0x009341BA;	//Payload[BA 41 93 00 
	data_array[2]=0x1800A416;	//16 A4 00 18],
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00143902;	//PacketHeader[14] // Set Power,
	data_array[1]=0x070000B1;	//Payload[B1 00 00 07 
	data_array[2]=0x111059F6;	//F6 50 10 11
	data_array[3]=0x2920F7F8;	// F8 F7 36 3E   2c24f7f8 
	data_array[4]=0x01423F3F;	// 3F 3F 42 01
	data_array[5]=0xE600F738;	// 3A F7 00 E6]
	dsi_set_cmdq(&data_array, 6, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set Display
	data_array[1]=0x780000B2;	//Payload[B2 00 00 78 
	data_array[2]=0xC03F120d;	//0E 12 3F C0]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00203902;	//;PacketHeader[20] // Set CYC
	data_array[1]=0x001c80B4;	//Payload[B4 80 14 00 
	data_array[2]=0x54051032;	//32 10 05 43 
	data_array[3]=0x1032D313;	//13 D3 32 10
	data_array[4]=0x36354700;	//00 47 43 44 
	data_array[5]=0x36354707;	//07 47 43 44 
	data_array[6]=0x0b40400b;	//14 FF FF 0A 
	data_array[7]=0x40004000;	//00 40 00 40 
	data_array[8]=0x0A504614;	//14 46 50 0A]
	dsi_set_cmdq(&data_array, 9, 1);
	MDELAY(1);

    data_array[0]=0x00033902;	//PacketHeader[04],// Set EXTC,
	data_array[1]=0xFF1010C7;	//Payload[B9 FF 83 89],
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00333902;	//PacketHeader[33] // Set GIP,
	data_array[1]=0x000000D5;	//Payload[D5 00 00 00 
	data_array[2]=0x00000100;	//00 01 00 00
	data_array[3]=0x88083C00;	//00 3C 08 88 
	data_array[4]=0x23010188;	//88 01 01 23
	data_array[5]=0xBBAA6745;	//45 67 AA BB 
	data_array[6]=0x67888845;	//45 88 88 67 
	data_array[7]=0x88888888;	//88 88 88 88 
	data_array[8]=0x88888888;	//88 88 88 88 
	data_array[9]=0x76103254;	//54 32 10 76 
	data_array[10]=0x10BBAA54;	//54 AA BB 10 
	data_array[11]=0x88768888;	//88 88 76 88 
	data_array[12]=0x88888888;	//88 88 88 88 
	data_array[13]=0xff030088;	//88 3C 03]
	dsi_set_cmdq(&data_array, 14, 1);
	MDELAY(1);

	data_array[0]=0x00233902;	//PacketHeader[23] // Set GAMMA
	data_array[1]=0x110e04E0;	//Payload[E0 00 14 1B 
	data_array[2]=0x193F2e2b;	//33 3A 3F 27 
	data_array[3]=0x0e0c0537;	//3F 07 0D 0F 
	data_array[4]=0x13121412;	//12 14 13 14 
	data_array[5]=0x0E041A12;	//10 17 00 13 
	data_array[6]=0x3F2E2B11;	//1A 33 39 3F 
	data_array[7]=0x0C053719;	//28 3F 08 0D 
	data_array[8]=0x1214120E;	//0D 12 14 12
	data_array[9]=0xFF1A1213;	//14 10 17]
	dsi_set_cmdq(&data_array, 10, 1);
	MDELAY(1);

	data_array[0]=0x00803902;	//PacketHeader[80] // Set DGC
	data_array[1]=0x080001C1;	//Payload[C1 01 00 08 
	data_array[2]=0x2A211A11;	//11 1A 21 2A 
	data_array[3]=0x4A423931;	//31 39 42 4A 
	data_array[4]=0x68605952;	//52 59 60 68 
	data_array[5]=0x88807970;	//70 79 80 88 
	data_array[6]=0xA69F978B;	//8F 97 9F A6
	data_array[7]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[8]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[9]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[10]=0x8DEC0298;	//98 02 EC 8D 
	data_array[11]=0x806B853D;	//3D 85 6B 80 
	data_array[12]=0x1A110800;	//00 08 11 1A 
	data_array[13]=0x39312A21;	//21 2A 31 39
	data_array[14]=0x59524A42;	//42 4A 52 59 
	data_array[15]=0x79706860;	//60 68 70 79 
	data_array[16]=0x978F8880;	//80 88 8F 97 
	data_array[17]=0xB7AEA69F;	//9F A6 AE B7 
	data_array[18]=0xD7CFC9C0;	//C0 C9 CF D7 
	data_array[19]=0xF7EFE6E0;	//E0 E6 EF F7 
	data_array[20]=0x029802FF;	//FF 02 98 02 
	data_array[21]=0x853D8DEC;	//EC 8D 3D 85 
	data_array[22]=0x0800806B;	//6B 80 00 08 
	data_array[23]=0x2A211A11;	//11 1A 21 2A 
	data_array[24]=0x4A423931;	//31 39 42 4A 
	data_array[25]=0x68605952;	//52 59 60 68 
	data_array[26]=0x88807970;	//70 79 80 88 
	data_array[27]=0xA69F978F;	//8F 97 9F A6 
	data_array[28]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[29]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[30]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[31]=0x8DEC0298;	//98 02 EC 8D 
	data_array[32]=0x806B853D;	//3D 85 6B 80]
//	dsi_set_cmdq(&data_array, 33, 1);
	MDELAY(1);

	data_array[0]=0x00023902;	//PacketHeader[02] // Set GIP forward scan
	data_array[1]=0x000002CC;	//Payload[CC 02]   //06 Set GIP backward scan
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);
	
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x009100B6;	//Payload[B6 00 91 00 
	data_array[2]=0x00000091;	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);
/*
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x000000B6|((vcomadj<<16)&0xff0000);	//Payload[B6 00 91 00 
	data_array[2]=0xffffff7F|(vcomadj&0xff);	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);
	vcomadj-=2;
*/
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x125805DE;	//Payload[B6 00 91 00 
	data_array[2]=0xffffff02;	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);


	data_array[0] = 0x00110500;	//PacketHeader[11] // Sleep Out
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;	//PacketHeader[29] // Display On
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);


/*
	data_array[0]=0x00043902;	//PacketHeader[04],// Set EXTC,
	data_array[1]=0x8983FFB9;	//Payload[B9 FF 83 89],
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set MIPI
	data_array[1]=0x009341BA;	//Payload[BA 41 93 00 
	data_array[2]=0x1800A416;	//16 A4 00 18],
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00143902;	//PacketHeader[14] // Set Power,
	data_array[1]=0x070000B1;	//Payload[B1 00 00 07 
	data_array[2]=0x111050F6;	//F6 50 10 11
	data_array[3]=0x3E36F7F8;	// F8 F7 36 3E 
	data_array[4]=0x01423F3F;	// 3F 3F 42 01
	data_array[5]=0xE600F73A;	// 3A F7 00 E6]
	dsi_set_cmdq(&data_array, 6, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set Display
	data_array[1]=0x780000B2;	//Payload[B2 00 00 78 
	data_array[2]=0xC03F120E;	//0E 12 3F C0]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00203902;	//;PacketHeader[20] // Set CYC
	data_array[1]=0x001480B4;	//Payload[B4 80 14 00 
	data_array[2]=0x43051032;	//32 10 05 43 
	data_array[3]=0x1032D313;	//13 D3 32 10
	data_array[4]=0x44434700;	//00 47 43 44 
	data_array[5]=0x44434707;	//07 47 43 44 
	data_array[6]=0x0AFFFF14;	//14 FF FF 0A 
	data_array[7]=0x40004000;	//00 40 00 40 
	data_array[8]=0x0A504614;	//14 46 50 0A]
	dsi_set_cmdq(&data_array, 9, 1);
	MDELAY(1);

	data_array[0]=0x00333902;	//PacketHeader[33] // Set GIP,
	data_array[1]=0x000000D5;	//Payload[D5 00 00 00 
	data_array[2]=0x00000100;	//00 01 00 00
	data_array[3]=0x88083C00;	//00 3C 08 88 
	data_array[4]=0x23010188;	//88 01 01 23
	data_array[5]=0xBBAA6745;	//45 67 AA BB 
	data_array[6]=0x67888845;	//45 88 88 67 
	data_array[7]=0x88888888;	//88 88 88 88 
	data_array[8]=0x88888888;	//88 88 88 88 
	data_array[9]=0x76103254;	//54 32 10 76 
	data_array[10]=0x10BBAA54;	//54 AA BB 10 
	data_array[11]=0x88768888;	//88 88 76 88 
	data_array[12]=0x88888888;	//88 88 88 88 
	data_array[13]=0x00033C88;	//88 3C 03]
	dsi_set_cmdq(&data_array, 14, 1);
	MDELAY(1);

	data_array[0]=0x00233902;	//PacketHeader[23] // Set GAMMA
	data_array[1]=0x1B1400E0;	//Payload[E0 00 14 1B 
	data_array[2]=0x273F3A33;	//33 3A 3F 27 
	data_array[3]=0x0F0D073F;	//3F 07 0D 0F 
	data_array[4]=0x14131412;	//12 14 13 14 
	data_array[5]=0x13001710;	//10 17 00 13 
	data_array[6]=0x3F39331A;	//1A 33 39 3F 
	data_array[7]=0x0D083F28;	//28 3F 08 0D 
	data_array[8]=0x1214120D;	//0D 12 14 12
	data_array[9]=0x00171014;	//14 10 17]
	dsi_set_cmdq(&data_array, 10, 1);
	MDELAY(1);

	data_array[0]=0x00803902;	//PacketHeader[80] // Set DGC
	data_array[1]=0x080001C1;	//Payload[C1 01 00 08 
	data_array[2]=0x2A211A11;	//11 1A 21 2A 
	data_array[3]=0x4A423931;	//31 39 42 4A 
	data_array[4]=0x68605952;	//52 59 60 68 
	data_array[5]=0x88807970;	//70 79 80 88 
	data_array[6]=0xA69F978B;	//8F 97 9F A6
	data_array[7]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[8]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[9]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[10]=0x8DEC0298;	//98 02 EC 8D 
	data_array[11]=0x806B853D;	//3D 85 6B 80 
	data_array[12]=0x1A110800;	//00 08 11 1A 
	data_array[13]=0x39312A21;	//21 2A 31 39
	data_array[14]=0x59524A42;	//42 4A 52 59 
	data_array[15]=0x79706860;	//60 68 70 79 
	data_array[16]=0x978F8880;	//80 88 8F 97 
	data_array[17]=0xB7AEA69F;	//9F A6 AE B7 
	data_array[18]=0xD7CFC9C0;	//C0 C9 CF D7 
	data_array[19]=0xF7EFE6E0;	//E0 E6 EF F7 
	data_array[20]=0x029802FF;	//FF 02 98 02 
	data_array[21]=0x853D8DEC;	//EC 8D 3D 85 
	data_array[22]=0x0800806B;	//6B 80 00 08 
	data_array[23]=0x2A211A11;	//11 1A 21 2A 
	data_array[24]=0x4A423931;	//31 39 42 4A 
	data_array[25]=0x68605952;	//52 59 60 68 
	data_array[26]=0x88807970;	//70 79 80 88 
	data_array[27]=0xA69F978F;	//8F 97 9F A6 
	data_array[28]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[29]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[30]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[31]=0x8DEC0298;	//98 02 EC 8D 
	data_array[32]=0x806B853D;	//3D 85 6B 80]
//	dsi_set_cmdq(&data_array, 33, 1);
	MDELAY(1);

	data_array[0]=0x00023902;	//PacketHeader[02] // Set GIP forward scan
	data_array[1]=0x000002CC;	//Payload[CC 02]   //06 Set GIP backward scan
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);
	
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x009100B6;	//Payload[B6 00 91 00 
	data_array[2]=0x00000091;	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0] = 0x00110500;	//PacketHeader[11] // Sleep Out
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;	//PacketHeader[29] // Display On
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);
	*/
    
#endif 

};

static void init_lcm_registers_new(void)
	{
	unsigned int data_array[40];

	// 4041????5.0QHD?那?HX8389B?那????“∟2?那“⊿?足?那????∫?“a??那?芍“|
    data_array[0]=0x00043902;	//PacketHeader[04],// Set EXTC,
	data_array[1]=0x8983FFB9;	//Payload[B9 FF 83 89],
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set MIPI
	data_array[1]=0x009341BA;	//Payload[BA 41 93 00 
	data_array[2]=0x1800A416;	//16 A4 00 18],
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00143902;	//PacketHeader[14] // Set Power,
	data_array[1]=0x070000B1;	//Payload[B1 00 00 07 
	data_array[2]=0x111059F6;	//F6 50 10 11
	data_array[3]=0x2920F7F8;	// F8 F7 36 3E   2c24f7f8 
	data_array[4]=0x01423F3F;	// 3F 3F 42 01
	data_array[5]=0xE600F738;	// 3A F7 00 E6]
	dsi_set_cmdq(&data_array, 6, 1);
	MDELAY(1);

	data_array[0]=0x00083902;	//PacketHeader[08] // Set Display
	data_array[1]=0x780000B2;	//Payload[B2 00 00 78 
	data_array[2]=0xC03F120d;	//0E 12 3F C0]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);

	data_array[0]=0x00203902;	//;PacketHeader[20] // Set CYC
	data_array[1]=0x001c80B4;	//Payload[B4 80 14 00 
	data_array[2]=0x54051032;	//32 10 05 43 
	data_array[3]=0x1032D313;	//13 D3 32 10
	data_array[4]=0x36354700;	//00 47 43 44 
	data_array[5]=0x36354707;	//07 47 43 44 
	data_array[6]=0x0b40400b;	//14 FF FF 0A 
	data_array[7]=0x40004000;	//00 40 00 40 
	data_array[8]=0x0A504614;	//14 46 50 0A]
	dsi_set_cmdq(&data_array, 9, 1);
	MDELAY(1);

    data_array[0]=0x00033902;	//PacketHeader[04],// Set EXTC,
	data_array[1]=0xFF1010C7;	//Payload[B9 FF 83 89],
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);

	data_array[0]=0x00333902;	//PacketHeader[33] // Set GIP,
	data_array[1]=0x000000D5;	//Payload[D5 00 00 00 
	data_array[2]=0x00000100;	//00 01 00 00
	data_array[3]=0x88083C00;	//00 3C 08 88 
	data_array[4]=0x23010188;	//88 01 01 23
	data_array[5]=0xBBAA6745;	//45 67 AA BB 
	data_array[6]=0x67888845;	//45 88 88 67 
	data_array[7]=0x88888888;	//88 88 88 88 
	data_array[8]=0x88888888;	//88 88 88 88 
	data_array[9]=0x76103254;	//54 32 10 76 
	data_array[10]=0x10BBAA54;	//54 AA BB 10 
	data_array[11]=0x88768888;	//88 88 76 88 
	data_array[12]=0x88888888;	//88 88 88 88 
	data_array[13]=0xff030088;	//88 3C 03]
	dsi_set_cmdq(&data_array, 14, 1);
	MDELAY(1);

	data_array[0]=0x00233902;	//PacketHeader[23] // Set GAMMA
	data_array[1]=0x110e04E0;	//Payload[E0 00 14 1B 
	data_array[2]=0x193F2e2b;	//33 3A 3F 27 
	data_array[3]=0x0e0c0537;	//3F 07 0D 0F 
	data_array[4]=0x13121412;	//12 14 13 14 
	data_array[5]=0x0E041A12;	//10 17 00 13 
	data_array[6]=0x3F2E2B11;	//1A 33 39 3F 
	data_array[7]=0x0C053719;	//28 3F 08 0D 
	data_array[8]=0x1214120E;	//0D 12 14 12
	data_array[9]=0xFF1A1213;	//14 10 17]
	dsi_set_cmdq(&data_array, 10, 1);
	MDELAY(1);

	data_array[0]=0x00803902;	//PacketHeader[80] // Set DGC
	data_array[1]=0x080001C1;	//Payload[C1 01 00 08 
	data_array[2]=0x2A211A11;	//11 1A 21 2A 
	data_array[3]=0x4A423931;	//31 39 42 4A 
	data_array[4]=0x68605952;	//52 59 60 68 
	data_array[5]=0x88807970;	//70 79 80 88 
	data_array[6]=0xA69F978B;	//8F 97 9F A6
	data_array[7]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[8]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[9]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[10]=0x8DEC0298;	//98 02 EC 8D 
	data_array[11]=0x806B853D;	//3D 85 6B 80 
	data_array[12]=0x1A110800;	//00 08 11 1A 
	data_array[13]=0x39312A21;	//21 2A 31 39
	data_array[14]=0x59524A42;	//42 4A 52 59 
	data_array[15]=0x79706860;	//60 68 70 79 
	data_array[16]=0x978F8880;	//80 88 8F 97 
	data_array[17]=0xB7AEA69F;	//9F A6 AE B7 
	data_array[18]=0xD7CFC9C0;	//C0 C9 CF D7 
	data_array[19]=0xF7EFE6E0;	//E0 E6 EF F7 
	data_array[20]=0x029802FF;	//FF 02 98 02 
	data_array[21]=0x853D8DEC;	//EC 8D 3D 85 
	data_array[22]=0x0800806B;	//6B 80 00 08 
	data_array[23]=0x2A211A11;	//11 1A 21 2A 
	data_array[24]=0x4A423931;	//31 39 42 4A 
	data_array[25]=0x68605952;	//52 59 60 68 
	data_array[26]=0x88807970;	//70 79 80 88 
	data_array[27]=0xA69F978F;	//8F 97 9F A6 
	data_array[28]=0xC9C0B7AE;	//AE B7 C0 C9 
	data_array[29]=0xE6E0D7CF;	//CF D7 E0 E6 
	data_array[30]=0x02FFF7EF;	//EF F7 FF 02 
	data_array[31]=0x8DEC0298;	//98 02 EC 8D 
	data_array[32]=0x806B853D;	//3D 85 6B 80]
//	dsi_set_cmdq(&data_array, 33, 1);
	MDELAY(1);

	data_array[0]=0x00023902;	//PacketHeader[02] // Set GIP forward scan
	data_array[1]=0x000002CC;	//Payload[CC 02]   //06 Set GIP backward scan
	dsi_set_cmdq(&data_array, 2, 1);
	MDELAY(1);
	
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x009100B6;	//Payload[B6 00 91 00 
	data_array[2]=0x00000091;	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);
/*
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x000000B6|((vcomadj<<16)&0xff0000);	//Payload[B6 00 91 00 
	data_array[2]=0xffffff7F|(vcomadj&0xff);	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);
	vcomadj-=2;
*/
	data_array[0]=0x00053902;	//PacketHeader[05] // Set VCOM
	data_array[1]=0x125805DE;	//Payload[B6 00 91 00 
	data_array[2]=0xffffff02;	//91]
	dsi_set_cmdq(&data_array, 3, 1);
	MDELAY(1);


	data_array[0] = 0x00110500;	//PacketHeader[11] // Sleep Out
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00290500;	//PacketHeader[29] // Display On
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(10);

};




static struct LCM_setting_table lcm_set_window[] = {
	{0x2A,	4,	{0x00, 0x00, (FRAME_WIDTH>>8), (FRAME_WIDTH&0xFF)}},
	{0x2B,	4,	{0x00, 0x00, (FRAME_HEIGHT>>8), (FRAME_HEIGHT&0xFF)}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 1, {0x00}},
    {REGFLAG_DELAY, 100, {}},

    // Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},

    // Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
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
		unsigned int id=0;
		unsigned char buffer[2];
		unsigned int array[16];  
	
		SET_RESET_PIN(1);
			MDELAY(10);
		SET_RESET_PIN(0);
		MDELAY(10);
		SET_RESET_PIN(1);
		MDELAY(120);//Must over 6 ms
	
		array[0]=0x00043902;
		array[1]=0x8983FFB9;// page enable
		dsi_set_cmdq(&array, 2, 1);
		MDELAY(10);
		array[0]=0x00083902;//Enable external Command//3
		array[1]=0x009341ba; //0x008341ba
		array[2]=0x1800a416; 
		dsi_set_cmdq(&array, 3, 1);
		MDELAY(10);

		array[0] = 0x00023700;// return byte number
		dsi_set_cmdq(&array, 1, 1);
		MDELAY(10);
	
		read_reg_v2(0xF4, buffer, 2);
		id = buffer[0]; 
		
#if defined(BUILD_UBOOT)
		printf(HX8389B "%s, id = 0x%08x\n", __func__, id);
#endif
    #ifdef BUILD_LK
    printf("HX8389B YYYYYYYYlcd id %x\n",id);   
	#else
    printk("HX8389B YYYYYYYYlcd id %x\n",id);   
    #endif
	
		return (0x89 == id)?1:0;
	
	}



static void lcm_get_params(LCM_PARAMS *params)
{
#if 0
		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif
	
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
		params->dsi.intermediat_buffer_num = 2;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 4;
		params->dsi.vertical_backporch					= 8;
		params->dsi.vertical_frontporch					= 8;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 6;
		params->dsi.horizontal_backporch				= 37;
		params->dsi.horizontal_frontporch				= 37;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
		params->dsi.pll_div1=25;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)

		/* ESD or noise interference recovery For video mode LCM only. */ // Send TE packet to LCM in a period of n frames and check the response. 
		params->dsi.lcm_int_te_monitor = FALSE; 
		params->dsi.lcm_int_te_period = 1; // Unit : frames 
 
		// Need longer FP for more opportunity to do int. TE monitor applicably. 
		if(params->dsi.lcm_int_te_monitor) 
			params->dsi.vertical_frontporch *= 2; 
 
		// Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.) 
		params->dsi.lcm_ext_te_monitor = FALSE; 
		// Non-continuous clock 
		params->dsi.noncont_clock = TRUE; 
		params->dsi.noncont_clock_period = 2; // Unit : frames
#else
    memset(params, 0, sizeof(LCM_PARAMS));
    
    params->type   = LCM_TYPE_DSI;
    
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;
    
    // enable tearing-free
    params->dbi.te_mode				= LCM_DBI_TE_MODE_DISABLED;
    params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
    
    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
    
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
    params->dsi.packet_size=256;
   // params->dsi.word_count=480*3;	//DSI CMD mode need set these two bellow params, different to 6577
   // params->dsi.vertical_active_line=800;

    params->dsi.vertical_sync_active				= 2;//4
    params->dsi.vertical_backporch					= 14;//7
    params->dsi.vertical_frontporch					= 20;//7
    params->dsi.vertical_active_line				= FRAME_HEIGHT;
    
    params->dsi.horizontal_sync_active				= 48;
    params->dsi.horizontal_backporch				= 48;
    params->dsi.horizontal_frontporch				= 48;
  //  params->dsi.horizontal_blanking_pixel				= 0;
    params->dsi.horizontal_active_pixel				= FRAME_WIDTH;
    
    // Bit rate calculation
    // Bit rate calculation
    //	params->dsi.pll_div1=26;//26;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
    //	params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
		
    //params->dsi.pll_div1=26;//26;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
    //params->dsi.pll_div2=1; 		        // div2=0~15: fout=fvo/(2*div2)
		
    //params->dsi.pll_div1=0;		
    //params->dsi.pll_div2=1;		
    //params->dsi.fbk_div =18;	// 18=54fps  
	params->dsi.PLL_CLOCK=230;
/* ESD or noise interference recovery For video mode LCM only. */ // Send TE packet to LCM in a period of n frames and check the response. 
		params->dsi.lcm_int_te_monitor = FALSE; 
		params->dsi.lcm_int_te_period = 1; // Unit : frames 
 
		// Need longer FP for more opportunity to do int. TE monitor applicably. 
		if(params->dsi.lcm_int_te_monitor) 
			params->dsi.vertical_frontporch *= 2; 
 
		// Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.) 
		params->dsi.lcm_ext_te_monitor = FALSE; 
		// Non-continuous clock 
		params->dsi.noncont_clock = TRUE; 
		params->dsi.noncont_clock_period = 2; // Unit : frames
		
#endif
		
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
     lcm_check_id_by_adc = 2; //?“?|足??那?????0V,???∫?“a?那? 8389B-L ,
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
    MDELAY(20);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);
	
	get_adc_vol();
	if(lcm_check_id_by_adc == 2) //???∫?“a?那?new,???“∟
    	init_lcm_registers_new();
	else
		init_lcm_registers();   //???∫?“a?那?old
}


static void lcm_suspend(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(20);
//	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
    lcm_init();
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




LCM_DRIVER hx8389b_qhd_dsi_vdo_drv = 
{
    .name			= "hx8389b_qhd_dsi_vdo_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.set_backlight	= lcm_setbacklight,
    .update         = lcm_update,
#endif
};

