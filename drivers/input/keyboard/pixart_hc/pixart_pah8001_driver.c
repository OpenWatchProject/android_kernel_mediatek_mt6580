#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include  <linux/syscalls.h> 
//#include <mach/gpio.h>
#include <linux/gpio.h>
#include  <linux/delay.h>
#include <linux/miscdevice.h>  

#include "led_ctrl.h"
#include "pah8001_reg.h"

#include <linux/mutex.h>
#include <linux/regulator/consumer.h>


#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


typedef struct {
	uint8_t HRD_Data[13] ;
	float MEMS_Data[3] ;
}ppg_mems_data_t;

typedef struct {
	struct i2c_client	*client;
	struct input_dev *pah8001_input_dev;
	struct delayed_work x_work;	//for PPR, HRV
	struct device *pah8001_device; 
	bool run_ppg ;
	ppg_mems_data_t ppg_mems_data ;
} pah8001_data_t;


int alsps_init_flag =-1; // 0<==>OK -1 <==> fail


#define pah8001_name "pixart_pah8001"

#define MTK_PLATFORM
#ifdef MTK_PLATFORM
#define PIXART_DEV_NAME     pah8001_name
#define PIXART_I2C_ADDRESS	0x33
static struct i2c_board_info __initdata i2c_pixart={ I2C_BOARD_INFO(PIXART_DEV_NAME, PIXART_I2C_ADDRESS)};
#endif

static  pah8001_data_t pah8001data;

unsigned char pah8001_write_reg(unsigned char addr, unsigned char data);
unsigned char pah8001_read_reg(unsigned char addr, unsigned char *data);
static void pah8001_ppg(void);

void pah8001_power_down(u8 yes)
{
	u8 data = 0 ;
	//Power Down
	//Bank0
	pah8001_write_reg(0x7f, 0);
	//ADDR6, Bit3 = 1
	pah8001_read_reg(0x06, &data); 
	if(yes)
		data |= 0x08 ;
	else
		data &= (~0x08) ;
	pah8001_write_reg(0x06, data); 	
}

void PAH8001_led_ctrl(uint8_t touch)
{			
	led_ctrl(touch);

}
/////****************************
static int pah8001_i2c_write(u8 reg, u8 *data, int len)
{
	u8  buf[20];
	int rc;
	int ret = 0;
	int i;

	buf[0] = reg;
	if (len >= 20) {
		printk("%s (%d) : FAILED: buffer size is limitted(20) %d\n", __func__, __LINE__, len);
		dev_err(&pah8001data.client->dev, "pah8001_i2c_write FAILED: buffer size is limitted(20)\n");
		return -1;
	}

	for( i=0 ; i<len; i++ ) {
		buf[i+1] = data[i];
	}
 
	rc = i2c_master_send(pah8001data.client, buf, len+1);  // Returns negative errno, or else the number of bytes written.

	if (rc != len+1) {
		printk("%s (%d) : FAILED: writing to reg 0x%x\n", __func__, __LINE__, reg);

		ret = -1;
	}

	return ret;
}

static int pah8001_i2c_burst_read(u8 reg, u8 *data, u8 len)
{
	u8  buf[256];
	int rc;		
	
	buf[0] = reg;
	
	rc = i2c_master_send(pah8001data.client, buf, 1);    // Returns negative errno, or else the number of bytes written.
	if (rc != 1) {
		printk("%s (%d) : FAILED: writing to address 0x%x\n", __func__, __LINE__, reg);
		return -1;
	}	
	
	rc = i2c_master_recv(pah8001data.client, data, len);
	if (rc != len) {
		printk("%s (%d) : FAILED: reading data %d\n", __func__, __LINE__, rc);
		return -1;
	}	
	
	return 0;
}

