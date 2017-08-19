/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
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


#include <cust_eint.h>

#include "tpd_custom_it7260.h"
#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif
#include <linux/wakelock.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include <mach/wd_api.h>
#include <linux/watchdog.h>
#include <mach/mt_wdt.h>

//#define __IT7260_DEBUG_A158_JFSS__ //lshun use tp for A152
#define s32 int
//#define I2C_SUPPORT_RS_DMA 
//extern kal_bool upmu_chr_det(upmu_chr_list_enum chr); // remove

#define ABS(x)				((x<0)?-x:x)
#define TPD_OK				0
#define TPD_FAIL 						(-1)

//#define MAX_BUFFER_SIZE		144
#define CTP_NAME			"IT7260"
#define IOC_MAGIC			'd'
#define IOCTL_SET 			_IOW(IOC_MAGIC, 1, struct ioctl_cmd168)
#define IOCTL_GET 			_IOR(IOC_MAGIC, 2, struct ioctl_cmd168)
#define HAS_8_BYTES_LIMIT 
extern struct tpd_device *tpd;

static int tpd_flag = 0;
static int tpd_halt=0;
#ifdef I2C_SUPPORT_RS_DMA
static u8 *I2CDMABuf_va = NULL;
static u32 I2CDMABuf_pa = NULL;
#endif
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
#ifdef DOUBLE_CLICK_WAKE
extern  int gesture_function_enable;
#endif
enum wk_wdt_type {
    WK_WDT_LOC_TYPE,
    WK_WDT_EXT_TYPE,
    WK_WDT_LOC_TYPE_NOLOCK,
    WK_WDT_EXT_TYPE_NOLOCK,
};
extern void mtk_wdt_restart(enum wk_wdt_type type);
long FWversion;


#include "mt_pm_ldo.h"
  extern struct tpd_device *tpd;
static  struct regulator *tempregs = NULL;
static int power_flag=0;// 0 power off,default, 1 power on
#define POWER_OFF                  0
#define POWER_ON                   1
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


extern int tpd_load_status;


static  void it7260_power_switch(s32 state)
{
	int ret = 0;
	switch (state) {
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
} 


 #if 0
static void mtk_kick_wdt(void)
{
    mtk_wdt_restart(WK_WDT_LOC_TYPE_NOLOCK);
    mtk_wdt_restart(WK_WDT_EXT_TYPE_NOLOCK);
}
#endif
extern unsigned char cfg_rawData[];
extern unsigned char rawData[];
unsigned int fw_size = sizeof(rawData);
unsigned int config_size = sizeof(cfg_rawData);

typedef unsigned char		BOOL;
typedef unsigned char		BYTE;

typedef unsigned char		uint8;
typedef unsigned short		WORD;
typedef unsigned long int	uint32;
typedef unsigned int		UINT;

typedef signed char			int8;
typedef signed short		int16;
typedef signed long int		int32;

//#define MAX_BUFFER_SIZE 256


bool fnFirmwareDownload(unsigned int unFirmwareLength, u8* pFirmware, unsigned int unConfigLength, u8* pConfig);

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static bool waitCommandDone(void);
static int tpd_print_version(void);
static int tpd_FW_version(void);
static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
static int tpd_i2c_write(struct i2c_client *client, uint8_t *buf, int len);
static int tpd_i2c_read(struct i2c_client *client, uint8_t *buf, int len , uint8_t addr);
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern int mtk_wdt_enable(enum wk_wdt_en en);

//lshun modify 20130615
extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

static struct i2c_client *i2c_client = NULL;

static const struct i2c_device_id tpd_i2c_id[] ={{CTP_NAME,0},{}}; // {{"mtk-tpd",0},{}}; 

static unsigned short force[] = {0, 0x8C, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};

static struct i2c_board_info __initdata it7260_i2c_tpd={ I2C_BOARD_INFO(CTP_NAME, (0x8c>>1))};

struct i2c_driver tpd_i2c_driver = {
	.driver = {
	.name = CTP_NAME,
	.owner = THIS_MODULE,
	},
    .probe			= tpd_i2c_probe,   
    .remove = tpd_i2c_remove,
    .detect			= tpd_i2c_detect,                           
    .driver.name 	= CTP_NAME, //"mtk-tpd", 
    .id_table 		= tpd_i2c_id,                             
    .address_list 	= forces,                        
}; 

struct ite7260_data {
	rwlock_t lock;
	unsigned short bufferIndex;
	unsigned short length;
	unsigned short buffer[MAX_BUFFER_SIZE];
};

struct ioctl_cmd168 {
	unsigned short bufferIndex;
	unsigned short length;
	unsigned short buffer[MAX_BUFFER_SIZE];
};

static ssize_t IT7260_upgrade_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk("[IT7260] %s():\n", __func__);
	int bitsmask = 0;

	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	bitsmask = tpd_print_version();
	if((bitsmask / 2)==1)
	{
		printk("FirmwareDownload");
		if(!fnFirmwareDownload(fw_size,rawData,0,NULL))
		{
			printk("FirmwareDownload_fail");
		}
	}
	if((bitsmask % 2)==1)
	{
		printk("ConfigDownload");
		if(!fnFirmwareDownload(0,NULL,config_size,cfg_rawData))
		{
			printk("ConfigDownload_fail");
		}
	}
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	//fnFirmwareDownload(fw_size,rawData,config_size,cfg_rawData);
	return count;
}

