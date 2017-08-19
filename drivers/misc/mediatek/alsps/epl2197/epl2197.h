#ifndef __ELAN_EPL2197__
#define __ELAN_EPL2197__

#define ELAN_IOCTL_MAGIC 'c'
#define ELAN_EPL8800_IOCTL_GET_PFLAG _IOR(ELAN_IOCTL_MAGIC, 1, int *)
#define ELAN_EPL8800_IOCTL_GET_LFLAG _IOR(ELAN_IOCTL_MAGIC, 2, int *)
#define ELAN_EPL8800_IOCTL_ENABLE_PFLAG _IOW(ELAN_IOCTL_MAGIC, 3, int *)
#define ELAN_EPL8800_IOCTL_ENABLE_LFLAG _IOW(ELAN_IOCTL_MAGIC, 4, int *)
#define ELAN_EPL8800_IOCTL_GETDATA _IOR(ELAN_IOCTL_MAGIC, 5, int *)

#define REG_0			0X00
#define REG_1			0X01
#define REG_2			0X02
#define REG_3			0X03
#define REG_4			0X04
#define REG_5			0X05
#define REG_6			0X06
#define REG_7			0X07
#define REG_8			0X08
#define REG_9			0X09
#define REG_10			0X0A
#define REG_11			0X0B
#define REG_12			0X0C
#define REG_13			0X0D
#define REG_14			0X0E
#define REG_15			0X0F
#define REG_16			0X10
#define REG_17			0X11
#define REG_18			0X12
#define REG_19			0X13  
#define REG_20			0X14
#define REG_21			0X15
#define REG_28			0x1C
#define REG_29			0x1D

#define W_SINGLE_BYTE	0X00
#define W_TWO_BYTE		0X01
#define W_THREE_BYTE	0X02
#define W_FOUR_BYTE		0X03
#define W_FIVE_BYTE		0X04
#define W_SIX_BYTE		0X05
#define W_SEVEN_BYTE	0X06
#define W_EIGHT_BYTE	0X07

#define R_SINGLE_BYTE	0X00
#define R_TWO_BYTE		0X01
#define R_THREE_BYTE	0X02
#define R_FOUR_BYTE		0X03
#define R_FIVE_BYTE		0X04
#define R_SIX_BYTE		0X05
#define R_SEVEN_BYTE	0X06
#define R_EIGHT_BYTE	0X07

const int INTEG_ARRAY[] = { 4, 6, 8, 10, 15, 20, 25, 30, 40, 55, 70, 90, 110, 150, 200, 250, 350, 450, 550, 
			 750, 950, 1250, 1600, 2000, 2700, 3500, 4500, 6000, 8000, 10000, 15000, 20000 };

#define EPL_INTEG_20		5
#define EPL_INTEG_25		6
#define EPL_INTEG_30		7
#define EPL_INTEG_40		8
#define EPL_INTEG_55		9
#define EPL_INTEG_70		10
#define EPL_INTEG_90		11
#define EPL_INTEG_110		12
#define EPL_INTEG_150	  	13
#define EPL_INTEG_200	  	14
#define EPL_INTEG_250	  	15
#define EPL_INTEG_350	  	16
#define EPL_INTEG_450	  	17
#define EPL_INTEG_550	  	18
#define EPL_INTEG_750	  	19
#define EPL_INTEG_950	  	20
#define EPL_INTEG_1250	  	21
#define EPL_INTEG_1600	  	22
#define EPL_INTEG_2000	  	23
#define EPL_INTEG_2700	  	24
#define EPL_INTEG_3500	  	25

#define EPL_FILTER_1		0<<5
#define EPL_FILTER_2		1<<5
#define EPL_FILTER_4		2<<5
#define EPL_FILTER_8		3<<5
#define EPL_FILTER_16	  	4<<5
#define EPL_FILTER_32	  	5<<5
#define EPL_FILTER_64	  	6<<5
#define EPL_FILTER_128	  	7<<5

#define EPL_MODE_HR         1<<4
#define EPL_MODE_HRS        9<<4

#define EPL_H_GAIN		    (0)
#define EPL_M_GAIN		    (1)
#define EPL_L_GAIN		    (3)
#define EPL_AUTO_GAIN	    (2)

#define EPL_OSR_64          0<<2
#define EPL_OSR_256			1<<2
#define EPL_OSR_1024		2<<2
#define EPL_OSR_2048		3<<2

#define EPL_IR_DISABLE     	0<<7
#define EPL_IR_ENABLE       1<<7

#define EPL_C_RESET			0x00
#define EPL_C_START_RUN		0x04
#define EPL_C_P_UP			0x04
#define EPL_C_P_DOWN		0x06
#define EPL_DATA_LOCK_ONLY	0x01
#define EPL_DATA_LOCK		0x05
#define EPL_DATA_UNLOCK		0x04

#define EPL_GO_MID			0x1E
#define EPL_GO_LOW			0x1E

#define EPL_INT_BINARY			0
#define EPL_INT_DISABLE			2
#define EPL_INT_ACTIVE_LOW		3
#define EPL_INT_FRAME_ENABLE	1

#define EPL_INT_CH0		(0<<2)
#define EPL_INT_CH1		(1<<2)
#define EPL_INT_CH2		(2<<2)
#define EPL_INT_CH3		(3<<2)

#define EPL_PST_1_TIME		(0 << 4)
#define EPL_PST_4_TIME		(1 << 4)
#define EPL_PST_8_TIME		(2 << 4)
#define EPL_PST_16_TIME		(3 << 4)
 
#define EPL_CK_FAST_2M		(1<<2)
#define EPL_CK_FAST_1M		(0<<2)
#define EPL_IRONTYPE_EACHCYCLE	1

#define EPL_IRONCTRL_ON		(1<<1)
#define EPL_IRONCTRL_OFF	(0<<1)

#define EPL_EN_GFIN_ENABLED		(1<<2)
#define EPL_EN_GFIN_DISABLED	(0<<2)

#define EPL_EN_VOS_ENABLED	(1<<1)
#define EPL_EN_VOS_DISABLED	(0<<1)

#define EPL_DOC_ON_ENABLED	1
#define EPL_DOC_ON_DISABLED	0

#define EPL_IR_MODE_CURRENT	0
#define EPL_IR_MODE_VOLTAGE	1

#define EPL_DRIVE_100MA		(1<<1)
#define EPL_DRIVE_200MA		(0<<1)

#endif



