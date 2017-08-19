/*
 * This software is licensed under the terms of the GNU General Public 
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms. 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * * VERSION      	DATE			AUTHOR          Note
 *    1.0		  2013-7-16			Focaltech        initial  based on MTK platform
 * 
 */

#include "tpd.h"

#include "tpd_custom_fts.h"
#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif
#ifdef TPD_SYSFS_DEBUG
#include "focaltech_ex_fun.h"
#endif
#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#include "cust_gpio_usage.h"

#define TPD_RES_X 400
#define TPD_RES_Y 400


//#define FTS_SCAP_TEST
#ifdef FTS_SCAP_TEST
struct i2c_client *g_focalclient = NULL;
#endif
extern  int gesture_function_enable;
#define GESTURE_SWITCH_FILE 		"/data/data/com.example.setgesture/shared_prefs/gesture.xml"  //总开关文件,获取第一个value的值,为1开,为0关

#ifdef FT6x36_DOWNLOAD
#include "ft6x36_download_lib.h"
static struct i2c_client *g_i2c_client = NULL;
static unsigned char CTPM_MAIN_FW1[]=
{
    #include "FT6336_V9_S4153_TH0464F00A_0x12_all.i"
};
static unsigned char CTPM_MAIN_FW2[]=
{
	#include "FT6336_V9_S4153_HC-C1853_0x12_all.i"
};
#endif



#include "mt_pm_ldo.h"
  extern struct tpd_device *tpd;
static  struct regulator *tempregs = NULL;
static int power_flag=0;// 0 power off,default, 1 power on
#define SWITCH_OFF                  0
#define SWITCH_ON                   1
#define POWER_ON					1
#define POWER_OFF					0


#define GTP_GPIO_OUTPUT(pin,level)      do{\
											  if(pin == GPIO_CTP_EINT_PIN)\
												  mt_set_gpio_mode(pin, GPIO_CTP_EINT_PIN_M_GPIO);\
											  else\
												  mt_set_gpio_mode(pin, GPIO_CTP_RST_PIN_M_GPIO);\
											  mt_set_gpio_dir(pin, GPIO_DIR_OUT);\
											  mt_set_gpio_out(pin, level);\
										  }while(0)

#define GTP_RST_PORT    GPIO_CTP_RST_PIN
#define GTP_INT_PORT    GPIO_CTP_EINT_PIN


static  void ft3x07_power_switch(s32 state)
{
	int ret = 0;
	switch (state) 
	{
		case POWER_ON:
			if(power_flag==0)
			{
				printk("Power switch on!\n");
				if(NULL == tpd->reg)
				{
				  	tpd->reg=regulator_get(tpd->tpd_dev,TPD_POWER_SOURCE_CUSTOM); // get pointer to regulator structure
					if (IS_ERR(tpd->reg)) {
						printk("touch panel regulator_get() failed!\n");
						return;
					}else
					{
						printk("regulator_get() Ok!\n");
					}
				}

				printk("regulator_set_voltage--begin\r\n");
				ret=regulator_set_voltage(tpd->reg, 2800000, 2800000);  // set 2.8v
				printk("regulator_set_voltage--end\r\n");
				if (ret)
					printk("regulator_set_voltage() failed!\n");
				ret=regulator_enable(tpd->reg);  //enable regulator
				if (ret)
					printk("regulator_enable() failed!\n");

				power_flag=1;
			}
			else
			{
		  		printk("######Power already is on!#######\n");
		  	}
			break;
		case POWER_OFF:
			if(power_flag==1)
			{
				printk("Power switch off!\n");
				if(!IS_ERR_OR_NULL(tpd->reg))
				{
					ret=regulator_disable(tpd->reg); //disable regulator
					if (ret)
						printk("regulator_disable() failed!\n");
					regulator_put(tpd->reg);
					tpd->reg = NULL;
					power_flag=0;
				}
			}
			else
			{
				printk("#######Power already is off!########\n");
			}
			break;
		  default:
			printk("Invalid power switch command!");
			break;
	}
	return;
}