static ssize_t IT7260_tpfwver_show(struct device *dev,struct device_attribute *attr,char *buf)
{
//get chip FW version
	uint8_t pucCommandBuffer[8], verFw[4]={0};
	ssize_t num_read_chars = 0;
	int ret = 0;

waitCommandDone();

    pucCommandBuffer[0] = 0x20;
    pucCommandBuffer[1] = 0xE1;
    pucCommandBuffer[2] = 0x04;
    pucCommandBuffer[3] = 0x01;
    pucCommandBuffer[4] = 0x08;
    pucCommandBuffer[5] = 0x00;
    pucCommandBuffer[6] = 0x00;
    pucCommandBuffer[7] = 0xD8;	

	ret = tpd_i2c_write(i2c_client, pucCommandBuffer, 8);
  	msleep(10);
waitCommandDone();
	ret = tpd_i2c_read(i2c_client, &verFw[0], 4, 0xA0);
		
	if (ret < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE,"get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02x %02x %02x %02x \n", verFw[0], verFw[1], verFw[2], verFw[3]);
	
	return num_read_chars;
}

static ssize_t IT7260_configversion_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	//get chip config version
	ssize_t num_read_chars = 0;
	int ret = 0;
	char verCfg[10] = {0};

waitCommandDone();

 	 verCfg[0] = 0x20;
	 verCfg[1] = 0x01;
  	 verCfg[2] = 0x06;
  	 ret = tpd_i2c_write(i2c_client, verCfg, 3);
 	 if(ret != 3){
 	       printk("[mtk-tpd] i2c write communcate error in getting Cfg version : 0x%x\n", ret);
	  }
 	  msleep(10);	

waitCommandDone();

	  ret = tpd_i2c_read(i2c_client, &verCfg[0], 7, 0xA0);

	if (ret < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE,"get tp config version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02x %02x %02x %02x \n", verCfg[1], verCfg[2], verCfg[3], verCfg[4]);
	
	return num_read_chars;
}

static DEVICE_ATTR(upgrade, 0666, NULL, IT7260_upgrade_store);	///sys/devices/platform/mt-i2c.1/i2c-1/1-0046
static DEVICE_ATTR(fwversion, 0777, IT7260_tpfwver_show, NULL);
static DEVICE_ATTR(configversion, 0777, IT7260_configversion_show, NULL);

static struct attribute *it7260_attributes[] = {
	&dev_attr_upgrade.attr,
	&dev_attr_fwversion.attr,
	&dev_attr_configversion.attr,
	NULL
};
static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};


int ite7260_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	struct ite7260_data *dev = filp->private_data;
	int retval = 0;
	int i;
	unsigned char ucQuery;
	unsigned char buffer[MAX_BUFFER_SIZE];
	struct ioctl_cmd168 data;
	unsigned char datalen;
	unsigned char ent[] = {0x60, 0x00, 0x49, 0x54, 0x37, 0x32};
	unsigned char ext[] = {0x60, 0x80, 0x49, 0x54, 0x37, 0x32};

	pr_info("=ite7260_ioctl=\n");
	memset(&data, 0, sizeof(struct ioctl_cmd168));

	switch (cmd) {
	case IOCTL_SET:
		pr_info("=IOCTL_SET=\n");
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		buffer[0] = (unsigned char) data.bufferIndex;
		pr_info("%.2X ", buffer[0]);
		for (i = 1; i < data.length + 1; i++) {
			buffer[i] = (unsigned char) data.buffer[i - 1];
			pr_info("%.2X ", buffer[i]);
		}
		if (!memcmp(&(buffer[1]), ent, sizeof(ent))) {

			pr_info("Disabling IRQ.\n");
			//disable_irq(gl_ts->client->irq);
			tpd_halt = 1;
			mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
		}

		if (!memcmp(&(buffer[1]), ext, sizeof(ext))) {

			pr_info("Enabling IRQ.\n");

			//enable_irq(gl_ts->client->irq);
			tpd_halt = 0;
				mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

		}

		datalen = (unsigned char) (data.length + 1);
		printk("IOCTL_SET: datalen=%d.\n",datalen);
		retval = tpd_i2c_write(i2c_client, &buffer[0], datalen);
		pr_info("SET:retval=%x\n", retval);
		retval = 0;
		break;

	case IOCTL_GET:
		pr_info("=IOCTL_GET=\n");
		if (!access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}

		retval = tpd_i2c_read(i2c_client, (unsigned char*) buffer, (unsigned char) data.length, (unsigned char) data.bufferIndex);

		pr_info("GET:retval=%x\n", retval);
		retval = 0;
		for (i = 0; i < data.length; i++) {
			data.buffer[i] = (unsigned short) buffer[i];
		}
		pr_info("GET:bufferIndex=%x, dataLength=%d, buffer[0]=%x, buffer[1]=%x, buffer[2]=%x, buffer[3]=%x\n", data.bufferIndex, data.length, buffer[0], buffer[1], buffer[2], buffer[3]);

		if ( copy_to_user((int __user *)arg, &data, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		break;

	default:
		retval = -ENOTTY;
		break;
	}

	done:
	pr_info("DONE! retval=%d\n", retval);
	return (retval);
}

int ite7260_open(struct inode *inode, struct file *filp) {
	int i;
	struct ite7260_data *dev;

	pr_info("=ite7260_open=\n");
	dev = kmalloc(sizeof(struct ite7260_data), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	/* initialize members */
	rwlock_init(&dev->lock);
	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		dev->buffer[i] = 0xFF;
	}

	filp->private_data = dev;

	return 0; /* success */
}

int ite7260_close(struct inode *inode, struct file *filp) {
	struct ite7260_data *dev = filp->private_data;

	pr_info("=ite7260_close=\n");
	if (dev) {
		kfree(dev);
	}

	return 0; /* success */
}

struct file_operations ite7260_fops = {
	.owner		= THIS_MODULE,
	.open		= ite7260_open,
	.release 	= ite7260_close,
	//.ioctl		= ite7260_ioctl,
	.unlocked_ioctl = ite7260_ioctl,
};
		
static struct miscdevice ctp_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= CTP_NAME,
	.fops	= &ite7260_fops,
};