static int pah8001_i2c_read(u8 reg, u8 *data)
{   

	u8  buf[20];
	int rc;		
	
	buf[0] = reg;
	
	rc = i2c_master_send(pah8001data.client, buf, 1);  //If everything went ok (i.e. 1 msg transmitted), return #bytes  transmitted, else error code.   thus if transmit is ok  return value 1
	if (rc != 1) {
		printk("%s (%d) : FAILED: writing to address 0x%x\n", __func__, __LINE__, reg);
		return -1;
	}	
	
	rc = i2c_master_recv(pah8001data.client, buf, 1);   // returns negative errno, or else the number of bytes read
	if (rc != 1) {
		printk("%s (%d) : FAILED: reading data\n", __func__, __LINE__);
		return -1;
	}	
		
	*data = buf[0] ;		
	return 0;
}


unsigned char pah8001_write_reg(unsigned char addr, unsigned char data)
{
	int ret =	pah8001_i2c_write(addr, &data, 1) ;
	
	if(ret != 0)
		return false;
	else
		return true;
}
unsigned char pah8001_read_reg(unsigned char addr, unsigned char *data)
{
	int ret =	pah8001_i2c_read(addr, data) ;  

	if(ret != 0)
		return false;
	else
		return true;
}
unsigned char pah8001_burst_read_reg(unsigned char addr, unsigned char *data, unsigned int length)
{
	int ret = pah8001_i2c_burst_read(addr, data, length); 
	
	if(ret != 0)      
		return false;
	else
		return true;
}

static int ppg_init(void)
{
	int i=0;
	int bank = 0;
	u8 data = 0 ;
	
	for(i = 0; i < INIT_PPG_REG_ARRAY_SIZE;i++){
		if(init_ppg_register_array[i][0] == 0x7F)
			bank = init_ppg_register_array[i][1];
		
		if((bank == 0) && (init_ppg_register_array[i][0] == 0x17) )
		{
			//read and write bit7=1
				pah8001_read_reg(0x17, &data);
				data |= 0x80 ;
				pah8001_write_reg(0x17,data);
		}
		else
		{
			pah8001_write_reg(init_ppg_register_array[i][0],init_ppg_register_array[i][1]);
		}
	}		
	pah8001_power_down(1);
	
	return 0;
}

static int pah8001_init_reg(void)
{
	u8 data0 = 0, data1 = 0;
	int ret = -1;
	
	printk("%s (%d) : pah8001 register initialize\n", __func__, __LINE__);

	pah8001_write_reg(127, 0);		
	pah8001_read_reg(0, &data0);	
	pah8001_read_reg(1, &data1) ;
	
	printk("%s (%d) : ADDR0 = 0x%x, ADDR1 = 0x%x.\n", __func__, __LINE__, data0, data1);     // ADDR0 = 0x30, ADDR1= 0xd3
	if( (data0 != 0x30 ) || ((data1&0xF0) != 0xD0) )
	{
		return -1 ;
	}

	ret = ppg_init();
	if( ret )
	{
		return ret ;
	}

	printk("%s (%d) : pah8001 initialize register.\n", __func__, __LINE__);
	
	return 0;
}