//wangcq327 --- add start
static int TP_VOL;
//wangcq327 --- add end
  struct Upgrade_Info fts_updateinfo[] =
{
        {0x55,"FT5x06",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
        {0x08,"FT5606",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x06, 100, 2000},
	{0x0a,"FT5x16",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x07, 1, 1500},
	{0x05,"FT6208",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,60, 30, 0x79, 0x05, 10, 2000},
	{0x06,"FT6x06",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x08, 10, 2000},
	{0x36,"FT6x36",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x18, 10, 2000},
	{0x0E,"FT3207",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x18, 10, 2000},//CHIP ID error
	{0x55,"FT5x06i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
	{0x14,"FT5336",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x13,"FT3316",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x12,"FT5436i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x11,"FT5336i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x54,"FT5422",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,2, 2, 0x54, 0x2c, 10, 2000},
};
				
struct Upgrade_Info fts_updateinfo_curr;

#ifdef TPD_PROXIMITY
#define APS_ERR(fmt,arg...)           	printk("<<proximity>> "fmt"\n",##arg)
#define TPD_PROXIMITY_DEBUG(fmt,arg...) printk("<<proximity>> "fmt"\n",##arg)
#define TPD_PROXIMITY_DMESG(fmt,arg...) printk("<<proximity>> "fmt"\n",##arg)

static u8 tpd_proximity_flag 			= 0;
static u8 tpd_proximity_suspend 		= 0;
static u8 tpd_proximity_detect 		= 1;//0-->close ; 1--> far away
static u8 tpd_proximity_detect_prev	= 0xff;//0-->close ; 1--> far away
#endif

#ifdef FTS_GESTRUE
#define GESTURE_LEFT		0x20
#define GESTURE_RIGHT		0x21
#define GESTURE_UP		    0x22
#define GESTURE_DOWN		0x23
#define GESTURE_DOUBLECLICK	0x24
#define GESTURE_O		    0x30
#define GESTURE_W		    0x31
#define GESTURE_M		    0x32
#define GESTURE_E		    0x33
#define GESTURE_C		    0x34
#define GESTURE_S		    0x46
#define GESTURE_V		    0x54
#define GESTURE_Z		    0x41

#include "ft_gesture_lib.h"

#define FTS_GESTRUE_POINTS 255
#define FTS_GESTRUE_POINTS_ONETIME  62
#define FTS_GESTRUE_POINTS_HEADER 8
#define FTS_GESTURE_OUTPUT_ADRESS 0xD3
#define FTS_GESTURE_OUTPUT_UNIT_LENGTH 4

//suspend_state_t get_suspend_state(void);

unsigned short coordinate_x[150] = {0};
unsigned short coordinate_y[150] = {0};
#endif
extern struct tpd_device *tpd;
 
static struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);
 
 
static void tpd_eint_interrupt_handler(void);
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
//extern void mt_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
 
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);

static int tpd_flag 					= 0;
static int tpd_halt						= 0;
static int point_num 					= 0;
static int p_point_num 					= 0;



//#define TPD_CLOSE_POWER_IN_SLEEP
#define TPD_OK 							0
//register define
#define DEVICE_MODE 					0x00
#define GEST_ID 						0x01
#define TD_STATUS 						0x02
//point1 info from 0x03~0x08
//point2 info from 0x09~0x0E
//point3 info from 0x0F~0x14
//point4 info from 0x15~0x1A
//point5 info from 0x1B~0x20
//register define

#define TPD_RESET_ISSUE_WORKAROUND

#define TPD_MAX_RESET_COUNT 			2
//extern int tpd_mstar_status ;  // compatible mstar and ft6306 chenzhecong

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

#define VELOCITY_CUSTOM_FT5206
#ifdef VELOCITY_CUSTOM_FT5206
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

// for magnify velocity********************************************

#ifndef TPD_VELOCITY_CUSTOM_X
#define TPD_VELOCITY_CUSTOM_X 			10
#endif
#ifndef TPD_VELOCITY_CUSTOM_Y
#define TPD_VELOCITY_CUSTOM_Y 			10
#endif

#define TOUCH_IOC_MAGIC 				'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)


static int g_v_magnify_x =TPD_VELOCITY_CUSTOM_X;
static int g_v_magnify_y =TPD_VELOCITY_CUSTOM_Y;

