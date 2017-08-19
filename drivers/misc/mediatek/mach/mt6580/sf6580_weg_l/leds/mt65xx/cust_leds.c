#include <cust_leds.h>
#include <cust_leds_def.h>
#include <mach/mt_pwm.h>

#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

extern int mtkfb_set_backlight_level(unsigned int level);
//extern int mtkfb_set_backlight_pwm(int div);
unsigned int set_backlight_first = 1;
int gesture_function_enable = 0;
unsigned int tmp_current_level = 0;
//brian-add
unsigned int tmp_previous_level = 32;
extern int disp_bls_set_backlight(unsigned int level);

unsigned int idle_clock_mode = 0;

// Only support 64 levels of backlight (when lcd-backlight = MT65XX_LED_MODE_PWM)
#define BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT 64 
// Support 256 levels of backlight (when lcd-backlight = MT65XX_LED_MODE_PWM)
#define BACKLIGHT_LEVEL_PWM_256_SUPPORT 256 

// Configure the support type "BACKLIGHT_LEVEL_PWM_256_SUPPORT" or "BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT" !!
#define BACKLIGHT_LEVEL_PWM_MODE_CONFIG BACKLIGHT_LEVEL_PWM_256_SUPPORT

unsigned int Cust_GetBacklightLevelSupport_byPWM(void)
{
	return BACKLIGHT_LEVEL_PWM_MODE_CONFIG;
}

unsigned int brightness_mapping(unsigned int level)
{
    unsigned int mapped_level;
    
    mapped_level = level;
       
	return mapped_level;
}
/*

 * To explain How to set these para for cust_led_list[] of led/backlight
 * "name" para: led or backlight
 * "mode" para:which mode for led/backlight
 *	such as:
 *			MT65XX_LED_MODE_NONE,	
 *			MT65XX_LED_MODE_PWM,	
 *			MT65XX_LED_MODE_GPIO,	
 *			MT65XX_LED_MODE_PMIC,	
 *			MT65XX_LED_MODE_CUST_LCM,	
 *			MT65XX_LED_MODE_CUST_BLS_PWM
 *
 *"data" para: control methord for led/backlight
 *   such as:
 *			MT65XX_LED_PMIC_LCD_ISINK=0,	
 *			MT65XX_LED_PMIC_NLED_ISINK0,
 *			MT65XX_LED_PMIC_NLED_ISINK1,
 *			MT65XX_LED_PMIC_NLED_ISINK2,
 *			MT65XX_LED_PMIC_NLED_ISINK3
 * 
 *"PWM_config" para:PWM(AP side Or BLS module), by default setting{0,0,0,0,0} Or {0}
 *struct PWM_config {	 
 *  int clock_source;
 *  int div; 
 *  int low_duration;
 *  int High_duration;
 *  BOOL pmic_pad;//AP side PWM pin in PMIC chip (only 89 needs confirm); 1:yes 0:no(default)
 *};
 *-------------------------------------------------------------------------------------------
 *   for AP PWM setting as follow:
 *1.	 PWM config data
 *  clock_source: clock source frequency, can be 0/1
 *  div: clock division, can be any value within 0~7 (i.e. 1/2^(div) = /1, /2, /4, /8, /16, /32, /64, /128)
 *  low_duration: only for BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT
 *  High_duration: only for BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT
 *
 *2.	 PWM freq.
 * If BACKLIGHT_LEVEL_PWM_MODE_CONFIG = BACKLIGHT_LEVEL_PWM_256_SUPPORT,
 *	 PWM freq. = clock source / 2^(div) / 256  
 *
 * If BACKLIGHT_LEVEL_PWM_MODE_CONFIG = BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT,
 *	 PWM freq. = clock source / 2^(div) / [(High_duration+1)(Level')+(low_duration+1)(64 - Level')]
 *	           = clock source / 2^(div) / [(High_duration+1)*64]     (when low_duration = High_duration)
 *Clock source: 
 *	 0: block clock/1625 = 26M/1625 = 16K (MT6571)
 *	 1: block clock = 26M (MT6571)
 *Div: 0~7
 *
 *For example, in MT6571, PWM_config = {1,1,0,0,0} 
 *	 ==> PWM freq. = 26M/2^1/256 	 =	50.78 KHz ( when BACKLIGHT_LEVEL_PWM_256_SUPPORT )
 *	 ==> PWM freq. = 26M/2^1/(0+1)*64 = 203.13 KHz ( when BACKLIGHT_LEVEL_PWM_64_FIFO_MODE_SUPPORT )
 *-------------------------------------------------------------------------------------------
 *   for BLS PWM setting as follow:
 *1.	 PWM config data
 *	 clock_source: clock source frequency, can be 0/1/2/3
 *	 div: clock division, can be any value within 0~1023
 *	 low_duration: non-use
 *	 High_duration: non-use
 *	 pmic_pad: non-use
 *
 *2.	 PWM freq.= clock source / (div + 1) /1024
 *Clock source: 
 *	 0: 26 MHz
 *	 1: 104 MHz
 *	 2: 124.8 MHz
 *	 3: 156 MHz
 *Div: 0~1023
 *
 *By default, clock_source = 0 and div = 0 => PWM freq. = 26 KHz 
 *-------------------------------------------------------------------------------------------
 */
 unsigned int brightness_mappingto16(unsigned int level)
{
	unsigned int map_level = 0;
	map_level = 16-level/16; //0-15,16-31,31-47......
	return map_level;
}