static void pah8001_ppg(void)
{
	static unsigned long volatile start_jiffies = 0, end_jiffies ;
	static u8 Frame_Count = 0 ;
	u8 touch_flag = 0 ;
	u8 data ;

	if(pah8001data.run_ppg)	
	{
		pah8001_write_reg(127, 0);
		pah8001_read_reg(0x59, &touch_flag);
		
    touch_flag &= 0x80  ;		

		PAH8001_led_ctrl(touch_flag);
		
		pah8001_write_reg(127, 1);
		pah8001_read_reg(0x68, &data);
		pah8001data.ppg_mems_data.HRD_Data[0] = data & 0x0f ;
		
		if(pah8001data.ppg_mems_data.HRD_Data[0] == 0)
		{
			pah8001_write_reg(127, 0);
			msleep(10);
		}
		else
		{		
			pah8001_burst_read_reg(0x64, &(pah8001data.ppg_mems_data.HRD_Data[1]), 4);
			pah8001_burst_read_reg(0x1A, &(pah8001data.ppg_mems_data.HRD_Data[5]), 3);
			pah8001data.ppg_mems_data.HRD_Data[8] = Frame_Count++;
			end_jiffies = jiffies ;
			pah8001data.ppg_mems_data.HRD_Data[9] = jiffies_to_msecs(end_jiffies - start_jiffies) ;
			start_jiffies = end_jiffies ;
			pah8001data.ppg_mems_data.HRD_Data[10] = get_led_current_change_flag() ;
	
			
			pah8001data.ppg_mems_data.HRD_Data[11] = touch_flag ;
			pah8001data.ppg_mems_data.HRD_Data[12] = pah8001data.ppg_mems_data.HRD_Data[6] ;

			input_report_abs(pah8001data.pah8001_input_dev, ABS_X, *(uint32_t *)(pah8001data.ppg_mems_data.HRD_Data));
			input_report_abs(pah8001data.pah8001_input_dev, ABS_Y, *(uint32_t *)(pah8001data.ppg_mems_data.HRD_Data + 4));
			input_report_abs(pah8001data.pah8001_input_dev, ABS_Z, *(uint32_t *)(pah8001data.ppg_mems_data.HRD_Data + 8));
			input_sync(pah8001data.pah8001_input_dev);		
			printk(">>>%s (%d)(%d)(%d)(%d) \n", __func__,pah8001data.ppg_mems_data.HRD_Data[0], pah8001data.ppg_mems_data.HRD_Data[1], pah8001data.ppg_mems_data.HRD_Data[2], pah8001data.ppg_mems_data.HRD_Data[3]);
//			printk(">>>%s (%d)(%d)(%d)(%d) \n", __func__,pah8001data.ppg_mems_data.HRD_Data[4], pah8001data.ppg_mems_data.HRD_Data[5], pah8001data.ppg_mems_data.HRD_Data[6], pah8001data.ppg_mems_data.HRD_Data[7]);
//			printk(">>>%s (%d)(%d)(%d)(%d) \n", __func__,pah8001data.ppg_mems_data.HRD_Data[8], pah8001data.ppg_mems_data.HRD_Data[9], pah8001data.ppg_mems_data.HRD_Data[10], pah8001data.ppg_mems_data.HRD_Data[11]);
			
		}		
	}
}

static void pah8001_x_work_func(struct work_struct *work)
{	
	pah8001_power_down(0);
//	printk(">>>%s (%d)\n", __func__, __LINE__);	
	while(pah8001data.run_ppg)
	{
		pah8001_ppg();
	}
	pah8001_power_down(1);
//	printk("<<< %s (%d)\n", __func__, __LINE__);
}

static int pah8001_input_open(struct input_dev *dev)
{
	printk(">>> %s (%d) \n", __func__, __LINE__);
	return 0;
}

static void pah8001_input_close(struct input_dev *dev)
{
	printk(">>> %s (%d) \n", __func__, __LINE__);
}

static int pah8001_init_input_data(void)
{
	int ret = 0;

	printk("%s (%d) : initialize data\n", __func__, __LINE__);
	
	pah8001data.pah8001_input_dev = input_allocate_device();
	
	if (!pah8001data.pah8001_input_dev) {
		printk("%s (%d) : could not allocate mouse input device\n", __func__, __LINE__);
		return -ENOMEM;
	}
	pah8001data.pah8001_input_dev->evbit[0] = BIT_MASK(EV_ABS);
	pah8001data.pah8001_input_dev->absbit[0] = BIT_MASK(ABS_X) | BIT_MASK(ABS_Y)| BIT_MASK(ABS_Z)| BIT_MASK(ABS_RX) | BIT_MASK(ABS_RY);

	input_abs_set_max(pah8001data.pah8001_input_dev, ABS_X, 0xffffffff);
	input_abs_set_max(pah8001data.pah8001_input_dev, ABS_Y, 0xffffffff);
	input_abs_set_max(pah8001data.pah8001_input_dev, ABS_Z, 0xffffffff);
	input_abs_set_max(pah8001data.pah8001_input_dev, ABS_RX, 0xffffffff);
	input_abs_set_max(pah8001data.pah8001_input_dev, ABS_RY, 0xffffffff);

	input_set_drvdata(pah8001data.pah8001_input_dev, &pah8001data);
	pah8001data.pah8001_input_dev->name = "Pixart PPG8001";	

	pah8001data.pah8001_input_dev->open = pah8001_input_open;
	pah8001data.pah8001_input_dev->close = pah8001_input_close;
	
	ret = input_register_device(pah8001data.pah8001_input_dev);
	if (ret < 0) {
		input_free_device(pah8001data.pah8001_input_dev);
		printk("%s (%d) : could not register input device\n", __func__, __LINE__);	
		return ret;
	}
	
	return 0;	
}