static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
  //  strcpy(info->type, "mtk-tpd"); 
  strcpy(info->type, CTP_NAME); 
    return 0;
}
static int tpd_i2c_write(struct i2c_client *client, uint8_t *buf, int len)
{
    int ret = 0;
#ifdef I2C_SUPPORT_RS_DMA
    int i = 0;
    printk("start tpd_i2c_write len=%d.\n",len);
    for(i = 0 ; i < len; i++){
        I2CDMABuf_va[i] = buf[i];
    }
    
    if(len < 8){
        client->addr = client->addr & I2C_MASK_FLAG;
        return i2c_master_send(client, buf, len);
    }else{
        client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
        return i2c_master_send(client, I2CDMABuf_pa, len);
    } 
#else
	i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
    ret = i2c_master_send(i2c_client, &buf[0], len);
	if(ret<0)
		printk("%s error\n",__func__);
	
	return ret;
#endif
    return ret;
}

int upgrade_thread_kthread(void *x)
{
	fnFirmwareDownload(fw_size,rawData,config_size,cfg_rawData);
	return 0;
}


static int tpd_i2c_read(struct i2c_client *client, uint8_t *buf, int len , uint8_t addr)
{
    int ret = 0;
	//printk("start tpd_i2c_read len=%d.\n",len);
#ifdef I2C_SUPPORT_RS_DMA
    int i = 0;
    if(len <= 8){
        buf[0] = addr;
        i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
        ret = i2c_master_send(i2c_client, &buf[0], (len << 8 | 1));
    }else{
		
		/**
		struct i2c_msg msg;
		
		i2c_smbus_write_byte(i2c_client, addr);
		msg.flags = i2c_client->flags & I2C_M_TEN;
		msg.timing = 100;
		msg.flags |= I2C_M_RD;
		msg.len = len;
		msg.ext_flag = i2c_client->ext_flag;
		if(len <= 8)
		{
			msg.addr = i2c_client->addr & I2C_MASK_FLAG;
			msg.buf = buf;
			ret = i2c_transfer(i2c_client->adapter, &msg, 1);
			return (ret == 1)? len : ret;
		}else{
			client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG ;
			msg.addr = (i2c_client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG;
			msg.buf = I2CDMABuf_pa;
			ret = i2c_transfer(i2c_client->adapter, &msg, 1);
			if(ret < 0)
			{
				return ret;
			}
			for(i = 0; i < len; i++)
			{
			
				buf[i] = I2CDMABuf_va[i];
			}
			return ret;
		}
		*/
		//i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
		
		//client->addr = client->addr & I2C_MASK_FLAG ;//| I2C_WR_FLAG | I2C_RS_FLAG;
		/**
		unsigned char buffer[256];
		i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
		do{
			int times = len >> 3;
			int last_len = len & 0x7;
			int ii=0;

			for(ii=0;ii<times;ii++)
			{
				//ret = i2c_smbus_read_i2c_block_data(i2c_client,addr+ (ii<<2), len, (buf+ (ii<<2)));
				buf[ii<<3]=addr+ii<<3;
				
				ret = i2c_master_send(i2c_client, &buf[ii<<3], (8<<8 | 1));
				if(ret < 0)
				{
					printk("read error 380.\n");
					break;
				}
				printk("line 383 ret =%d",ret);
				msleep(20);
			}
			
			if(last_len > 0)
			{
				//ret = i2c_smbus_read_i2c_block_data(i2c_client,addr+ (ii<<2), last_len, (buf+ (ii<<2)));
//				*(buf+ii<<3)=addr+ii<<3;
				buf[ii<<3]=addr+ii<<3;
				ret=i2c_master_send(i2c_client,&buf[ii<<3], (last_len << 8 | 1));
				printk("line 392 ret =%d",ret);
				if(ret<0)
				{
					printk("read error 392.\n");
				}
			}
		}while(0);
		*/
		/**
		I2CDMABuf_va[0] = addr;
    	I2CDMABuf_va[9] = 0xFF;
    	I2CDMABuf_va[8] = 0xFF;
		
        
        //ret = i2c_master_recv(client, I2CDMABuf_pa, ((len+1) << 8 | 1));
		ret = i2c_master_recv(client, I2CDMABuf_pa, len);
    
        if(ret < 0){
			printk("%s:i2c read error.\n", __func__);
            return ret;
        }
    
        for(i = 0; i < len; i++){
            buf[i] = I2CDMABuf_va[i];
        }
		*/
    }
#else
    buf[0] = addr;
    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
    ret = i2c_master_send(i2c_client, &buf[0], (len << 8 | 1));
#endif

    return ret;
}
static int tpd_print_version(void) {
	int i=0;
	int sum = 0;
#ifdef I2C_SUPPORT_RS_DMA
    char buffer[9];
#else
    char buffer[8];
	char verFw[4];
	char verCfg[8];
#endif
	uint8_t pucCommandBuffer[8];
    int ret = -1;
    printk("[mtk-tpd] enter tpd_print_version .\n");
//get chip FW version
	waitCommandDone();

	printk("[mtk-tpd] tpd_print_version 413line.\n");
    pucCommandBuffer[0] = 0x20;
    pucCommandBuffer[1] = 0xE1;
    pucCommandBuffer[2] = 0x04;
    pucCommandBuffer[3] = 0x01;
    pucCommandBuffer[4] = 0x08;
    pucCommandBuffer[5] = 0x00;
    pucCommandBuffer[6] = 0x00;
    pucCommandBuffer[7] = 0xD8;
    ret = tpd_i2c_write(i2c_client, pucCommandBuffer, 8);
    msleep(10);  
    waitCommandDone();
    ret = tpd_i2c_read(i2c_client, &verFw[0], 4, 0xA0);

    printk("[mtk-tpd] ITE7260 Touch Panel Firmware Version %x %x %x %x \n", 
            verFw[0], verFw[1], verFw[2], verFw[3]); 
//get chip config version	
	msleep(10);
	waitCommandDone();
	printk("[mtk-tpd] tpd_print_version 413line.\n");
    buffer[0] = 0x20;
    buffer[1] = 0x01;
    buffer[2] = 0x06;
    ret = tpd_i2c_write(i2c_client, buffer, 3);
    msleep(10);
    waitCommandDone();
	ret = tpd_i2c_read(i2c_client, &verCfg[0], 8, 0xA0);
    printk("[mtk-tpd] ITE7260 Touch Panel config Version %x %x %x %x \n", 
            verCfg[1], verCfg[2], verCfg[3], verCfg[4]); 
	
	
	if ((verCfg[1] != cfg_rawData[config_size-8]) || (verCfg[2] != cfg_rawData[config_size-7]) || (verCfg[3] != cfg_rawData[config_size-6]) || (verCfg[4] != cfg_rawData[config_size-5]))
		{
		sum += 1;
		}
	if((verFw[0] != rawData[8]) || (verFw[1] != rawData[9]) || (verFw[2] != rawData[10]) || (verFw[3] != rawData[11]))
		{
		sum += 2;
		}
		
		printk("print sum = %d\n", sum); 
		return sum;
}
static int tpd_FW_version(void) {
	int i=0;
	long sum = 0;
#ifdef I2C_SUPPORT_RS_DMA
    char buffer[9];
#else
    char buffer[8];
	char verFw[4];
#endif
	uint8_t pucCommandBuffer[8];
    int ret = -1;
    printk("[mtk-tpd] enter tpd_print_version .\n");
//get chip FW version
	waitCommandDone();

	printk("[mtk-tpd] tpd_print_version 413line.\n");
    pucCommandBuffer[0] = 0x20;
    pucCommandBuffer[1] = 0xE1;
    pucCommandBuffer[2] = 0x04;
    pucCommandBuffer[3] = 0x01;
    pucCommandBuffer[4] = 0x08;
    pucCommandBuffer[5] = 0x00;
    pucCommandBuffer[6] = 0x00;
    pucCommandBuffer[7] = 0xD8;
    ret = tpd_i2c_write(i2c_client, pucCommandBuffer, 8);
    msleep(10);  
    waitCommandDone();
    ret = tpd_i2c_read(i2c_client, &verFw[0], 4, 0xA0);
    sum = (verFw[0] << 24) | (verFw[1] << 16) | (verFw[2] << 8) | (verFw[3]);
	return sum;
}
static void IT7260_upgrade(void)
{
	printk("[IT7260] %s():\n", __func__);
	int bitsmask = 0;

	bitsmask = tpd_print_version();
#if 0	
	if((bitsmask / 2)==1)
	{
		printk("FirmwareDownload");
		if(!fnFirmwareDownload(fw_size,rawData,0,NULL))
		{
			printk("FirmwareDownload_fail");
		}
	}
	if((bitsmask % 2)==1)
	{
		printk("ConfigDownload");
		if(!fnFirmwareDownload(0,NULL,config_size,cfg_rawData))
		{
			printk("ConfigDownload_fail");
		}
	}
#endif
	tpd_print_version();
	#if 0
	printk("bitsmask=%d\n",bitsmask);
	if(0 != bitsmask )
		printk("1\n");
		fnFirmwareDownload(fw_size,rawData,config_size,cfg_rawData);
	#endif
	
	//fnFirmwareDownload(fw_size,rawData,config_size,cfg_rawData);
}




