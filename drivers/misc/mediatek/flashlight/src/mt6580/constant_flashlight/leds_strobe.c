#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <cust_i2c.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

#include <linux/i2c.h>
#include <linux/leds.h>
#if defined(Z61)
#include <mach/upmu_common.h>
#include "leds_sw.h"
#endif



/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE

#define TAG_NAME "[leds_strobe.c]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(TAG_NAME "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        pr_warning(TAG_NAME "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      pr_notice(TAG_NAME "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        pr_info(TAG_NAME "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              pr_debug(TAG_NAME "<%s>\n", __FUNCTION__)
#define PK_TRC_VERBOSE(fmt, arg...) pr_debug(TAG_NAME fmt, ##arg)
#define PK_ERROR(fmt, arg...)       pr_err(TAG_NAME "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG PK_DBG_FUNC
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */


static u32 strobe_Res = 0;
static u32 strobe_Timeus = 0;
static BOOL g_strobe_On = 0;

static int g_duty=-1;
static int g_step=-1;
static int g_timeOutTimeMs=0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif


#define STROBE_DEVICE_ID 0xC6


static struct work_struct workTimeOut;

//#define FLASH_GPIO_ENF GPIO12
//#define FLASH_GPIO_ENT GPIO13

static int g_bLtVersion=0;

/*****************************************************************************
Functions
*****************************************************************************/
#if defined(Z61)
#elif defined(Z43W5)
#define GPIO_ENF GPIO_CAMERA_FLASH_EN_PIN
//#define GPIO_ENT GPIO_CAMERA_FLASH_EN_PIN
#else
#define GPIO_ENF GPIO_CAMERA_FLASH_EN_PIN
#define GPIO_ENT GPIO_CAMERA_FLASH_MODE_PIN
#endif

    /*CAMERA-FLASH-EN */


extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
static void work_timeOutFunc(struct work_struct *data);

static struct i2c_client *LM3642_i2c_client = NULL;




struct LM3642_platform_data {
	u8 torch_pin_enable;    // 1:  TX1/TORCH pin isa hardware TORCH enable
	u8 pam_sync_pin_enable; // 1:  TX2 Mode The ENVM/TX2 is a PAM Sync. on input
	u8 thermal_comp_mode_enable;// 1: LEDI/NTC pin in Thermal Comparator Mode
	u8 strobe_pin_disable;  // 1 : STROBE Input disabled
	u8 vout_mode_enable;  // 1 : Voltage Out Mode enable
};

struct LM3642_chip_data {
	struct i2c_client *client;

	//struct led_classdev cdev_flash;
	//struct led_classdev cdev_torch;
	//struct led_classdev cdev_indicator;

	struct LM3642_platform_data *pdata;
	struct mutex lock;

	u8 last_flag;
	u8 no_pdata;
};

/* i2c access*/
/*
static int LM3642_read_reg(struct i2c_client *client, u8 reg,u8 *val)
{
	int ret;
	struct LM3642_chip_data *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (ret < 0) {
		PK_ERR("failed reading at 0x%02x error %d\n",reg, ret);
		return ret;
	}
	*val = ret&0xff;

	return 0;
}*/