/******************************************************************************/

static ssize_t pah8001_read(struct file *filp,char *buf,size_t count,loff_t *l)
{
	printk(">>>%s (%d) \n", __func__, __LINE__);
	return 0;
}

static ssize_t pah8001_write(struct file *filp,const char *buf,size_t count,loff_t *f_ops)
{
	printk(">>>%s (%d) \n", __func__, __LINE__);
	return count;
}

static long pah8001_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
//static int pah8001_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	printk(">>> %s (%d) \n", __func__, __LINE__);	
	//cancel_delayed_work_sync(&pah8001data.work);
	return 0;
}

static int pah8001_open(struct inode *inode, struct file *filp)
{
	printk(">>>%s (%d) \n", __func__, __LINE__);
	return 0;
}

static int pah8001_release(struct inode *inode, struct file *filp)
{
	printk(">>> %s (%d) \n", __func__, __LINE__);

	return 0;
}
static struct file_operations pah8001_fops = 
{
	owner	:	THIS_MODULE,
	read	:	pah8001_read,
	write	:	pah8001_write,
	//ioctl	:	pah8001_ioctl,
	unlocked_ioctl	:	pah8001_ioctl,
	open	:	pah8001_open,
	release	:	pah8001_release,
};
/*----------------------------------------------------------------------------*/
struct miscdevice pixart_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = pah8001_name,
	.fops = &pah8001_fops,
};
static ssize_t pah8001_enable_store(struct device* dev, 
                                   struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s (%d) :\n", __func__, __LINE__);
	if((buf != NULL) && ( (1 == buf[0]) || ('1' == buf[0]) ))
	{
		printk("Enable!!\n");		
		pah8001data.run_ppg = true;
		schedule_delayed_work(&pah8001data.x_work, msecs_to_jiffies(100));
		//pah8011_start();
	}
	else
	{
		printk("Disable!!\n");	
		pah8001data.run_ppg = false ;
	}
	return count;
}                       

static ssize_t pah8001_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	//cat rw_reg	
	printk("%s (%d) : \n", __func__, __LINE__);
	
	return 0; 
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO , pah8001_enable_show, pah8001_enable_store);
static struct device_attribute *pah8001_attr_list[] =
{
	&dev_attr_enable,
};