unsigned int brightness_mappingto32(unsigned int level)
{
	unsigned int map_level = 0;
	map_level = 32-level/8; //0-15,16-31,31-47......
	return map_level;
}

#ifdef GPIO_SIGNAL_LCM_BACKLIAGHT_PIN
unsigned int Cust_SetBacklight(int level, int div)
{
	mt_set_gpio_mode(GPIO_SIGNAL_LCM_BACKLIAGHT_PIN, 0);
	mt_set_gpio_dir(GPIO_SIGNAL_LCM_BACKLIAGHT_PIN, GPIO_DIR_OUT);


	printk("Cust_SetBacklight,level=%d\n",level);

	if(level == 0)
	{
			mt_set_gpio_out(GPIO_SIGNAL_LCM_BACKLIAGHT_PIN, GPIO_OUT_ZERO);
			mdelay(2);
	}
	else
	{
			mt_set_gpio_out(GPIO_SIGNAL_LCM_BACKLIAGHT_PIN, GPIO_OUT_ONE);
			mdelay(2);
	}

	
	mtkfb_set_backlight_level(level);

	
}
#endif
#if defined(Z43W5)
void turn_onoff_charge_led(BOOL On)
{
	printk("turn_onoff_charge_led,on=d%\n",On);
	if( On == 1)
	{
		mt_set_gpio_mode(GPIO_INDICATE_LED_PIN, GPIO_INDICATE_LED_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_INDICATE_LED_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_INDICATE_LED_PIN,GPIO_OUT_ONE);
	}else
	{
		mt_set_gpio_out(GPIO_INDICATE_LED_PIN,GPIO_OUT_ZERO);
	}
}

EXPORT_SYMBOL(turn_onoff_charge_led);

#endif

#if defined(GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN)
unsigned int Cust_Set_button_led(int level, int div)
{
	if( level == 0)
	{
		mt_set_gpio_out(GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN,GPIO_OUT_ZERO);
	}else
	{
		mt_set_gpio_mode(GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN, GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_SIGNAL_BUTTON_BACKLIAGHT_PIN,GPIO_OUT_ONE);
		
	}
}
#endif


#ifdef GPIO_SIGNAL_LED_RED_PIN 
unsigned int Cust_SetRedlight(int level, int div)
{
	printk("Cust_SetRedlight,level=%d\n",level);

	mt_set_gpio_mode(GPIO_SIGNAL_LED_RED_PIN, 0);
	mt_set_gpio_dir(GPIO_SIGNAL_LED_RED_PIN, GPIO_DIR_OUT);

	if(level == 0)
		mt_set_gpio_out(GPIO_SIGNAL_LED_RED_PIN, GPIO_OUT_ZERO);
	else
		mt_set_gpio_out(GPIO_SIGNAL_LED_RED_PIN, GPIO_OUT_ONE);
	mdelay(2);		
    return 0;
}
#endif