static int tpd_misc_open(struct inode *inode, struct file *file)
{
/*
	file->private_data = adxl345_i2c_client;

	if(file->private_data == NULL)
	{
		printk("tpd: null pointer!!\n");
		return -EINVAL;
	}
	*/
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tpd_misc_release(struct inode *inode, struct file *file)
{
	//file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int adxl345_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	//struct i2c_client *client = (struct i2c_client*)file->private_data;
	//struct adxl345_i2c_data *obj = (struct adxl345_i2c_data*)i2c_get_clientdata(client);	
	//char strbuf[256];
	void __user *data;
	
	long err = 0;
	
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case TPD_GET_VELOCITY_CUSTOM_X:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

	   case TPD_GET_VELOCITY_CUSTOM_Y:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		default:
			printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	return err;
}


static struct file_operations tpd_fops = {
//	.owner = THIS_MODULE,
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TPD_NAME,
	.fops = &tpd_fops,
};

//**********************************************
#endif

struct touch_info {
    int y[10];
    int x[10];
    int p[10];
    int id[10];
};
 
static const struct i2c_device_id ft5206_tpd_id[] = {{TPD_NAME,0},{}};
//unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
//static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata ft5206_i2c_tpd={ I2C_BOARD_INFO(TPD_NAME, (0x70>>1))};
 
static struct i2c_driver tpd_i2c_driver = {
  	.driver = {
	 	.name 	= TPD_NAME,
	//	.owner 	= THIS_MODULE,
  	},
  	.probe 		= tpd_probe,
  	.remove 	= tpd_remove,
  	.id_table 	= ft5206_tpd_id,
  	.detect 	= tpd_detect,
// 	.shutdown	= tpd_shutdown,
//  .address_data = &addr_data,
};


static  void tpd_down(int x, int y, int p) {
	int temp;
	#if MTK_LCM_PHYSICAL_ROTATION== 270
		temp = x;
		x = y;
		y = TPD_RES_X - temp;
	#elif MTK_LCM_PHYSICAL_ROTATION == 180
		x = TPD_RES_X - x;
		y = TPD_RES_Y -y;
	#elif MTK_LCM_PHYSICAL_ROTATION == 90
		temp = x;
		x = TPD_RES_Y -y;
		y = temp;
	#else
	#endif

	// input_report_abs(tpd->dev, ABS_PRESSURE, p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 20);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	//printk("D[%4d %4d %4d] ", x, y, p);
	/* track id Start 0 */
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	input_mt_sync(tpd->dev);
	TPD_EM_PRINT(x, y, x, y, p-1, 1);
//    if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
//    {   
//      tpd_button(x, y, 1);  
//    }
}
 
static  void tpd_up(int x, int y) {
	int temp;
	#if MTK_LCM_PHYSICAL_ROTATION== 270
		temp = x;
		x = y;
		y = TPD_RES_X - temp;
	#elif MTK_LCM_PHYSICAL_ROTATION == 180
		x = TPD_RES_X - x;
		y = TPD_RES_Y -y;
	#elif MTK_LCM_PHYSICAL_ROTATION == 90
		temp = x;
		x = TPD_RES_Y -y;
		y = temp;
	#else
	#endif

	//input_report_abs(tpd->dev, ABS_PRESSURE, 0);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	//input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	//printk("U[%4d %4d %4d] ", x, y, 0);
	input_mt_sync(tpd->dev);
	TPD_EM_PRINT(x, y, x, y, 0, 0);
//    if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
//    {   
//       tpd_button(x, y, 0); 
//    }   		 
}

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
	int i = 0;
//#if (TPD_MAX_POINTS==2)
//	char data[35] = {0};
//#else
//	char data[16] = {0};
//#endif	
char data[128] = {0};
    u16 high_byte,low_byte,reg;
	u8 report_rate =0;

	p_point_num = point_num;
	if (tpd_halt)
	{
		TPD_DMESG( "tpd_touchinfo return ..\n");
		return false;
	}
	mutex_lock(&i2c_access);

    reg = 0x00;
	fts_i2c_Read(i2c_client, &reg, 1, data, 64);
	mutex_unlock(&i2c_access);
	//TPD_DEBUG("received raw data from touch panel as following:\n");
	//TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
	//TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
	//TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);

	/*get the number of the touch points*/
	point_num= data[2] & 0x0f;

	//TPD_DEBUG("point_num =%d\n",point_num);
        if(point_num>fts_updateinfo_curr.TPD_MAX_POINTS)return false;
        
	for(i = 0; i < point_num; i++)  
	{
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 
     	cinfo->id[i] = data[3+6*i+2]>>4; //touch id
	   	/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] = high_byte |low_byte;

		//cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra

		/*get the Y coordinate, 2 bytes*/

		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;

		//if(!(point_num) && !(cinfo->p[i]) && !(cinfo->id[i]) && !(cinfo->x[i]) && !(cinfo->y[i]))return false;
                          


		 //cinfo->y[i]=  cinfo->y[i] * 800 >> 11;
	}
	TPD_DEBUG(" cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
	//TPD_DEBUG(" cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);		
	//TPD_DEBUG(" cinfo->x[2]= %d, cinfo->y[2]= %d, cinfo->p[2] = %d\n", cinfo->x[2], cinfo->y[2], cinfo->p[2]);	

	return true;
};

#ifdef TPD_PROXIMITY
static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;
}