static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0, ret = -1;
    unsigned char Wrbuf[2] = { 0x20, 0x07 };
    unsigned char Rdbuffer[10];
#ifdef I2C_SUPPORT_RS_DMA
    char buffer[9];
#else
   char buffer[8];
#endif
	char buffer1[14];
    i2c_client = client;
    printk("MediaTek it7260 touch panel i2c probe\n");

	
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE); 
	msleep(10);
	printk("TPd enter power on---begin-\r\n");
	it7260_power_switch(POWER_ON);
	printk("TPd enter power on---end-\r\n");
	msleep(40);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO); 
	msleep(100);

    FWversion = tpd_FW_version();
    if(FWversion&0x05170000) //7258
    //if(0)
    {
    	printk("FWversion 1\n");
		/*
		msleep(20);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE); 
		msleep(40);
	 	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO); 
	 	*/
    }
    else
    {
    	
    	printk("Not 7258 touch IC\n");
   		//mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
		//msleep(100);
		goto __probe_fail;
    }


  
#ifdef I2C_SUPPORT_RS_DMA
    I2CDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 4096, &I2CDMABuf_pa, GFP_KERNEL);
    if(!I2CDMABuf_va)
    {
        printk("it7260 Allocate Touch DMA I2C Buffer failed!\n");
        return -1;
    }