/*----------------------------------------------------------------------------*/
static int pah8001_create_attr(struct device *dev) 
{
	int idx, err = 0;
	int num = (int)(sizeof(pah8001_attr_list)/sizeof(pah8001_attr_list[0]));
	if(!dev)
	{
		return -EINVAL;
	}	

	for(idx = 0; idx < num; idx++)
	{
		if((err = device_create_file(dev, pah8001_attr_list[idx])))
		{            
			printk("device_create_file (%s) = %d\n", pah8001_attr_list[idx]->attr.name, err);        
			break;
		}
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int pah8001_delete_attr(struct device *dev)
{
	
	int idx ,err = 0;
	int num = (int)(sizeof(pah8001_attr_list)/sizeof(pah8001_attr_list[0]));
	if (!dev)
	{
		return -EINVAL;
	}
	

	for (idx = 0; idx < num; idx++)
	{
		device_remove_file(dev, pah8001_attr_list[idx]);
	}	

	return err;
} 

static int pah8001_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;    	
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct regulator *reg = NULL;		 
	reg = regulator_get(&(client->dev), "VGP1");

	printk("%s (%d) \n", __func__, __LINE__);
 
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		return err;
	}	
	regulator_set_voltage(reg, 3300000, 3300000);
	regulator_enable(reg);	//PDN	
	mt_set_gpio_out(GPIO_HEART_PDN_PIN, GPIO_OUT_ZERO);	//reset	
	mt_set_gpio_mode(GPIO_HEART_RESET_PIN, 0);	
	mt_set_gpio_dir(GPIO_HEART_RESET_PIN, GPIO_DIR_OUT);	
	mt_set_gpio_out(GPIO_HEART_RESET_PIN, GPIO_OUT_ONE);	
	mdelay(10);	
	mt_set_gpio_out(GPIO_HEART_RESET_PIN, GPIO_OUT_ZERO);	
	mdelay(10);	
	mt_set_gpio_out(GPIO_HEART_RESET_PIN, GPIO_OUT_ONE);	
	mdelay(10);

	pah8001data.client = client;
	if((err = misc_register(&pixart_device)))
		{
			printk("pixart_device register failed\n");
			return err;
		} 
	
	pah8001data.pah8001_device = pixart_device.this_device;
	
	if( (err = pah8001_create_attr(pah8001data.pah8001_device)) )
	{
		printk("create attribute err = %d\n", err);
		return err;
	}  
	
	err = pah8001_init_reg();
	if (err < 0) {
     return err;
	}

	INIT_DELAYED_WORK(&pah8001data.x_work, pah8001_x_work_func);

	err = pah8001_init_input_data();
	if (err < 0) {
		return err;
	}
		     	
	pah8001data.run_ppg = false;	
	alsps_init_flag = 0;
	return err;	

}


static int pah8001_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int pah8001_suspend(struct device *dev)
{   printk("%s (%d) : pah8001 suspend \n", __func__, __LINE__);
	return 0;
}

static int pah8001_resume(struct device *dev)
{
	printk("%s (%d) : pah8001 resume \n", __func__, __LINE__);
	return 0;
}

static const struct i2c_device_id pah8001_device_id[] = {
	{"pixart_pah8001", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pah8001_device_id);

static const struct dev_pm_ops pah8001_pm_ops = {
	.suspend = pah8001_suspend,
	.resume = pah8001_resume
};


static struct of_device_id pixart_pah8001_match_table[] = {
	{ .compatible = "pixart,pah8001",},
	{ },
}; 

static struct i2c_driver pah8001_i2c_driver = {
	.driver = {
		   .name = pah8001_name,
		   .owner = THIS_MODULE,
		   .pm = &pah8001_pm_ops,
		   .of_match_table = pixart_pah8001_match_table,
		   },
	.probe = pah8001_i2c_probe,
	.remove = pah8001_i2c_remove,
	.id_table = pah8001_device_id,
};

static int __init pah8001_init(void)
{
	printk("%s (%d) :init module\n", __func__, __LINE__);
	
	#ifdef MTK_PLATFORM	
	if(1)
	{		
		int ret ;
		if( (ret = i2c_register_board_info(2, &i2c_pixart, 1)) < 0)
		{
			printk(KERN_WARNING "i2c_register_board_info fail\n");
			return ret;
		}
	}
	#endif

	return i2c_add_driver(&pah8001_i2c_driver);
}




static void __exit pah8001_exit(void)
{
	printk("%s (%d) : exit module\n", __func__, __LINE__);	
	misc_register(&pixart_device);
	pah8001_delete_attr(pah8001data.pah8001_device);
	i2c_del_driver(&pah8001_i2c_driver);
}

module_init(pah8001_init);
module_exit(pah8001_exit);
MODULE_AUTHOR("pixart");
MODULE_DESCRIPTION("pixart pah8001 driver");
MODULE_LICENSE("GPL");
  