static int tpd_enable_ps(int enable)
{
	u8 state, state2;
	int ret = -1;

	i2c_smbus_read_i2c_block_data(i2c_client, 0xB0, 1, &state);
	printk("[proxi_5206]read: 999 0xb0's value is 0x%02X\n", state);
	if (enable){
		state |= 0x01;
		tpd_proximity_flag = 1;
		TPD_PROXIMITY_DEBUG("[proxi_5206]ps function is on\n");	
	}else{
		state &= 0x00;	
		tpd_proximity_flag = 0;
		TPD_PROXIMITY_DEBUG("[proxi_5206]ps function is off\n");
	}

	ret = i2c_smbus_write_i2c_block_data(i2c_client, 0xB0, 1, &state);
	TPD_PROXIMITY_DEBUG("[proxi_5206]write: 0xB0's value is 0x%02X\n", state);

	i2c_smbus_read_i2c_block_data(i2c_client, 0xB0, 1, &state2);
	if(state!=state2)
	{
		tpd_proximity_flag=0;
		printk("[proxi_5206]ps fail!!! state = 0x%x,  state2 =  0x%X\n", state,state2);
	}

	return 0;
}

static int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;

	hwm_sensor_data *sensor_data;
	TPD_DEBUG("[proxi_5206]command = 0x%02X\n", command);		
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value)
				{
					if((tpd_enable_ps(1) != 0))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
				}
				else
				{
					if((tpd_enable_ps(0) != 0))
					{
						APS_ERR("disable ps fail: %d\n", err);
						return -1;
					}
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{

				sensor_data = (hwm_sensor_data *)buff_out;

				sensor_data->values[0] = tpd_get_ps_value();
				TPD_PROXIMITY_DEBUG("huang sensor_data->values[0] 1082 = %d\n", sensor_data->values[0]);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}
#endif

#ifdef FTS_GESTRUE
static void check_gesture(int gesture_id)
{
	
    printk("kaka gesture_id==0x%x\n ",gesture_id);
    
	switch(gesture_id)
	{
		case GESTURE_DOUBLECLICK:	
				input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);		    
			break;
#if 0
		case GESTURE_LEFT:		     
		      input_report_key(tpd->dev, KEY_LEFT, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_LEFT, 0);
			    input_sync(tpd->dev);
			break;
		case GESTURE_RIGHT:						
  				input_report_key(tpd->dev, KEY_RIGHT, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_RIGHT, 0);
			    input_sync(tpd->dev);
 
			break;
		case GESTURE_UP:	
			input_report_key(tpd->dev, KEY_UP, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_UP, 0);
			    input_sync(tpd->dev);
			    
			break;
		case GESTURE_DOWN:		
				input_report_key(tpd->dev, KEY_DOWN, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_DOWN, 0);
			    input_sync(tpd->dev);
		    
			break;
#endif
/*
		case GESTURE_O:	
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
					input_report_key(tpd->dev, KEY_O, 1);
			    input_sync(tpd->dev);
			    input_report_key(tpd->dev, KEY_O, 0);
			    input_sync(tpd->dev);
			break;
		case GESTURE_W:			
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
		input_report_key(tpd->dev, KEY_W, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_W, 0);
			    input_sync(tpd->dev);
			    
			break;
		case GESTURE_M:		
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
	input_report_key(tpd->dev, KEY_M, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_M, 0);
			    input_sync(tpd->dev);
			    
			break;
		case GESTURE_E:		
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
		input_report_key(tpd->dev, KEY_E, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_E, 0);
			    input_sync(tpd->dev);
			    
			break;
		case GESTURE_C:		
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
		input_report_key(tpd->dev, KEY_C, 1);
			 input_sync(tpd->dev);
			 input_report_key(tpd->dev, KEY_C, 0);
			 input_sync(tpd->dev);
			break;

		case GESTURE_S:		
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
	input_report_key(tpd->dev, KEY_S, 1);
		 input_sync(tpd->dev);
		 input_report_key(tpd->dev, KEY_S, 0);
		 input_sync(tpd->dev);
		break;

		case GESTURE_V:
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
				input_report_key(tpd->dev, KEY_V, 1);
		 input_sync(tpd->dev);
		 input_report_key(tpd->dev, KEY_V, 0);
		 input_sync(tpd->dev);
		break;

		case GESTURE_Z:	
							input_report_key(tpd->dev, KEY_POWER, 1);
			    input_sync(tpd->dev);
			     input_report_key(tpd->dev, KEY_POWER, 0);
			    input_sync(tpd->dev);
		input_report_key(tpd->dev, KEY_Z, 1);
		 input_sync(tpd->dev);
		 input_report_key(tpd->dev, KEY_Z, 0);
		 input_sync(tpd->dev);
			break;
*/
		default:
		
			break;
	}

}