#endif
	
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
  	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	// Interrupt Input High

  	//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
  	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 0);  /* disable auto-unmask */

    tpd_i2c_write(i2c_client, Wrbuf, 2);   // Clean Queue {0x20, 0x07}
    do{
		tpd_i2c_read(i2c_client, buffer, 1, 0x80);
    }while( buffer[0] & 0x01 );
	
    if(tpd_i2c_read(i2c_client, buffer, 2, 0xA0) < 0)
	{
		 TPD_DMESG("it7260 I2C probe try transfer error, line: %d\n", __LINE__);
		// return -1; //ysong
		goto __probe_fail;
	}
  
    thread = kthread_run(touch_event_handler, 0, CTP_NAME);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        printk(CTP_NAME " it7260 failed to create kernel thread: %d\n", err);
		goto __probe_fail;
    }

	sysfs_create_group(&(client->dev.kobj), &it7260_attr_group);

	#ifdef DOUBLE_CLICK_WAKE
	input_set_capability(tpd->dev, EV_KEY, KEY_POWER);
	#endif
	
    tpd_load_status = 1;  
    printk("DDD_____ 0xA0 : %X, %X\n", buffer[0], buffer[1]);  // add FAE   End
    return TPD_OK;

__probe_fail:
	printk("IT7260 probe fail , enter power off---begin-\r\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	it7260_power_switch(POWER_OFF);
	printk("IT7260 probe fail , enter power off---end-\r\n");
	return TPD_FAIL;
}

void tpd_eint_interrupt_handler(void)
{	
 //   printk("###########tp_int#############\n");
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
} 

static int tpd_i2c_remove(struct i2c_client *client) 
{
#ifdef I2C_SUPPORT_RS_DMA
    if( I2CDMABuf_va ){
		dma_free_coherent(NULL, 4096, I2CDMABuf_va, I2CDMABuf_pa);
		I2CDMABuf_va = NULL;
		I2CDMABuf_pa = 0;
    }
#endif
    return 0;
}

void tpd_down(int raw_x, int raw_y, int x, int y, int p) {	

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
	
    input_report_abs(tpd->dev, ABS_PRESSURE, 128);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);

    input_mt_sync(tpd->dev);
  //  printk("D[%4d %4d %4d]\n", x, y, p);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
}

void tpd_up(int raw_x, int raw_y, int x, int y, int p) {

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
	
    input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
//    printk("U[%4d %4d %4d]\n", x, y, 0);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
}

#ifdef DOUBLE_CLICK_WAKE
static void check_gesture(int gesture_id)
{
	
    printk("it7260 gesture_id==0x%x\n ",gesture_id);
    
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
#endif
		default:
		
			break;
	}

}
#endif

static int x[2] = { (int) -1, (int) -1 };
static int y[2] = { (int) -1, (int) -1 };
static bool finger[2] = { 0, 0 };
static bool flag = 0;

static int touch_event_handler( void *unused )
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    unsigned char pucPoint[14];
	unsigned char key_temp=0;
#ifndef I2C_SUPPORT_RS_DMA
    unsigned char cPoint[8];
    unsigned char ePoint[6];