int FL_Enable(void)
{
	unsigned int level;
	static unsigned char duty_mapping[108] = {
				 0, 0,	0,	0,	0,	6,	1,	2,	1,	10, 1,	12,
				 6, 14, 2,	3,	16, 2,	18, 3,	6,	10, 22, 3,
				12, 8,	6,	28, 4,	30, 7,	10, 16, 6,	5,	18,
				12, 7,	6,	10, 8,	22, 7,	9,	16, 12, 8,	10,
				13, 18, 28, 9,	30, 20, 15, 12, 10, 16, 22, 13,
				11, 14, 18, 12, 19, 15, 26, 13, 16, 28, 21, 14,
				22, 30, 18, 15, 19, 16, 25, 20, 17, 21, 27, 18,
				22, 28, 19, 30, 24, 20, 31, 25, 21, 26, 22, 27,
				23, 28, 24, 30, 25, 31, 26, 27, 28, 29, 30, 31,
			};
			static unsigned char current_mapping[108] = {
				1,	2,	3,	4,	5,	0,	3,	2,	4,	0,	5,	0,
				1,	0,	4,	3,	0,	5,	0,	4,	2,	1,	0,	5,
				1,	2,	3,	0,	5,	0,	3,	2,	1,	4,	5,	1,
				2,	4,	5,	3,	4,	1,	5,	4,	2,	3,	5,	4,
				3,	2,	1,	5,	1,	2,	3,	4,	5,	3,	2,	4,
				5,	4,	3,	5,	3,	4,	2,	5,	4,	2,	3,	5,
				3,	2,	4,	5,	4,	5,	3,	4,	5,	4,	3,	5,
				4,	3,	5,	3,	4,	5,	3,	4,	5,	4,	5,	4,
				5,	4,	5,	4,	5,	4,	5,	5,	5,	5,	5,	5,
			};

#if defined(Z61)   
   printk("karlanz-->FL_Enable-->g_duty=%d\n",g_duty);
   if(g_duty == 0)
   {
    mt6325_upmu_set_isink_dim0_duty(31);
    mt6325_upmu_set_isink_dim1_duty(31);
    mt6325_upmu_set_isink_dim2_duty(31);
    mt6325_upmu_set_isink_dim3_duty(31);
    mt6325_upmu_set_isink_ch0_step(ISINK_4);
    mt6325_upmu_set_isink_ch1_step(ISINK_4);
    mt6325_upmu_set_isink_ch2_step(ISINK_4);
    mt6325_upmu_set_isink_ch3_step(ISINK_4);
    mt6325_upmu_set_isink_dim0_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim1_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim2_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim3_fsel(ISINK_2M_20KHZ); // 20Khz       
	
    mt6325_upmu_set_isink_ch0_en(NLED_ON); // Turn on ISINK Channel 0
    mt6325_upmu_set_isink_ch1_en(NLED_ON); // Turn on ISINK Channel 1
    mt6325_upmu_set_isink_ch2_en(NLED_ON); // Turn on ISINK Channel 2
    mt6325_upmu_set_isink_ch3_en(NLED_ON); // Turn on ISINK Channel 3
   }
   else
   {
     mt6325_upmu_set_isink_dim0_duty(31);
    mt6325_upmu_set_isink_dim1_duty(31);
    mt6325_upmu_set_isink_dim2_duty(31);
    mt6325_upmu_set_isink_dim3_duty(31);
    mt6325_upmu_set_isink_ch0_step(ISINK_5);
    mt6325_upmu_set_isink_ch1_step(ISINK_5);
    mt6325_upmu_set_isink_ch2_step(ISINK_5);
    mt6325_upmu_set_isink_ch3_step(ISINK_5);
    mt6325_upmu_set_isink_dim0_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim1_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim2_fsel(ISINK_2M_20KHZ); // 20Khz
    mt6325_upmu_set_isink_dim3_fsel(ISINK_2M_20KHZ); // 20Khz       

	mt6325_upmu_set_rg_isink0_double_en(0x01);
	mt6325_upmu_set_rg_isink1_double_en(0x01);
	mt6325_upmu_set_rg_isink2_double_en(0x01);
	mt6325_upmu_set_rg_isink3_double_en(0x01);

	
    mt6325_upmu_set_isink_ch0_en(NLED_ON); // Turn on ISINK Channel 0
    mt6325_upmu_set_isink_ch1_en(NLED_ON); // Turn on ISINK Channel 1
    mt6325_upmu_set_isink_ch2_en(NLED_ON); // Turn on ISINK Channel 2
    mt6325_upmu_set_isink_ch3_en(NLED_ON); // Turn on ISINK Channel 3
   }
   printk("FL_enable");        
#elif defined(Z43W5)
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
	PK_DBG(" FL_Enable line=%d\n",__LINE__);
#else
	if(g_duty==0)
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}
	else
	{
		mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ONE);
		mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ONE);
		PK_DBG(" FL_Enable line=%d\n",__LINE__);
	}
#endif
    return 0;
}



int FL_Disable(void)
{
#if defined(Z61)
	mt6325_upmu_set_isink_ch0_en(NLED_OFF); // Turn off ISINK Channel 0
    mt6325_upmu_set_isink_ch1_en(NLED_OFF); // Turn off ISINK Channel 1
    mt6325_upmu_set_isink_ch2_en(NLED_OFF); // Turn off ISINK Channel 2
    mt6325_upmu_set_isink_ch3_en(NLED_OFF); // Turn off ISINK Channel 3
	printk("FL_disable");
#elif defined(Z43W5)
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
	PK_DBG(" FL_Enable line=%d\n",__LINE__);
#else
	mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO);
	mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO);
	PK_DBG(" FL_Disable line=%d\n",__LINE__);
#endif
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
	g_duty = duty;
    return 0;
}

int FL_step(kal_uint32 step)
{

    return 0;
}