static int ft5x0x_read_Touchdata(void)
{
    unsigned char buf[FTS_GESTRUE_POINTS * 4] = { 0 };
    int ret = -1;
    int i = 0;
    buf[0] = 0xd3;
    int gestrue_id = 0;
    short pointnum = 0;
    ret = fts_i2c_Read(i2c_client, buf, 1, buf, FTS_GESTRUE_POINTS_HEADER);
    if (ret < 0)
    {
        printk( "%s read touchdata failed.\n", __func__);
        return ret;
    }
    /* FW ?±?ó??3?ê?ê? */
  //  if (0x24 == buf[0] || 0x20 == buf[0] || 0x21 == buf[0] || 0x22 == buf[0] || 0x23 == buf[0])
  if(buf[0]!=0xfe)
    {
        gestrue_id =  buf[0];
        check_gesture(gestrue_id);
        return -1;
    }

    pointnum = (short)(buf[1]) & 0xff;
    buf[0] = 0xd3;
    if((pointnum * 4 + 2+6)<255)
    {
    ret = fts_i2c_Read(i2c_client, buf, 1, buf, (pointnum * 4 + 2 + 6));
    }
    else
    {
         ret = fts_i2c_Read(i2c_client, buf, 1, buf, 255);
          ret = fts_i2c_Read(i2c_client, buf, 0, buf+255, (pointnum * 4 + 2 +6)-255);

    }
    if (ret < 0)
    {
        printk( "%s read touchdata failed.\n", __func__);
        return ret;
    }
   gestrue_id = fetch_object_sample(buf, pointnum);
    for(i = 0;i < pointnum;i++)
    {
        coordinate_x[i] =  (((s16) buf[0 + (4 * i)]) & 0x0F) <<
            8 | (((s16) buf[1 + (4 * i)])& 0xFF);
        coordinate_y[i] = (((s16) buf[2 + (4 * i)]) & 0x0F) <<
            8 | (((s16) buf[3 + (4 * i)]) & 0xFF);
    }
	check_gesture(gestrue_id);
    return -1;
}
#endif
 static int touch_event_handler(void *unused)
{ 
	struct touch_info cinfo, pinfo;
	int i=0;

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);

#ifdef TPD_PROXIMITY
	int err;
	hwm_sensor_data sensor_data;
	u8 proximity_status;
	
#endif
	u8 state;

	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE); 
	//	printk("wangcq327 --- waitting\n");
		wait_event_interruptible(waiter,tpd_flag!=0);				 
//		printk("wangcq327 --- pass\n");
		tpd_flag = 0;

		set_current_state(TASK_RUNNING);
		printk("touch_event_handler\n");
#ifdef FTS_GESTRUE
	if(gesture_function_enable == 1)
	{
		i2c_smbus_read_i2c_block_data(i2c_client, 0xd0, 1, &state);
		printk("state=%d\n",state);
//		if((get_suspend_state() == PM_SUSPEND_MEM) && (state ==1))
		if(state ==1)
		{
			ft5x0x_read_Touchdata();
			continue;
		}
	}
#endif

#ifdef TPD_PROXIMITY
		if (tpd_proximity_flag == 1)
		{
			i2c_smbus_read_i2c_block_data(i2c_client, 0xB0, 1, &state);
			TPD_PROXIMITY_DEBUG("proxi_5206 0xB0 state value is 1131 0x%02X\n", state);

			if(!(state&0x01))
			{
				tpd_enable_ps(1);
			}

			i2c_smbus_read_i2c_block_data(i2c_client, 0x01, 1, &proximity_status);
			TPD_PROXIMITY_DEBUG("proxi_5206 0x01 value is 1139 0x%02X\n", proximity_status);

			if (proximity_status == 0xC0)
			{
				tpd_proximity_detect = 0;	
			}
			else if(proximity_status == 0xE0)
			{
				tpd_proximity_detect = 1;
			}

			TPD_PROXIMITY_DEBUG("tpd_proximity_detect 1149 = %d\n", tpd_proximity_detect);

			if(tpd_proximity_detect != tpd_proximity_detect_prev)
			{
				tpd_proximity_detect_prev = tpd_proximity_detect;
				sensor_data.values[0] = tpd_get_ps_value();
				sensor_data.value_divide = 1;
				sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
				if ((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
				{
					TPD_PROXIMITY_DMESG(" proxi_5206 call hwmsen_get_interrupt_data failed= %d\n", err);	
				}
			}
		}  
#endif

		if (tpd_touchinfo(&cinfo, &pinfo)) 
		{
			//TPD_DEBUG("point_num = %d\n",point_num);
			TPD_DEBUG_SET_TIME;
			if(point_num >0) 
			{
				for(i =0; i<point_num; i++)//only support 3 point
				{
					tpd_down(cinfo.x[i], cinfo.y[i], cinfo.id[i]);
				}
				input_sync(tpd->dev);
			}
			else
			{
				tpd_up(cinfo.x[0], cinfo.y[0]);
				//TPD_DEBUG("release --->\n"); 
				//input_mt_sync(tpd->dev);
				input_sync(tpd->dev);
			}
		}

		if(tpd_mode==12)
		{
			//power down for desence debug
			//power off, need confirm with SA
#ifdef TPD_POWER_SOURCE_CUSTOM
				printk("TPd enter power off---begin-\r\n");
			ft3x07_power_switch(SWITCH_OFF);
			printk("TPd enter power off---end-\r\n");
#else
			hwPowerDown(MT65XX_POWER_LDO_VGP2, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
			hwPowerDown(TPD_POWER_SOURCE_1800, "TP");
#endif
			msleep(20);
		}
	}while(!kthread_should_stop());

	return 0;
}
 
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
{
	strcpy(info->type, TPD_DEVICE);	
	return 0;
}
 
static void tpd_eint_interrupt_handler(void)
{
	//TPD_DEBUG("TPD interrupt has been triggered\n");
	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}

void focaltech_get_upgrade_array(void)
{
	u8 chip_id;
	u32 i;

	i2c_smbus_read_i2c_block_data(i2c_client,FT_REG_CHIP_ID,1,&chip_id);

	DBG("%s chip_id = %x\n", __func__, chip_id);
//chip_id = 0x36;
	for(i=0;i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info);i++)
	{
		if(chip_id==fts_updateinfo[i].CHIP_ID)
		{
			memcpy(&fts_updateinfo_curr, &fts_updateinfo[i], sizeof(struct Upgrade_Info));
			break;
		}
	}

	if(i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info))
	{
		memcpy(&fts_updateinfo_curr, &fts_updateinfo[11], sizeof(struct Upgrade_Info));
	}
}