#endif
    int ret = 0;
    int xraw, yraw;
    int i = 0;

    sched_setscheduler(current, SCHED_RR, &param); 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    do{
        set_current_state(TASK_INTERRUPTIBLE);
        while (tpd_halt) {tpd_flag = 0; msleep(20);}
        wait_event_interruptible(waiter, tpd_flag != 0);
        tpd_flag = 0;
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 

        ret = tpd_i2c_read(i2c_client, &pucPoint[0], 1, 0x80);
        
    //   printk("[IT7260 mtk-tpd] Query status= 0x%x\n", pucPoint[0]);
		if (!( pucPoint[0] & 0x80 || pucPoint[0] & 0x01 )){
			printk("[mtk-tpd] No point information\n");
			msleep(10);
			goto exit_work_func;
		}
	//	printk("1\n");
#ifdef I2C_SUPPORT_RS_DMA
        ret = tpd_i2c_read(i2c_client, &pucPoint[0], 14, 0xE0);
#else        
        ret = tpd_i2c_read(i2c_client, &cPoint[0], 8, 0xC0);
        ret += tpd_i2c_read(i2c_client, &ePoint[0], 6, 0xE0);
        for(i=0; i<6; i++) pucPoint[i] = ePoint[i];
        for(i=0; i<8; i++) pucPoint[i+6] = cPoint[i];
#endif

//	printk("***********************pucPoint[0] = %d\n",pucPoint[0]);
//	printk("***********************pucPoint[1] = %d\n",pucPoint[1]);
//	printk("***********************pucPoint[2] = %d\n",pucPoint[2]);

#ifdef I2C_SUPPORT_RS_DMA        
        if (ret == 0xF01) {
#else
        if (ret == 0xE02) {
#endif
	    // gesture
		    if (pucPoint[0] & 0xF0) {
		    	#ifdef DOUBLE_CLICK_WAKE
				if((gesture_function_enable == 1) && (tpd_halt == 1 ))
				{
			    	 if (pucPoint[0] == 0x10)
			    	 {
			    	 	check_gesture(GESTURE_DOUBLECLICK);
						continue;
			    	  }
				}
		    	#endif
//			printk("-----------------------------------------------pucPoint[0] & 0xF0---------------------.\n");
		    #ifdef TPD_HAVE_PHYSICAL_BUTTON
		    	//physical key
		    	if (  pucPoint[0] & 0x40  && pucPoint[0] & 0x01){
	//				printk("-----------------------------------------------physical key---------------------.\n");
	//				printk("***********************pucPoint[1] = %d\n",pucPoint[1]);
		    	    if ( pucPoint[2] ){
		//				printk("-----------------------------------------------pucPoint[2]---------------------.\n");
						//tpd_button(tpd_keys_dim_local[pucPoint[1]-1][0], tpd_keys_dim_local[pucPoint[1]-1][1], 1); 
						input_report_key(tpd->dev, tpd_it7260_keys[(pucPoint[1]-1)],1);//ysong
						input_sync(tpd->dev);
						key_temp = pucPoint[1]-1;
		       		}
					else{
						//tpd_button(tpd_keys_dim_local[pucPoint[1]-1][0], tpd_keys_dim_local[pucPoint[1]-1][1], 1); 
						input_report_key(tpd->dev, tpd_it7260_keys[key_temp],0);
						input_sync(tpd->dev);
					}
		    	}else
		    #endif
		    	{
		            TPD_DEBUG("[mtk-tpd] it's a button/gesture\n");
		            goto exit_work_func;
		        }
		    }
		    // palm
		    else if( pucPoint[1] & 0x01 ) {
		        TPD_DEBUG("[mtk-tpd] it's a palm\n");
				goto exit_work_func;
		    }
		    // no more data
		    else if (!(pucPoint[0] & 0x08)) {
				if( finger[0] ){
				    finger[0] = 0;
				    tpd_up(x[0], y[0], x[0], y[0], 0);
				    flag = 1;
				}
				if( finger[1] ){
				    finger[1] = 0;
				    tpd_up(x[1], y[1], x[1], y[1], 0);
				    flag = 1;
				}
				if( flag ){
				    flag = 0;
				    input_sync(tpd->dev);
				}
				TPD_DEBUG("[mtk-tpd] no more data\n");
				mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
				goto exit_work_func;
		    }
		    // 3 fingers
		    else if (pucPoint[0] & 0x04) {
		        TPD_DEBUG("[mtk-tpd] we don't support three fingers\n");
				mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
				goto exit_work_func;
		    }
			else{
				// finger 1
		        if (pucPoint[0] & 0x01) {
				    //char pressure_point;
		
				    xraw = ((pucPoint[3] & 0x0F) << 8) + pucPoint[2];
				    yraw = ((pucPoint[3] & 0xF0) << 4) + pucPoint[4];
					//pressure_point = pucPoint[5] & 0x0f;
					//TPD_DEBUG("[mtk-tpd] input Read_Point1 x=%d y=%d p=%d\n",xraw,yraw,pressure_point);
					//tpd_calibrate(&xraw, &yraw);			
				    x[0] = xraw;
					
				    y[0] = yraw;
				    finger[0] = 1;
	//				printk("***********************x[0] = %d,y[0]=%d\n",x[0],y[0]);
				    tpd_down(x[0], y[0], x[0], y[0], 0);
					//printk("*******************tpd_down:x0=%d,y0=%d\n",x[0],y[0]);
					
				} 
				else if( finger[0] ){
				    tpd_up(x[0], y[0], x[0], y[0], 0);
				    finger[0] = 0;
				}
	
				// finger 2
				if (pucPoint[0] & 0x02) {
				    //char pressure_point;
				    xraw = ((pucPoint[7] & 0x0F) << 8) + pucPoint[6];
				    yraw = ((pucPoint[7] & 0xF0) << 4) + pucPoint[8];
				    //pressure_point = pucPoint[9] & 0x0f;
				    //TPD_DEBUG("[mtk-tpd] input Read_Point2 x=%d y=%d p=%d\n",xraw,yraw,pressure_point);
				   // tpd_calibrate(&xraw, &yraw);
		  		    x[1] = xraw;
				    y[1] = yraw;
				    finger[1] = 1;
				    tpd_down(x[1], y[1], x[1], y[1], 1);
					//printk("*******************tpd_down:x1=%d,y1=%d\n",x[1],y[1]);

				} else if (finger[1]){
				    tpd_up(x[1], y[1], x[1], y[1], 0);
				    finger[1] = 0;
				}
			input_sync(tpd->dev);
		    }
		}else{
		    TPD_DEBUG("[mtk-tpd] i2c read communcate error in getting pixels : 0x%x\n", ret);
		}
		
exit_work_func:
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    } while (!kthread_should_stop()); 
    return 0;
}

static int tpd_local_init(void) 
{
    int r;
    
	TPD_DMESG("Focaltech it7260 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
		//power on, need confirm with SA

    if(i2c_add_driver(&tpd_i2c_driver)!=0) {
        printk("unable to add i2c driver.\n");
        return -1;
    }
    
    /* register device (/dev/IT7260 CTP) */
    //ctp_dev.parent = tpd->dev;
    r = misc_register(&ctp_dev);
    if (r) {
        printk("register ctp device failed (%d)\n", r);
        return -1;
    }
   

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
    printk("end %s, %d\n", __FUNCTION__, __LINE__);  

    tpd_type_cap = 1;
    
    return 0;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct i2c_client *client, pm_message_t message)
{
    int ret = 0;
    printk("it7260 tpd_suspend start\n");

#ifdef  DOUBLE_CLICK_WAKE
    unsigned char Wrbuf_sleep[4] = { 0x20, 0x04, 0x00, 0x02 };
 	unsigned char Wrbuf_idle[4] = { 0x20, 0x04, 0x00, 0x01 };
  if(gesture_function_enable == 1 )
  {
    TPD_DEBUG("IT7260 call suspend,enter ilde\n");    
    ret = tpd_i2c_write(i2c_client, Wrbuf_idle, 4);   
    msleep(20); 
	if(ret != 4){
        TPD_DEBUG("[mtk-tpd] i2c write communcate error during suspend: 0x%x\n", ret);
    }else
    {
    	 printk("IT7260 call suspend OK\n");
    }
  }else
#endif
	{
		tpd_halt = 1;
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); 
		it7260_power_switch(POWER_OFF);
  	}
	printk("it7260 tpd_suspend end\n");
}