int FL_init(void)
{
#if defined(Z61)
			mt6325_upmu_set_rg_g_drv_2m_ck_pdn(0x0); // Disable power down 

            // For backlight: Current: 24mA, PWM frequency: 20K, Duty: 20~100, Soft start: off, Phase shift: on
            // ISINK0
            mt6325_upmu_set_rg_drv_isink0_ck_pdn(0x0); // Disable power down    
            mt6325_upmu_set_rg_drv_isink0_ck_cksel(0x1); // Freq = 1Mhz for Backlight
			mt6325_upmu_set_isink_ch0_mode(ISINK_PWM_MODE);
            mt6325_upmu_set_isink_ch0_step(ISINK_5); // 24mA
            mt6325_upmu_set_isink_sfstr0_en(0x0); // Disable soft start
			mt6325_upmu_set_rg_isink0_double_en(0x1); // Enable double current
			mt6325_upmu_set_isink_phase_dly_tc(0x0); // TC = 0.5us
			mt6325_upmu_set_isink_phase0_dly_en(0x1); // Enable phase delay
            mt6325_upmu_set_isink_chop0_en(0x1); // Enable CHOP clk
            // ISINK1
            mt6325_upmu_set_rg_drv_isink1_ck_pdn(0x0); // Disable power down   
            mt6325_upmu_set_rg_drv_isink1_ck_cksel(0x1); // Freq = 1Mhz for Backlight
			mt6325_upmu_set_isink_ch1_mode(ISINK_PWM_MODE);
            mt6325_upmu_set_isink_ch1_step(ISINK_3); // 24mA
            mt6325_upmu_set_isink_sfstr1_en(0x0); // Disable soft start
			mt6325_upmu_set_rg_isink1_double_en(0x1); // Enable double current
			mt6325_upmu_set_isink_phase1_dly_en(0x1); // Enable phase delay
            mt6325_upmu_set_isink_chop1_en(0x1); // Enable CHOP clk         
            // ISINK2
            mt6325_upmu_set_rg_drv_isink2_ck_pdn(0x0); // Disable power down   
            mt6325_upmu_set_rg_drv_isink2_ck_cksel(0x1); // Freq = 1Mhz for Backlight
			mt6325_upmu_set_isink_ch2_mode(ISINK_PWM_MODE);
            mt6325_upmu_set_isink_ch2_step(ISINK_3); // 24mA
            mt6325_upmu_set_isink_sfstr2_en(0x0); // Disable soft start
			mt6325_upmu_set_rg_isink2_double_en(0x1); // Enable double current
			mt6325_upmu_set_isink_phase2_dly_en(0x1); // Enable phase delay
            mt6325_upmu_set_isink_chop2_en(0x1); // Enable CHOP clk   
			// ISINK3
			mt6325_upmu_set_rg_drv_isink3_ck_pdn(0x0); // Disable power down   
			mt6325_upmu_set_rg_drv_isink3_ck_cksel(0x1); // Freq = 1Mhz for Backlight
			mt6325_upmu_set_isink_ch3_mode(ISINK_PWM_MODE);
			mt6325_upmu_set_isink_ch3_step(ISINK_3); // 24mA
			mt6325_upmu_set_isink_sfstr3_en(0x0); // Disable soft start
			mt6325_upmu_set_rg_isink3_double_en(0x1); // Enable double current
			mt6325_upmu_set_isink_phase3_dly_en(0x1); // Enable phase delay
			mt6325_upmu_set_isink_chop3_en(0x1); // Enable CHOP clk  


			

			mt6325_upmu_set_isink_ch0_en(NLED_OFF); // Turn off ISINK Channel 0
            mt6325_upmu_set_isink_ch1_en(NLED_OFF); // Turn off ISINK Channel 1
            mt6325_upmu_set_isink_ch2_en(NLED_OFF); // Turn off ISINK Channel 2
			mt6325_upmu_set_isink_ch3_en(NLED_OFF); // Turn off ISINK Channel 3
	printk("FL_init wang");
#elif defined(Z43W5)
	if(mt_set_gpio_mode(GPIO_ENF,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
	if(mt_set_gpio_dir(GPIO_ENF,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
	if(mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
#else
	if(mt_set_gpio_mode(GPIO_ENF,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENF,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENF,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
    /*Init. to disable*/
    if(mt_set_gpio_mode(GPIO_ENT,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
    if(mt_set_gpio_dir(GPIO_ENT,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
    if(mt_set_gpio_out(GPIO_ENT,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}
#endif
    INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	FL_Disable();
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("ledTimeOut_callback\n");
    //printk(KERN_ALERT "work handler function./n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;

}



static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int iFlashType = (int)FLASHLIGHT_NONE;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("LM3642 constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift,(int)arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",(int)arg);
			g_timeOutTimeMs=arg;
		break;


    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",(int)arg);
    		g_duty=arg;
    		FL_dim_duty(arg);
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",(int)arg);
    		g_step=arg;
    		FL_step(arg);
    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",(int)arg);
    		if(arg==1)
    		{

    		    int s;
    		    int ms;
    		    if(g_timeOutTimeMs>1000)
            	{
            		s = g_timeOutTimeMs/1000;
            		ms = g_timeOutTimeMs - s*1000;
            	}
            	else
            	{
            		s = 0;
            		ms = g_timeOutTimeMs;
            	}

				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( s, ms*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
    			FL_Enable();
    			g_strobe_On=1;
    		}
    		else
    		{
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
				g_strobe_On=0;
    		}
    		break;
		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_init();
		timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);

        strobe_Res = 0;
        strobe_Timeus = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

    	FL_Uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}


FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);