#ifdef FT6x36_DOWNLOAD

int ft5x0x_download_i2c_Read(unsigned char *writebuf,
            int writelen, unsigned char *readbuf, int readlen)
{
    int ret;

	ret=fts_i2c_Read(g_i2c_client, writebuf,writelen,readbuf,readlen);
  
    return ret;
}
/*write data by i2c*/
int ft5x0x_download_i2c_Write(unsigned char *writebuf, int writelen)
{
    int ret;

   ret=fts_i2c_Write(g_i2c_client, writebuf,writelen);

    return ret;
}


int ft6x36_Enter_Debug(void)
{
    //ft5x0x_reset_tp(0);
    //msleep(4);
    //ft5x0x_reset_tp(1);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(2);  
   // mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
   // mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
 DBG("ft6x36_Enter_Debug\n");
    return ft6x36_Lib_Enter_Download_Mode();
}
//if return 0, main flash is ok, else download.
int ft6x36_IsDownloadMain(void)
{
    //add condition to check main flash
    return -1;
}
int ft6x36_DownloadMain(void)
{
    unsigned short fwlen = 0;
	DBG("ft6x36_DownloadMain 1");
    if (ft6x36_Enter_Debug() < 0) {
       // DBG("-----enter debug mode failed\n");
        return -1;
    }
	DBG("ft6x36_DownloadMain 2");
	if(1 == TP_VOL){
	    fwlen = sizeof(CTPM_MAIN_FW1);
	   // DBG("wangcq327 ---1 ----fwlen=%d\n", fwlen);

	    //return ft6x06_Lib_DownloadMain(CTPM_MAIN_FW, fwlen);
	    return ft6x36_Lib_DownloadMain(CTPM_MAIN_FW1,fwlen);
	}else if(0 == TP_VOL){
	    fwlen = sizeof(CTPM_MAIN_FW2);
	    //DBG("wangcq327 ---2 ----fwlen=%d\n", fwlen);

	    //return ft6x06_Lib_DownloadMain(CTPM_MAIN_FW, fwlen);
	    return ft6x36_Lib_DownloadMain(CTPM_MAIN_FW2,fwlen);	
	}
	return -1;
}


//wangcq327 --- add start
int Check_TP_ID(void)
{
		int data[4] = {0};
		int res = 0;
		int rawdata = 0;
		int tp_vol = 0;
#define AUXADC_TP_VOLTAGE_CHANNEL 0
#ifdef AUXADC_TP_VOLTAGE_CHANNEL
		extern int IMM_GetOneChannelValue(int,int,int*);
		res = IMM_GetOneChannelValue(AUXADC_TP_VOLTAGE_CHANNEL,data,&rawdata);
		if(res < 0)
		{ 
#ifdef BUILD_LK
				printf("[adc_uboot]: get data error\n");
#endif
				return 0;

		}
#endif

		tp_vol = data[0]*1000+data[1]*10;
		//printk("wangcq327 --- data[0]:%d  data[1]:%d  tp_vol:%d\n",data[0],data[1],tp_vol);

		if(tp_vol >1200 && tp_vol < 2400){
			return TP_VOL = 1;
		}
		else if(tp_vol < 1000 && tp_vol > 700){
			return TP_VOL = 0;
		}
		else{
			return TP_VOL = -1;
		}
}
//wangcq327 --- add end