/* Function to manage power-on resume */
static void tpd_resume(struct i2c_client *client)
{   
    #define TRY_COUNTS 5
    int ret = 0;
    unsigned char Wrbuf[2] = { 0x20, 0x6F};
    char gsbuffer[2];

	printk("it7260 tpd_resume start\n");
	it7260_power_switch(POWER_ON);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    if(FWversion&0x05170000) //7258
    {
      mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE); 
	  msleep(20);
	  mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO); 
    }
    else
    {
    	msleep(100);
    	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO); 
		msleep(100);
     	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE );
    }

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
    tpd_halt = 0;
    // liuzhiyong 20120619 update code ++ 
//	tpd_up(0, 0, 0, 0, 0);
	tpd_up(0, 0, 0, 0, 0);
	input_sync(tpd->dev);
	printk("it7260 tpd_resume end\n");
}
		
static struct tpd_driver_t tpd_device_driver = {
	//.tpd_device_name = "IT7260",
	.tpd_device_name =CTP_NAME,  // TPD_DEVICE,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif 
};


static bool waitCommandDone(void)
{
	unsigned char ucQuery = 0xFF;
	unsigned int count = 0;

	do{
		ucQuery = 0xFF;
		tpd_i2c_read(i2c_client, &ucQuery, 1, 0x80);
		count++;
	}while(ucQuery & 0x01 && count < 500);

	if( !(ucQuery & 0x01) ){
	        return  true;
	}else{
		printk("XXX %s, %d\n", __FUNCTION__, __LINE__);
		return  false;
	}
}