#ifdef GPIO_SIGNAL_LED_GREEN_PIN 
unsigned int Cust_SetGreenlight(int level, int div)
{
	printk("Cust_SetRedlight,level=%d\n",level);

	mt_set_gpio_mode(GPIO_SIGNAL_LED_GREEN_PIN, 0);
	mt_set_gpio_dir(GPIO_SIGNAL_LED_GREEN_PIN, GPIO_DIR_OUT);

	if(level == 0)
		mt_set_gpio_out(GPIO_SIGNAL_LED_GREEN_PIN, GPIO_OUT_ZERO);
	else
		mt_set_gpio_out(GPIO_SIGNAL_LED_GREEN_PIN, GPIO_OUT_ONE);
	mdelay(2);		
    return 0;
}
#endif

#ifdef GPIO_SIGNAL_LED_BLUE_PIN 
unsigned int Cust_SetBluelight(int level, int div)
{
	printk("Cust_SetRedlight,level=%d\n",level);

	mt_set_gpio_mode(GPIO_SIGNAL_LED_BLUE_PIN, 0);
	mt_set_gpio_dir(GPIO_SIGNAL_LED_BLUE_PIN, GPIO_DIR_OUT);

	if(level == 0)
		mt_set_gpio_out(GPIO_SIGNAL_LED_BLUE_PIN, GPIO_OUT_ZERO);
	else
		mt_set_gpio_out(GPIO_SIGNAL_LED_BLUE_PIN, GPIO_OUT_ONE);
	mdelay(2);		
    return 0;
}
#endif


#if 1 //defined(CONFIG_TOUCHSCREEN_MTK_GT9XX_HOTKNOT)
//extern void gesture_function_open(BOOL on);

void gesture_function_open(BOOL on)
{
	printk(KERN_ERR"gesture_function_open,on=%d\n",on);
	if( on == 1)
	{
		printk(KERN_ERR"1111111111111111111111111\n");
		gesture_function_enable =1;
	}else
	{
		printk(KERN_ERR"00000000000000000000000000\n");
		gesture_function_enable =0;
	}
}


extern int ofn_open_1(int enable);
unsigned int Cust_SetJogBall(int level)
{
	printk("Cust_SetJogBall,level=%d\n",level);
	if(level == 201)
	{
		printk("############0#############\n");
		gesture_function_open(0);
	}
	else if(level == 202)
	{
		printk("############1#############\n");
		gesture_function_open(1);
	}
	else if(level == 203)
	{
		printk("############203#############\n");
		idle_clock_mode = 0;
	}
	else if(level == 204)
	{
		printk("############204#############\n");
		idle_clock_mode = 1;
	}
	else
	{
		printk("############nothing#############\n");
	}
}
#else
unsigned int Cust_SetJogBall(int level)
{
	printk("##Cust_SetJogBall,level=%d\n",level);	
}
#endif
#if defined(CONFIG_C1_PROJECT)
static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
		{"red", 	      MT65XX_LED_MODE_NONE, -1,{0,0,0,0,0}},
		{"green",	      MT65XX_LED_MODE_NONE, -1,{0,0,0,0,0}},
		{"blue",              MT65XX_LED_MODE_NONE, -1,{0,0,0,0,0}},
		{"jogball-backlight", MT65XX_LED_MODE_GPIO, (int)Cust_SetJogBall,{0,0,0,0,0}},
		{"keyboard-backlight",MT65XX_LED_MODE_NONE, -1,{0,0,0,0,0}},
		{"button-backlight",  MT65XX_LED_MODE_NONE, -1,{0,0,0,0,0}},
		{"lcd-backlight",	  MT65XX_LED_MODE_GPIO, (int)Cust_SetBacklight, {0}},
	};
#else
static struct cust_mt65xx_led cust_led_list[MT65XX_LED_TYPE_TOTAL] = {
	{"red",               MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0,{0}},
	{"green",             MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK1,{0}},
	{"blue",              MT65XX_LED_MODE_PMIC, MT65XX_LED_PMIC_NLED_ISINK0,{0}},
	{"jogball-backlight", MT65XX_LED_MODE_NONE, -1,{0}},
	{"keyboard-backlight",MT65XX_LED_MODE_NONE, -1,{0}},
	{"button-backlight",  MT65XX_LED_MODE_NONE, -1,{0}},
	{"lcd-backlight",     MT65XX_LED_MODE_GPIO, (long)Cust_SetBacklight,{0}},
};
#endif


struct cust_mt65xx_led *get_cust_led_list(void)
{
	return cust_led_list;
}