int fts_ctpm_auto_download(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FT_REG_FW_VER;
	u8 uc_tp_fm_ver;
	u8 uc_host_vendor_id = FT_REG_VENDOR_ID;
	u8 uc_tp_vendor_id=0;
	int i_ret;

      //  fts_read_reg(client, FT_REG_VENDOR_ID, &uc_tp_vendor_id);
	//uc_host_vendor_id = fts_ctpm_get_i_file_vendor();
//	if(uc_tp_vendor_id!=uc_host_vendor_id)return 0;
//wangcq327 --- add start	   
	fts_read_reg(client, FT_REG_VENDOR_ID,&uc_tp_vendor_id);
	printk("wangcq327 --- uc_tp_vendor_id == %d\n",uc_tp_vendor_id);
	printk("wangcq327 --- ID_pin_vol == %d \n",Check_TP_ID());

if(1 == TP_VOL){//通豪
	if(uc_tp_vendor_id == 0xd1) {//通豪	
		fts_read_reg(client, FT_REG_FW_VER, &uc_tp_fm_ver);
		uc_host_fm_ver =CTPM_MAIN_FW1[0x90a];// fts_ctpm_get_i_file_ver();
		if(uc_tp_fm_ver >= uc_host_fm_ver)  return -1;
	}//else if(uc_tp_vendor_id != 0x49) {//若不为通豪，且不为汉创，退出，若为汉创，往下升级
	//	return -1;
//	};

}else if(0 == TP_VOL){//汉创
	if(uc_tp_vendor_id == 0x49) {//汉创	
		fts_read_reg(client, FT_REG_FW_VER, &uc_tp_fm_ver);
		uc_host_fm_ver =CTPM_MAIN_FW2[0x90a];// fts_ctpm_get_i_file_ver();
		if(uc_tp_fm_ver >= uc_host_fm_ver)  return -1;
	}//else if(uc_tp_vendor_id != 0xd1){
	//	return -1;
	//};

}else{
	printk("wangcq327 --- ID_pin_vol error!!! \n");
	return -1;
}
//wangcq327 --- add end
       DBG("wangcq327 --- [FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",uc_tp_fm_ver, uc_host_fm_ver);

#if 0 
	   if (/*the firmware in touch panel maybe corrupted */
		uc_tp_fm_ver == FT_REG_FW_VER ||
		/*the firmware in host flash is new, need upgrade */
	     uc_tp_fm_ver < uc_host_fm_ver ||

	    ) 
#endif
	    {
		g_i2c_client = client;
        	FTS_I2c_Read_Function fun_i2c_read = ft5x0x_download_i2c_Read;
        	FTS_I2c_Write_Function fun_i2c_write = ft5x0x_download_i2c_Write;
        	Init_I2C_Read_Func(fun_i2c_read);
        	Init_I2C_Write_Func(fun_i2c_write);
        	 if(ft6x36_IsDownloadMain() < 0)
        	 {
                	#if 1
                		DBG("--------FTS---------download main\n");
                		if(ft6x36_DownloadMain()<0)
                		{
                			DBG("---------FTS---------Download main failed\n");
                		}
                	#endif
        	 } else
        		DBG("--------FTS---------no download main\n");
	}

	return 0;
}

#endif


static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	 
	int retval = TPD_OK;
	char data;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;
	u8 chip_id,i;

	i2c_client = client;
#ifdef FTS_SCAP_TEST
	g_focalclient = client;
#endif

reset_proc:
	//power on, need confirm with SA
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	ft3x07_power_switch(SWITCH_OFF);
	msleep(5);
	printk("TPd enter power on---end-\r\n");
	ft3x07_power_switch(SWITCH_ON);
	printk("TPd enter power on---begin-\r\n");
	msleep(5);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(20);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(50);


	err = i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data);
	DBG("gao_i2c:err %d,data:%d\n", err,data);
	if(err< 0 || data!=0)// reg0 data running state is 0; other state is not 0
	{
		DBG("I2C transfer error, line: %d\n", __LINE__);
#ifdef TPD_RESET_ISSUE_WORKAROUND
        if ( ++reset_count < TPD_MAX_RESET_COUNT )
        {
        	ft3x07_power_switch(SWITCH_OFF);
            goto reset_proc;
        }
#endif
	 	ft3x07_power_switch(SWITCH_OFF);
		return -1; 
	}

	//msleep(200);

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 0);  /* disable auto-unmask */

	msleep(100);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	tpd_load_status = 1;
 
	focaltech_get_upgrade_array();