bool fnFirmwareReinitialize(void)
{
	u8 pucBuffer[2];
	waitCommandDone();

	pucBuffer[0] = 0x20;
	pucBuffer[1] = 0x6F;
	if( !tpd_i2c_write(i2c_client, pucBuffer, 2) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	return true;
}

bool fnSetStartOffset(unsigned short wOffset)
{
	u8 pucBuffer[MAX_BUFFER_SIZE];
	char wCommandResponse[2] = { 0xFF, 0xFF };

	waitCommandDone();

	pucBuffer[0] = 0x20;
	pucBuffer[1] = 0x61;
	pucBuffer[2] = 0;
	pucBuffer[3] = ( wOffset & 0x00FF );
	pucBuffer[4] = (( wOffset & 0xFF00 ) >> 8 );

	if( !tpd_i2c_write(i2c_client, pucBuffer, 5) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	//printk("[IT7260] fnSetStartOffset %s, %d\n", __FUNCTION__, __LINE__);
	waitCommandDone();

	if( !tpd_i2c_read(i2c_client, wCommandResponse, 2, 0xA0 )  ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}

	if(wCommandResponse[0] | wCommandResponse[1]){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}

	return true;
}


bool fnEnterFirmwareUpgradeMode(void)
{
	unsigned char pucBuffer[MAX_BUFFER_SIZE];
	char wCommandResponse[2] = { 0xFF, 0xFF };

	waitCommandDone();

	pucBuffer[0] = 0x20;
	pucBuffer[1] = 0x60;
	pucBuffer[2] = 0x00;
	pucBuffer[3] = 'I';
	pucBuffer[4] = 'T';
	pucBuffer[5] = '7';
	pucBuffer[6] = '2';
	pucBuffer[7] = '6';
	pucBuffer[8] = '0';
	pucBuffer[9] = 0x55;
	pucBuffer[10] = 0xAA;

#ifdef HAS_8_BYTES_LIMIT
	if( !tpd_i2c_write(i2c_client, pucBuffer, 7) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
#else
	if( !tpd_i2c_write(i2c_client, pucBuffer, 11) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
#endif
	waitCommandDone();
	if(!tpd_i2c_read(i2c_client, wCommandResponse, 2, 0xA0 ) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}

	if( wCommandResponse[0] | wCommandResponse[1] ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	//TPD_DEBUG("[IT7260] OOO %s, %d\n", __FUNCTION__, __LINE__);
	return true;
}

static bool fnWriteAndCompareFlash(unsigned int nLength, char pnBuffer[], unsigned short nStartOffset)
{
	unsigned int nIndex = 0;
	unsigned char buffer[144] = {0};
	unsigned char bufWrite[144] = {0};
	unsigned char bufRead[144] = {0};
	unsigned char bufTemp[4] = {0};
	unsigned char nReadLength;
	int retryCount;
	int i;
	u8 nOffset = 0;

#ifdef HAS_8_BYTES_LIMIT
	nReadLength = 4;
#else
	nReadLength = 128;
#endif

	while ( nIndex < nLength ) {
		retryCount = 0;
		do {
			fnSetStartOffset(nStartOffset + nIndex);
#ifdef HAS_8_BYTES_LIMIT
			// Write to Flash
			for(nOffset = 0; nOffset < 128; nOffset += 4) {
				buffer[0] = 0x20;
				buffer[1] = 0xF0;
				buffer[2] = nOffset;
				for ( i = 0; i < 4; i++ ) {
					bufWrite[nOffset + i] = buffer[3 + i] = pnBuffer[nIndex + i + nOffset];
				}
				tpd_i2c_write(i2c_client, buffer, 7);
			}
			//mtk_kick_wdt();
			buffer[0] = 0x20;
			buffer[1] = 0xF1;
			tpd_i2c_write(i2c_client, buffer, 2);
#else
			buffer[0] = 0x20;
			buffer[1] = 0x62;
			buffer[2] = 128;
			for (i = 0; i < 128; i++) {
				bufWrite[i] = buffer[3 + i] = pnBuffer[nIndex + i];
			}
			tpd_i2c_write(i2c_client, buffer, 131);
#endif
			// Read from Flash
			buffer[0] = 0x20;
			buffer[1] = 0x63;
	  		buffer[2] = nReadLength;

#ifdef HAS_8_BYTES_LIMIT
			for (nOffset = 0; nOffset < 128; nOffset += 4) {
				//printk("[IT7260] fnWriteAndCompareFlash %s, %d,nStartOffset=%x,nIndex=%x,nOffset=%x\n", __FUNCTION__, __LINE__,nStartOffset,nIndex,nOffset);
				fnSetStartOffset(nStartOffset + nIndex + nOffset);
				tpd_i2c_write(i2c_client, buffer, 3);
				waitCommandDone();
				tpd_i2c_read(i2c_client, bufTemp, nReadLength, 0xA0);
				for( i = 0; i < 4; i++ ){
						bufRead[nOffset+i] = bufTemp[i];
				}
			}
			//mtk_kick_wdt();
#else
	 		fnSetStartOffset(nStartOffset + nIndex);
			tpd_i2c_write(i2c_client, buffer, 3);
			waitCommandDone();
			tpd_i2c_read(i2c_client, bufRead, nReadLength, 0xA0);
#endif
			// Compare
			for (i = 0; i < 128; i++) {
				if (bufRead[i] != bufWrite[i]) {
					break;
				}
			}
			if (i == 128) break;
		}while ( retryCount++ < 4 );

		if ( retryCount == 4 && i != 128 ){
			//TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
			return false;
		}
		nIndex += 128;
	}
	return true;
}


bool fnExitFirmwareUpgradeMode(void)
{
	char pucBuffer[MAX_BUFFER_SIZE];
	char wCommandResponse[2] = { 0xFF, 0xFF };

	waitCommandDone();

	pucBuffer[0] = 0x20;
	pucBuffer[1] = 0x60;
	pucBuffer[2] = 0x80;
	pucBuffer[3] = 'I';
	pucBuffer[4] = 'T';
	pucBuffer[5] = '7';
	pucBuffer[6] = '2';
	pucBuffer[7] = '6';
	pucBuffer[8] = '0';
	pucBuffer[9] = 0xAA;
	pucBuffer[10] = 0x55;

#ifdef HAS_8_BYTES_LIMIT
	if( !tpd_i2c_write(i2c_client, pucBuffer, 7) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
#else
	if( !tpd_i2c_write(i2c_client, pucBuffer, 11) ){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
#endif

	waitCommandDone();

	if(!tpd_i2c_read(i2c_client, wCommandResponse, 2, 0xA0 )){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}

	if(wCommandResponse[0] | wCommandResponse[1]){
		TPD_DEBUG("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	//TPD_DEBUG("[IT7260] OOO %s, %d\n", __FUNCTION__, __LINE__);
	return true;
}
extern int mtk_wdt_enable(enum wk_wdt_en en);
bool fnFirmwareDownload(unsigned int unFirmwareLength, u8* pFirmware, unsigned int unConfigLength, u8* pConfig)
{
	
	if((unFirmwareLength == 0 || pFirmware == NULL) && (unConfigLength == 0 || pConfig == NULL)){
		printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	printk("2\n");
	//printk("[IT7260] fnFirmwareDownload %s, %d\n", __FUNCTION__, __LINE__);

	if(!fnEnterFirmwareUpgradeMode()){
		printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
	printk("3\n");
	//printk("[IT7260] fnEnterFirmwareUpgradeMode %s, %d\n", __FUNCTION__, __LINE__);

   mtk_wdt_enable ( WK_WDT_DIS );//	mtk_kick_wdt();
	if(unFirmwareLength != 0 && pFirmware != NULL){
		// Download firmware
		if(!fnWriteAndCompareFlash(unFirmwareLength, pFirmware, 0)){
			printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
			return false;
		}
	}
	//printk("[IT7260] fnWriteAndCompareFlash Fireware %s, %d\n", __FUNCTION__, __LINE__);
	printk("4\n");

//	mtk_kick_wdt();
	if(unConfigLength != 0 && pConfig != NULL){
		// Download configuration
		unsigned short wFlashSize = 0x8000;
		if( !fnWriteAndCompareFlash(unConfigLength, pConfig, wFlashSize - (unsigned short)unConfigLength)){
			printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
			return false;
		}
	}
	//printk("[IT7260] fnWriteAndCompareFlash Config %s, %d\n", __FUNCTION__, __LINE__);
	printk("5\n");

//	mtk_kick_wdt();
	if(!fnExitFirmwareUpgradeMode()){
		printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
//	mtk_kick_wdt();
	printk("[IT7260] fnExitFirmwareUpgradeMode %s, %d\n", __FUNCTION__, __LINE__);

	if(!fnFirmwareReinitialize()){
		printk("[IT7260] XXX %s, %d\n", __FUNCTION__, __LINE__);
		return false;
	}
//	mtk_kick_wdt();
	printk("[IT7260] OOO %s, %d\n", __FUNCTION__, __LINE__);

	mtk_wdt_enable (WK_WDT_EN );
	return true;
}


/* called when loaded into kernel */
static int __init tpd_driver_init(void)
{
    printk("MediaTek IT7260 touch panel driver init\n");
#if defined(TPD_I2C_NUMBER)	//lshun modify 20130615
    i2c_register_board_info(TPD_I2C_NUMBER, &it7260_i2c_tpd, 1);
#else
	i2c_register_board_info(0, &it7260_i2c_tpd, 0);
#endif
	if(tpd_driver_add(&tpd_device_driver) < 0)
		printk("add generic driver failed\n");
       
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
    printk("MediaTek IT7260 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}


module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