#ifdef FTS_APK_DEBUG
	ft5x0x_create_apk_debug_channel(client);
#endif
#ifdef TPD_SYSFS_DEBUG
	fts_create_sysfs(i2c_client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(i2c_client) < 0)
		DBG( "%s:[FTS] create fts control iic driver failed\n",__func__);
#endif

	
#ifdef VELOCITY_CUSTOM_FT5206
	if((err = misc_register(&tpd_misc_device)))
	{
		printk("mtk_tpd: tpd_misc_device register failed\n");
	}
#endif

#ifdef TPD_AUTO_UPGRADE
	printk("********************Enter CTP Auto Upgrade********************\n");
	fts_ctpm_auto_upgrade(i2c_client);
#endif

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread))
	{
		retval = PTR_ERR(thread);
		DBG( " failed to create kernel thread: %d\n", retval);
	}

	DBG("FTS Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");

	return 0;
 }

 static int tpd_remove(struct i2c_client *client)
{

#ifdef FTS_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
#endif
#ifdef TPD_SYSFS_DEBUG
	fts_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif

	TPD_DEBUG("TPD removed\n");

   	return 0;
}
 
static int tpd_local_init(void)
{
  	DBG("FTS I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 	#ifdef CONFIG_ARCH_MT6580
		if(tpd->reg == NULL)
		{
		tpd->reg=regulator_get(tpd->tpd_dev,TPD_POWER_SOURCE_CUSTOM); // get pointer to regulator structure
		if (IS_ERR(tpd->reg)) {
			printk("regulator_get() failed!\n");
		}else
		{
			printk("regulator_get() Ok!\n");
		}
		}
#endif
   	if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		DBG("FTS unable to add i2c driver.\n");
      	return -1;
    }
    if(tpd_load_status == 0) 
    {
		ft3x07_power_switch(SWITCH_OFF);
 		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    	DBG("FTS add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }

#if 0 //enable it if support 10 points
	//TINNO_TOUCH_TRACK_IDS <--- finger number
	//TINNO_TOUCH_TRACK_IDS	5
	#if 1//(LINUX_VERSION_CODE > KERNEL_VERSION(3, 8, 0))
		//for linux 3.8
		input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (TPD_MAX_POINTS_10-1), 0, 0);
	#endif
#endif

#ifdef TPD_HAVE_BUTTON     
	//if(TPD_RES_Y > 854)
	{
	    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
	}
	//else
	{
	    //tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local_fwvga);// initialize tpd button data
	}
#endif   

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
    DBG("end %s, %d\n", __FUNCTION__, __LINE__);  
    tpd_type_cap = 1;
    return 0; 
 }

 static void tpd_resume( struct early_suspend *h )
 {
  //int retval = TPD_OK;
  //char data;
#ifdef TPD_PROXIMITY	
	if (tpd_proximity_suspend == 0)
	{
		return;
	}
	else
	{
		tpd_proximity_suspend = 0;
	}
#endif	
 
   	DBG("TPD wake up\n");

#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
#else

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(2);  
   // mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
   // mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
#endif



	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	msleep(200);

#ifdef FTS_GESTRUE
		if(gesture_function_enable == 1)
			{
					fts_write_reg(i2c_client,0xD0,0x00);
				//fts_write_reg(i2c_client,0xD1,0x00);
				//fts_write_reg(i2c_client,0xD2,0x00);
				tpd_halt = 0;
			  //  return;
			}
#endif

	
	tpd_halt = 0;
	/* for resume debug
	if((i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data))< 0)
	{
		TPD_DMESG("resume I2C transfer error, line: %d\n", __LINE__);
	}
	*/
	tpd_up(0,0);
	input_sync(tpd->dev);
	DBG("TPD wake up done\n");
	 //return retval;
 }

 static void tpd_suspend( struct early_suspend *h )
 {
	// int retval = TPD_OK;
	 static char data = 0x3;
	u8 state,i,ret;




 	 tpd_halt = 1;

	DBG("TPD enter sleep\n");
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");
#else
	 mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	 msleep(2);
	 mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	 msleep(200);
	mutex_lock(&i2c_access);
	ret = i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  //TP enter sleep mode
	printk("###########suspend ,ret = %d\n",ret);
	mutex_unlock(&i2c_access);
#endif
	DBG("TPD enter sleep done\n");
	//return retval;
 } 


 static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TPD_NAME,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif		
 };
 /* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	printk("MediaTek FTS touch panel driver init\n");
	i2c_register_board_info(IIC_PORT, &ft5206_i2c_tpd, 1);
//	i2c_register_board_info(0, &ft5206_i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FTS driver failed\n");
	 return 0;
 }
 
 /* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek FTS touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
