

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>

#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <asm/uaccess.h>

#include "tpd_custom_nt11004.h"

#include <linux/wakelock.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>

#include <mach/wd_api.h>
#include <linux/watchdog.h>
#include <mach/mt_wdt.h>



#include <linux/miscdevice.h>
#include <pmic_drv.h>

#include "tpd.h"
#include <cust_eint.h>

#ifndef TPD_NO_GPIO
#include "cust_gpio_usage.h"
#endif

#ifdef NVT_PROXIMITY_FUNC_SUPPORT
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif


#define __devexit
#define __devinitdata
#define __devinit 
#define __devexit_p


//extern kal_bool upmu_chr_det(upmu_chr_list_enum chr);
extern struct tpd_device *tpd;

static int tpd_flag = 0;
static int tpd_halt = 0;

static struct task_struct *thread = NULL;
#define POWER_OFF                  0
#define POWER_ON                   1
static int power_flag=0;// 0 power off,default, 1 power on

extern int tpd_load_status;

#ifdef TPD_HAVE_BUTTON_TYPE_COOR
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif


static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_remove(struct i2c_client *client);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);





//tpd i2c
static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id nt11004_tpd_id[] = {{NT11004_TS_NAME, TPD_I2C_NUMBER},{}};
//static unsigned short force[] = {0, NT11004_TS_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short * const forces[] = {force, NULL};
//static struct i2c_client_address_data addr_data = {.forces = forces, };
static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO(NT11004_TS_NAME, NT11004_TS_ADDR)};


static struct i2c_driver tpd_i2c_driver = {
	.driver = 
	{
		.name = NT11004_TS_NAME,
		//.owner = THIS_MODULE,
	},
	.probe = tpd_i2c_probe,
	.remove = __devexit_p(tpd_i2c_remove),
	.id_table = nt11004_tpd_id,
	.detect = tpd_i2c_detect,
	//.address_data = &addr_data,
};

#ifdef TPD_HAVE_BUTTON
void tpd_nt11004_key_init(void)
{
    int i = 0;

#ifdef TPD_HAVE_BUTTON_TYPE_COOR
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
#else
    for(i=0;i<TPD_KEY_COUNT;i++)
    {
        __set_bit(tpd_nt11004_keys[i], tpd->dev->keybit);
    }
#endif

}
#endif


static  void nt11004_power_switch(s32 state)
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

static int i2c_read_bytes( struct i2c_client *client, u8 addr, u8 *rxbuf, int len )
{
    u8 retry;
    u16 left = len;
    u16 offset = 0;

    if ( rxbuf == NULL )
    {
        return TPD_FAIL;
    }

    //TPD_DMESG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len );
	
    while ( left > 0 )
    {
        if ( left > MAX_TRANSACTION_LENGTH )
        {
            rxbuf[offset] = ( addr+offset ) & 0xFF;
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
            retry = 0;
            while ( i2c_master_send(i2c_client, &rxbuf[offset], (MAX_TRANSACTION_LENGTH << 8 | 1)) < 0 )
           //while ( i2c_smbus_read_i2c_block_data(i2c_client, offset,8,&rxbuf[offset] )
            {
                retry++;

                if ( retry == 5 )
                {
                    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
                    TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, MAX_TRANSACTION_LENGTH);
                    return -1;
                }
            }
            left -= MAX_TRANSACTION_LENGTH;
            offset += MAX_TRANSACTION_LENGTH;
        }
        else
        {

            //rxbuf[0] = addr;
            rxbuf[offset] = ( addr+offset ) & 0xFF;
            i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;

            retry = 0;
			//while ( i2c_smbus_read_i2c_block_data(i2c_client, offset,left,&rxbuf[offset] )
            while ( i2c_master_send(i2c_client, &rxbuf[offset], (left<< 8 | 1)) < 0 )
            {
                retry++;

                if ( retry == 5 )
                {
                    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;
                    TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, left);
                    return TPD_FAIL;
                }
            }
            left = 0;
        }
    }

    i2c_client->addr = i2c_client->addr & I2C_MASK_FLAG;

    return TPD_OK;
}


static int i2c_write_bytes( struct i2c_client *client, u16 addr, u8 *txbuf, int len )
{
    u8 buffer[MAX_TRANSACTION_LENGTH];
    u16 left = len;
    u16 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg =
    {
        .addr = ((client->addr&I2C_MASK_FLAG )|(I2C_ENEXT_FLAG )),
        .flags = 0,
        .buf = buffer
    };

    if ( txbuf == NULL )
    {
        return TPD_FAIL;
    }

   	//TPD_DEBUG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len );

    while ( left > 0 )
    {
        retry = 0;
        buffer[0] = ( addr+offset ) & 0xFF;

        if ( left > MAX_I2C_TRANSFER_SIZE )
        {
            memcpy( &buffer[I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], MAX_I2C_TRANSFER_SIZE );
            msg.len = MAX_TRANSACTION_LENGTH;
            left -= MAX_I2C_TRANSFER_SIZE;
            offset += MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy( &buffer[I2C_DEVICE_ADDRESS_LEN], &txbuf[offset], left );
            msg.len = left + I2C_DEVICE_ADDRESS_LEN;
            left = 0;
        }

        TPD_DEBUG("byte left %d offset %d\n", left, offset );

        while ( i2c_transfer( client->adapter, &msg, 1 ) != 1 )
        {
            retry++;
            if ( retry == 5 )
            {
                TPD_DEBUG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
                return TPD_FAIL;
            }
            else
        	{
             	TPD_DEBUG("I2C write retry %d addr 0x%X%X\n", retry, buffer[0], buffer[1]);
        	}
        }
    }

    return TPD_OK;
}

static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, NT11004_TS_NAME);
	return TPD_OK;
}

static void tpd_reset(void)
{
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
}



static int tpd_i2c_print_version(void)
{
	return TPD_OK;
}

#ifdef NVT_APK_DRIVER_FUNC_SUPPORT
struct nvt_flash_data 
{
	rwlock_t lock;
	unsigned char bufferIndex;
	unsigned int length;
	struct i2c_client *client;
};
static int nvt_apk_mode = 0;
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"
struct nvt_flash_data *flash_priv;

/*******************************************************
Description:
	Novatek touchscreen control driver initialize function.

Parameter:
	priv:	i2c client private struct.
	
return:
	Executive outcomes.0---succeed.
*******************************************************/
int nvt_flash_write(struct file *file, const char __user *buff, size_t count, loff_t *offp)
{
	struct i2c_msg msgs[2];	
	char *str;
	int ret = -1;
	int retries = 0;
	unsigned char tmpaddr;

	TPD_DMESG("nvt_flash_write\n");
	
	file->private_data = (uint8_t *)kmalloc(64, GFP_KERNEL);
	str = file->private_data;
	ret = copy_from_user(str, buff, count);

	TPD_DMESG("str[0]=%x, str[1]=%x, str[2]=%x, str[3]=%x\n", str[0], str[1], str[2], str[3]);
	
	tmpaddr = i2c_client->addr;
	if((str[0] == 0x7F)||(str[0] == (0x7F<<1)))
	{
		i2c_client->addr = NT11004_HW_ADDR;
	}
	else if((str[0] == 0x01)||(str[0] == (0x01<<1)))
	{
		i2c_client->addr = NT11004_FW_ADDR;
	}
	else
	{
		i2c_client->addr = NT11004_TS_ADDR;
	}
	
	i2c_smbus_write_i2c_block_data(i2c_client, str[2], str[1]-1, &str[3]);
	i2c_client->addr = tmpaddr;

	return ret;
}

int nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	struct i2c_msg msgs[2];	 
	char *str;
	int ret = -1;
	int retries = 0;
	unsigned char tmpaddr;

	TPD_DMESG("nvt_flash_read\n");

	file->private_data = (uint8_t *)kmalloc(64, GFP_KERNEL);
	str = file->private_data;
	
	if(copy_from_user(str, buff, count))
	{
		return -EFAULT;
	}

	TPD_DMESG("str[0]=%x, str[1]=%x, str[2]=%x, str[3]=%x\n", str[0], str[1], str[2], str[3]);
		
	tmpaddr = i2c_client->addr;
	if((str[0] == 0x7F)||(str[0] == (0x7F<<1)))
	{
		i2c_client->addr = NT11004_HW_ADDR;
	}
	else if((str[0] == 0x01)||(str[0] == (0x01<<1)))
	{
		i2c_client->addr = NT11004_FW_ADDR;
	}
	else
	{
		i2c_client->addr = NT11004_TS_ADDR;
	}
	
	i2c_smbus_read_i2c_block_data(i2c_client, str[2], str[1]-1, &str[3]);
	i2c_client->addr = tmpaddr;
	ret = copy_to_user(buff, str, count);
	
	return ret;
}

int nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) 
	{
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	nvt_apk_mode = 1;
	
	return 0;
}

int nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev) 
	{
		kfree(dev);
	}

	nvt_apk_mode = 0;
	
	return 0;   
}

struct file_operations nvt_flash_fops = 
{
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.write = nvt_flash_write,
	.read = nvt_flash_read,
};

static int nvt_flash_init()
{		
	int ret=0;
  	NVT_proc_entry = create_proc_entry(DEVICE_NAME, 0666, NULL);
	if(NVT_proc_entry == NULL)
	{
		TPD_DMESG("Couldn't create proc entry!\n");
		ret = -ENOMEM;
		return ret ;
	}
	else
	{
		TPD_DMESG("Create proc entry success!\n");
		NVT_proc_entry->proc_fops = &nvt_flash_fops;
	}
	flash_priv=kzalloc(sizeof(*flash_priv),GFP_KERNEL);	
	TPD_DMESG("============================================================\n");
	TPD_DMESG("NVT_flash driver loaded\n");
	TPD_DMESG("============================================================\n");	
	return 0;
error:
	if(ret != 0)
	{
		TPD_DMESG("flash_priv error!\n");
	}
	return -1;
}

#endif	// NVT_APK_DRIVER_FUNC_SUPPORT


#ifdef NVT_PROXIMITY_FUNC_SUPPORT
#define TPD_PROXIMITY_ENABLE_REG                  0x88//01
static u8 tpd_proximity_flag = 0;
static u8 tpd_proximity_detect = 1;	//0-->close ; 1--> far away

static s32 tpd_proximity_get_value(void)
{
    return tpd_proximity_detect;
}

static s32 tpd_proximity_enable(s32 enable)
{
    u8  state;
    s32 ret = -1;
    
	TPD_DMESG("tpd_proximity_enable enable=%d\n",enable);
	
    if (enable)
    {
        state = 1;
        tpd_proximity_flag = 1;
        TPD_DMESG("TPD proximity function to be on.\n");
    }
    else
    {
        state = 0;
        tpd_proximity_flag = 0;
        TPD_DMESG("TPD proximity function to be off.\n");
    }

    ret = i2c_write_bytes(i2c_client, TPD_PROXIMITY_ENABLE_REG, &state, 1);

    if (ret < 0)
    {
        TPD_DMESG("TPD %s proximity cmd failed.\n", state ? "enable" : "disable");
        return ret;
    }

    TPD_DMESG("TPD proximity function %s success.\n", state ? "enable" : "disable");
    return 0;
}

s32 tpd_proximity_operate(void *self, u32 command, void *buff_in, s32 size_in,
                   void *buff_out, s32 size_out, s32 *actualout)
{
    s32 err = 0;
    s32 value;
    hwm_sensor_data *sensor_data;

    switch (command)
    {
        case SENSOR_DELAY:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                TPD_DMESG("Set delay parameter error!");
                err = -EINVAL;
            }

            // Do nothing
            break;

        case SENSOR_ENABLE:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                TPD_DMESG("Enable sensor parameter error!");
                err = -EINVAL;
            }
            else
            {
                value = *(int *)buff_in;
                err = tpd_proximity_enable(value);
            }

            break;

        case SENSOR_GET_DATA:
            if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data)))
            {
                TPD_DMESG("Get sensor data parameter error!");
                err = -EINVAL;
            }
            else
            {
                sensor_data = (hwm_sensor_data *)buff_out;
                sensor_data->values[0] = tpd_proximity_get_value();
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
            }
            break;

        default:
            TPD_DMESG("proxmy sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }

    return err;
}

static int tpd_proximity_event(u8 buf1, u8 buf2)
{
	int ret = 0;
    s32 err = 0;
    hwm_sensor_data sensor_data;
    u8 proximity_status;
    u8 point_data[20];

	TPD_DMESG("tpd_proximity_flag = %d, buf2 = %d\n", tpd_proximity_flag, buf2);

	if (tpd_proximity_flag == 1)
	{
		proximity_status = buf2;

		if (proximity_status & 0x80)				//proximity or large touch detect,enable hwm_sensor.
		{
			tpd_proximity_detect = 0;
		}
		else
		{
			tpd_proximity_detect = 1;
		}

		TPD_DMESG("PROXIMITY STATUS:0x%02X\n", tpd_proximity_detect);
		//map and store data to hwm_sensor_data
		sensor_data.values[0] = tpd_proximity_get_value();
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		//report to the up-layer
		ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

		if (ret)
		{
			TPD_DMESG("Call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
}

int tpd_proximity_init(void)
{
    int err = 0;
	struct hwmsen_object obj_ps;

    //obj_ps.self = cm3623_obj;
    obj_ps.polling = 0;         //0--interrupt mode; 1--polling mode;
    obj_ps.sensor_operate = tpd_ps_operate;

    if ((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
    {
        TPD_DMESG("hwmsen attach fail, return:%d.", err);
    }
}
#endif // NVT_PROXIMITY_FUNC_SUPPORT

#ifdef NVT_FW_UPDATE_FUNC_SUPPORT

static void nvt_delay_ms(long delay)
{
	msleep(delay);
}

static void nvt_reset(void)
{
	tpd_reset();
}


int nvt_i2c_read_bytes(unsigned char addr, unsigned char reg, unsigned char *data, int len)
{
	unsigned char tmpaddr;
	int ret;
	
	tmpaddr = i2c_client->addr;
	i2c_client->addr = addr;
	ret = i2c_smbus_read_i2c_block_data(i2c_client, reg, len, data);
	i2c_client->addr = tmpaddr;

	if(ret < 0)
	{
	    return 0;
	}
	else
	{
	    return 1;
	}
}


int nvt_i2c_write_bytes(unsigned char addr, unsigned char reg, unsigned char *data, int len)
{
	unsigned char tmpaddr;
	int ret;
	
	tmpaddr = i2c_client->addr;
	i2c_client->addr = addr;
	ret = i2c_smbus_write_i2c_block_data(i2c_client, reg, len, data);
	i2c_client->addr = tmpaddr;

	if(ret < 0)
	{
	    return 0;
	}
	else
	{
	    return 1;
	}
}

int nvt_fw_compare_chexksum_style_file(void)
{
	unsigned short checksum1, checksum2;
	unsigned char offset = 0;
	unsigned char buf[4] = {0};
	int i, count = 0;
	int ret;

	TPD_DMESG("nvt_fw_compare_chexksum_style_file \n");

	// SW RST and then CPU normally run or HW RST
	offset = 0x00;
	buf[0] = 0x5A;
	ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 1);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_write_bytes Error \n"); 		
		return 0;
	}
	nvt_delay_ms(100);


	// I2C特別指令切換至Address至0x0EF9
	offset = 0xFF;
	buf[0] = 0x0E;
	buf[1] = 0xF9;	
	ret = nvt_i2c_write_bytes(NT11004_TS_ADDR, offset, buf, 2);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_write_bytes Error \n"); 		
		return 0;
	}

	// I2C Write Short Test Command (0xE1)
	offset = 0x00;
	buf[0] = 0xE1;
	ret = nvt_i2c_write_bytes(NT11004_TS_ADDR, offset, buf, 1);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_write_bytes Error \n"); 		
		return 0;
	}
	nvt_delay_ms(100);


	// I2C特別指令切換至Address至0x0CD7
	offset = 0xFF;
	buf[0] = 0x0C;
	buf[1] = 0xD7;	
	ret = nvt_i2c_write_bytes(NT11004_TS_ADDR, offset, buf, 2);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_write_bytes Error \n"); 		
		return 0;
	}

	// I2C Read 1 Byte, 重覆直到收到0xAA
	count = 0;
	while(count < 10)
	{
		offset = 0x00;
		ret = nvt_i2c_read_bytes(NT11004_TS_ADDR, offset, buf, 1);
		if( ret < 0 )			
		{
			TPD_DMESG("nvt_i2c_read_bytes Error \n"); 		
			return 0;
		}

		if(buf[0] == 0XAA)
		{
			//TPD_DMESG("buf[0] == 0XAA \n");
			break;
		}
	}

	offset = 0xFF;
	buf[0] = 0x0C;
	buf[1] = 0xD8;	
	ret = nvt_i2c_write_bytes(NT11004_TS_ADDR, offset, buf, 2);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_write_bytes Error \n"); 		
		return 0;
	}

	// I2C Read 2 Byte Result
	offset = 0x00;
	ret = nvt_i2c_read_bytes(NT11004_TS_ADDR, offset, buf, 2);
	if( ret < 0 )			
	{
		TPD_DMESG("nvt_i2c_read_bytes Error \n"); 		
		return 0;
	}

	checksum1 = (((unsigned short)buf[0]) << 8)|((unsigned short)buf[1]);


	// get checksum of bin file
	checksum2 = 0;
	for(i = 0; i < FLASH_SIZE; i++)
	{
		checksum2 = checksum2 + fw_binary_data[i];
	}
	
	TPD_DMESG("checksum1 = %x; checksum2 = %x\n", checksum1, checksum2); 
	
	//Compare the chechsum
	if (checksum1!= checksum2)
	{
		return 0;
	}
	
	return 1; 							// Boot loader function is completed.
}


int nvt_fw_to_normal_state(void)
{
	unsigned char buf[2] = {0};
	unsigned char offset = 0;
	int i = 10;
	int ret = 0;

	TPD_DMESG("nvt_fw_to_normal_state\n");

	//sw reset
	while(i--)
	{	

		offset = 0x00;
		buf[0] = 0x5A;
		ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
		{	
			return 0;
		}
		nvt_delay_ms(100);

		offset = 0x88;
		buf[0] = 0x55;
		ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
		{	
			return 0;
		}		
		nvt_delay_ms(10);
		
		// Host Controller read NT11004 status form I2C address 7FH
		// Read Status
		offset = 0x00;
		ret = nvt_i2c_read_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
	  	{	
			return 0;
		}

		if(buf[0] == 0xAA)
		{
			//TPD_DMESG("buf[1] == 0xAA\n");	
			return 1;
		}
	}

	return 0;
}


int nvt_boot_loader_init(void)
{
	unsigned char buf[2] = {0};
	unsigned char offset = 0;
	int i = 10;
	int ret = 0;

	TPD_DMESG("nvt_boot_loader_init\n");

	//sw reset
	while(i--)
	{	
		// Software Reset Command
		// Host Controller write a A5H to NT11004 I2C addr 7FH
		offset = 0x00;
		buf[0] = 0xA5;
	
		ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
		{
			TPD_DMESG("Send Software Reset Command Error \n");			
			return 0;
		}
		nvt_delay_ms(5);

		// Initiation Command
		// Host Controller write a 00H to NT11004 I2C address 7F
		// Enter flash mode
		offset = 0x00;
		buf[0] = 0x00; 
		ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
		{	
			return 0;
		}
		nvt_delay_ms(5);
		
		// Host Controller read NT11004 status form I2C address 7FH
		// Read Status
		offset = 0x00;
		ret = nvt_i2c_read_bytes(NT11004_HW_ADDR, offset, buf, 1);
		if( ret < 0 )   		
	  	{	
			return 0;
		}

		if(buf[0] == 0xAA)
		{
			//TPD_DMESG("buf[1] == 0xAA\n");	
			return 1;
		}
	}

	return 0;
}


int nvt_disable_flash_protect(void)
{
	unsigned char buf[4] = {0};
	unsigned char offset = 0;
	unsigned char ret,i;

	TPD_DMESG("nvt_disable_flash_protect \n");

	offset = 0xff;
	buf[0] = 0xF0;
	buf[1] = 0xAC;
	ret = nvt_i2c_write_bytes(NT11004_FW_ADDR, offset, buf, 2);
    if(ret < 0)   		
  	{
	    return 0;
    }
    nvt_delay_ms(20);   //Delay 

	offset = 0x00;
	buf[0] = 0x21;
	ret = nvt_i2c_write_bytes(NT11004_FW_ADDR, offset, buf, 1);
    if(ret < 0)   		
  	{
	    return 0;
    }
    nvt_delay_ms(20);   //Delay 
    
    offset = 0x00;
	buf[0] = 0x99;
	buf[1] = 0x00;
	buf[2] = 0x0E;
	buf[3] = 0X01;
	ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 4);
    if(ret < 0)   		
  	{
	    return 0;
    }	
    nvt_delay_ms(20);   //Delay 
    
	offset = 0x00;
	buf[0] = 0x81;
	ret = nvt_i2c_write_bytes(NT11004_FW_ADDR, offset, buf, 1);
    if(ret < 0)   		
  	{
	    return 0;
    }	
    nvt_delay_ms(20);   //Delay 
       
	//NT11004_TRACE("[FangMS] %s,%d  \r\n",__func__,__LINE__);
	offset = 0x00;
	buf[0] = 0x99;
	buf[1] = 0x00;
	buf[2] = 0x0F;
	buf[3] = 0X01;
	ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 4);
    if(ret < 0)   		
  	{
		return 0;
    }	
    nvt_delay_ms(20);   //Delay 
    
	offset = 0x00;
	buf[0] = 0x01;
	ret = nvt_i2c_write_bytes(NT11004_FW_ADDR, offset, buf, 1);
    if(ret < 0)   		
  	{
	    return 0;
    }	
    nvt_delay_ms(20);   //Delay 
    
	offset = 0xff;
	buf[0] = 0x00;
	buf[1] = 0x00;
	ret = nvt_i2c_write_bytes(NT11004_FW_ADDR, offset, buf, 2);
    if(ret < 0)   		
  	{
	    return 0;
    }

    return 1;
}


int nvt_erase_flash_sector(int sec)
{
	unsigned char buf[4] = {0};
	unsigned char offset = 0;
	int i;
	int ret = 0;

	TPD_DMESG("nvt_erase_flash_sector sec = %x \n", sec);
	
	offset = 0x00;	// offset address
	buf[0] = 0x33;	// erase flash command 33H
	buf[1] = (unsigned char)((sec*SECTOR_SIZE) >> 8); 		// section_addr high 8 bit
	buf[2] = (unsigned char)((sec*SECTOR_SIZE) & 0x00ff); 	// section_addr low 8 bit

	// Host write erase flash command(33H) and flash section address to NT11004 I2C device address 7FH
	ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 3);
	if( ret < 0 )   		
	{
		return 0;
	}

	// Delay 40mS
	nvt_delay_ms(40);

	// Read status
	offset = 0x00;	// offset address	
	ret = nvt_i2c_read_bytes(NT11004_HW_ADDR, offset, buf, 1);
	if( ret < 0 )   		
	{
		return 0;
	}

	if(buf[0] == 0xAA)
	{
		//TPD_DMESG("buf[0] == 0xAA\n");
		return 1;
	}

	return 0;
}

int nvt_erase_flash(void)
{
	int i, ret;

	TPD_DMESG("nvt_erase_flash \n");
    
	nvt_disable_flash_protect();
    
	for(i = 0; i < SECTOR_NUM; i++)
	{
		ret = nvt_erase_flash_sector(i);

		if(ret != 1)
		{
			return 0;
		}
	}
	
	return 1;
}


int nvt_write_binary_data_to_flash(unsigned char *fw_data, unsigned int data_len)
{
	unsigned char buf[16] = {0};
	unsigned char offset = 0;
	unsigned char checksum;
	unsigned short addr;
	int sector;
	int ret = 0;
	int i;
	
	TPD_DMESG("nvt_write_binary_data_to_flash \n");

	sector = 0;
	addr = 0;	// start address is 0
	do
	{
		TPD_DMESG("Write data to flash ... sector = %d\n", sector);
		// write 8 bytes to flash start
		#ifdef MTK_2BYTES_MODE
		for(i = 0; i < (16*4); i++)
		#else
		for(i = 0; i < 16; i++)
		#endif
		{	
			#ifdef MTK_2BYTES_MODE
			offset = 0x00;
			buf[0] = 0x55;								// flash write command
			buf[1] = (char)(addr >> 8);					// address high 8 bit
			buf[2] = (char)(addr & 0x00FF);				// address low 8 bit
			buf[3] = 8;									// length
			//buf[4] = 0;									// checksum
			buf[5] = fw_binary_data[addr + 0];			// data 0
			buf[6] = fw_binary_data[addr + 1];			// data 1
			checksum = (~(buf[1]+buf[2]+buf[3]+buf[5]+buf[6]))+1;
			buf[4] = checksum;

			//TPD_DMESG("buf %x, %x, %x, %x, %x, %x\n", buf[0], buf[1], buf[2],buf[3],buf[4], buf[5]);
			//TPD_DMESG("buf %x, %x, %x, %x, %x, %x, %x, %x\n", buf[6], buf[7], buf[8],buf[9], buf[10], buf[11], buf[12], buf[13]);
			//TPD_DMESG(" addr =%x, checksum=%x \n", addr, checksum);
			
			ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 7);			
			#else
			offset = 0x00;
			buf[0] = 0x55;								// flash write command
			buf[1] = (char)(addr >> 8);					// address high 8 bit
			buf[2] = (char)(addr & 0x00FF);				// address low 8 bit
			buf[3] = 8;									// length
			//buf[4] = 0;									// checksum
			buf[5] = fw_binary_data[addr + 0];			// data 0
			buf[6] = fw_binary_data[addr + 1];			// data 1
			buf[7] = fw_binary_data[addr + 2];			// data 2
			buf[8] = fw_binary_data[addr + 3];			// data 3
			buf[9] = fw_binary_data[addr + 4];			// data 4
			buf[10]= fw_binary_data[addr + 5];			// data 5
			buf[11]= fw_binary_data[addr + 6];			// data 6
			buf[12]= fw_binary_data[addr + 7];			// data 7

			checksum = (~(buf[1]+buf[2]+buf[3]+buf[5]+buf[6]+buf[7]+buf[8]+buf[9]+buf[10]+buf[11]+buf[12]))+1;
			buf[4] = checksum;

			//TPD_DMESG("buf %x, %x, %x, %x, %x, %x\n", buf[0], buf[1], buf[2],buf[3],buf[4], buf[5]);
			//TPD_DMESG("buf %x, %x, %x, %x, %x, %x, %x, %x\n", buf[6], buf[7], buf[8],buf[9], buf[10], buf[11], buf[12], buf[13]);
			//TPD_DMESG(" addr =%x, checksum=%x \n", addr, checksum);
			
			ret = nvt_i2c_write_bytes(NT11004_HW_ADDR, offset, buf, 13);
			#endif
			if( ret < 0 )			
			{
				return 0;
			}

			#ifdef MTK_2BYTES_MODE
			if (i != (16*4 - 1))
			#else
			if (i != 15)
			#endif
			{
				nvt_delay_ms(1); 	//Delay 0.1ms
			}
			else
			{
				nvt_delay_ms(6);	//Delay 6ms
			}
			// write 8 bytes to flash start

			offset = 0x00;
			ret = nvt_i2c_read_bytes(NT11004_HW_ADDR, offset, buf, 1);
			if( ret < 0 )			
			{
				return 0;
			}
						
			if(buf[0] != 0xAA)
			{
				TPD_DMESG("Error buf[0] != 0xAA\n");
				return 0;
			}
			#ifdef MTK_2BYTES_MODE
			addr = addr + 2;
			#else
			addr = addr + 8;
			#endif
		}
		sector++;
	}
	while(addr < FLASH_SIZE);

	return 1;
}

// update ctp firmware
//
int nvt_fw_update(void)
{
	int ret = 0;

	TPD_DMESG("nvt_fw_update\n");

	// step 1 init boot loader
	ret = nvt_boot_loader_init();
	if( ret == 0 )   		
	{
		TPD_DMESG("Init boot loader Error;\n");
		return 0;
	}
	
	// step 2 erase flash
	ret = nvt_erase_flash();
	if( ret == 0 )   		
	{
		TPD_DMESG("Erase flash Error;\n");
		return 0;
	}

	// step 3 write data to flash
	ret = nvt_write_binary_data_to_flash(fw_binary_data, FW_BINARY_DATA_SIZE);
	if( ret == 0 )   		
	{
		TPD_DMESG("write data to flash Error;\n");
		return 0;
	}
	
	// step 4 confirm update is success
	ret = nvt_fw_compare_chexksum_style_file();
	if( ret == 0 )   		
	{
		TPD_DMESG("Checksum compare does not match;\n");
		return 0;
	}
	
	TPD_DMESG("Update firmware success!\n");
	
	return 1;
}



// function 
// return 1: need update; 0: do not need update
int nvt_fw_need_update(void)
{
	int ret = 0;
	
	ret = nvt_fw_compare_chexksum_style_file();
	if(ret != 1)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


int nvt_fw_config(void)
{
	int ret = 0;

	TPD_DMESG("nvt_fw_config\n");

	if(nvt_fw_need_update() == 1)
	{
		ret = nvt_fw_update();
	}

    nvt_reset();
	
	return ret;
}

#endif //NVT_FW_UPDATE_FUNC_SUPPORT

#ifdef NVT_CTP_ESD_CHECK_SUPPORT
static struct task_struct *tp_thread_ptr;
#ifdef NVT_APK_DRIVER_FUNC_SUPPORT
extern int nvt_apk_mode;
#endif
void nvt_esd_auto_check()
{
	unsigned char buf_send[2] = {0x55, 0x00};
	unsigned char buf_recv[2] = {0x00, 0x00};
#ifdef NVT_CTP_DEBUG_BUFF_SUPPORT
	unsigned char buf_log[30] = {0x00};
#endif

	TPD_DMESG("nvt_esd_auto_check");

#ifdef NVT_APK_DRIVER_FUNC_SUPPORT
	if(nvt_apk_mode == 1)
	{
		return;
	}
#endif
	
	i2c_write_bytes(i2c_client, 0x8C, &buf_send[0], 8);
	msleep(20);
	i2c_read_bytes(i2c_client, 0x8C, &buf_recv[0], 8);

	TPD_DMESG("buf_recv[0]=%x, buf_recv[1]=%x", buf_recv[0], buf_recv[1]);

	if((buf_recv[0] != 0x00)||(buf_recv[1] != 0xAA))
	{
		tpd_reset();
	}
#ifdef NVT_CTP_DEBUG_BUFF_SUPPORT
	else
	{
		// read 30 bytes data
		i2c_read_bytes(i2c_client, 90, &buf_log[0], 8);
		i2c_read_bytes(i2c_client, 98, &buf_log[8], 8);
		i2c_read_bytes(i2c_client, 106, &buf_log[16], 8);
		i2c_read_bytes(i2c_client, 114, &buf_log[24], 6);
		TPD_DMESG("buf_log 0 - 9: %x, %x, %x, %x, %x, %x, %x, %x, %x, %x", buf_log[0],  buf_log[1],  buf_log[2],  buf_log[3],  buf_log[4],  buf_log[5],  buf_log[6],  buf_log[7],  buf_log[8],  buf_log[9]);
		TPD_DMESG("buf_log 10-19: %x, %x, %x, %x, %x, %x, %x, %x, %x, %x", buf_log[10], buf_log[11], buf_log[12], buf_log[13], buf_log[14], buf_log[15], buf_log[16], buf_log[17], buf_log[18], buf_log[19]);
		TPD_DMESG("buf_log 20-29: %x, %x, %x, %x, %x, %x, %x, %x, %x, %x", buf_log[20], buf_log[21], buf_log[22], buf_log[23], buf_log[24], buf_log[25], buf_log[26], buf_log[27], buf_log[28], buf_log[29]);
	}
#endif
}

static int nvt_esd_check_main(void *unused)
{
	msleep(2000);
	while(!kthread_should_stop())
	{
		nvt_esd_auto_check();
		msleep(2000);
	}
}
#endif // NVT_CTP_ESD_CHECK_SUPPORT

static tpd_down(int id, int x, int y, int w, int press)
{
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

    input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
	//input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, w);
	//input_report_abs(tpd->dev, ABS_MT_PRESSURE,	press);
	input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	input_mt_sync(tpd->dev);
}

static tpd_up(int id, int x, int y, int w, int press)
{

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
    //input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);

}


void tpd_eint_interrupt_handler(void)
{
	//TPD_DEBUG("call function tpd_eint_interrupt_handler\n");
    TPD_DEBUG_PRINT_INT;
	tpd_flag=1;
	wake_up_interruptible(&waiter);
}

#ifdef TPD_HAVE_BUTTON

static int tpd_button_event(u8 buf1, u8 buf2)
{
	static int key_status = 0;
	int ret = 0;
	int i;

	TPD_DMESG("tpd_button_event\n");

	if((buf1 >> 3) == 0x1F)
	{
		switch(buf2)
		{
		// key released
		case 0x00:
			if(key_status == 1)
			{
				for(i=0; i < TPD_KEY_COUNT; i++)
				{
					#ifdef TPD_HAVE_BUTTON_TYPE_COOR
					tpd_up(0,0,0,0,0);
					#else
					input_report_key(tpd->dev, tpd_nt11004_keys[i], 0);					
					#endif
				}
				TPD_DMESG("key released\n");
				input_sync(tpd->dev);
				key_status = 0;
				ret = 1;
			}
			break;

		// key[0] pressed
		case 0x01:
			#ifdef TPD_HAVE_BUTTON_TYPE_COOR
			tpd_down(0,60,910,0,0);
			#else
			input_report_key(tpd->dev, tpd_nt11004_keys[0], 1);			
			#endif
			input_sync(tpd->dev);
			TPD_DMESG("key[0] pressed\n");
			key_status = 1;
			ret = 1;
			break;

		// key[1] pressed
		case 0x02:
			#ifdef TPD_HAVE_BUTTON_TYPE_COOR
			tpd_down(0,180,910,0,0);
			#else
			input_report_key(tpd->dev, tpd_nt11004_keys[1], 1);
			#endif
			input_sync(tpd->dev);
			TPD_DMESG("key[1] pressed\n");
			key_status = 1;
			ret = 1;
			break;

		// key[2] pressed
		case 0x04:
			#ifdef TPD_HAVE_BUTTON_TYPE_COOR
			tpd_down(0,300,910,0,0);
			#else
			input_report_key(tpd->dev, tpd_nt11004_keys[2], 1);
			#endif
			input_sync(tpd->dev);
			TPD_DMESG("key[2] pressed\n");
			key_status = 1;
			ret = 1;
			break;

		// key[3] pressed
		case 0x08:
			#ifdef TPD_HAVE_BUTTON_TYPE_COOR
			tpd_down(0,420,910,0,0);
			#else
			input_report_key(tpd->dev, tpd_nt11004_keys[3], 1);
			#endif
			input_sync(tpd->dev);
			TPD_DMESG("key[3] pressed\n");
			key_status = 1;
			ret = 1;
			break;
		}
	}

	return ret;
}

#endif

static void tpd_work_func(void)
{
    static u8 buf[16] = {0};
	int index, pos, id, points;
    int ret, ret1, ret2;
	unsigned char checksum = 0;
	unsigned int raw_x = 0, raw_y = 0;
    unsigned int x = 0, y = 0, w = 0, p = 0;

	TPD_DMESG("tpd_work_func\n");

	ret1 = i2c_read_bytes(i2c_client, 0, &buf[0], 8);
	ret2 = i2c_read_bytes(i2c_client, 8, &buf[8], 8);

#ifdef NVT_CTP_ESD_CHECK_SUPPORT
	if((ret1 != TPD_OK)||(ret2 != TPD_OK))
	{
		tpd_reset();
		return;
	}
	
	checksum = ~(buf[0]+buf[1]+buf[2]+buf[3]+buf[4]+buf[5]+buf[6]+buf[7]+buf[8]+buf[9]+buf[11]+buf[12]+buf[13])+1;
	TPD_DMESG("checksum =%x, buf[10]=%x", checksum , buf[10]);
	if(checksum != buf[10])
	{
		tpd_reset();
		return;
	}
#endif // NVT_CTP_ESD_CHECK_SUPPORT
	
	//TPD_DMESG("buf[0] = %x, buf[1] = %x, buf[2] = %x,buf[3] = %x, buf[4] = %x, buf[5] = %x\n", buf[0], buf[1], buf[2],buf[3],buf[4],buf[5]);
	//TPD_DMESG("buf[6] = %x, buf[7] = %x, buf[8] = %x,buf[9] = %x, buf[10] = %x, buf[11] = %x\n", buf[6], buf[7], buf[8],buf[9],buf[10],buf[11]);
	//TPD_DMESG("buf[12] = %x, buf[13] = %x\n", buf[12], buf[13]);

#ifdef NVT_PROXIMITY_FUNC_SUPPORT
	ret = tpd_proximity_event(buf[12], buf[13]);
#endif

#ifdef TPD_HAVE_BUTTON		
	ret = tpd_button_event(buf[12], buf[13]);
	if(ret != 1)
#endif
	{
		points = 0;
		for(index = 0; index < TPD_MAX_POINTS_NUM; index++)
		{
			pos = TPD_EACH_POINT_BYTES * index;
			id  = (buf[pos]>>3) - 1;

			//TPD_DMESG("index = %d, pos = %d, id = %d\n", index, pos, id);

			// id is the point index
			if((id >= 0) && (id < TPD_MAX_POINTS_NUM)) 
			{
				raw_x = (unsigned int)(buf[pos+1]<<4) + (unsigned int)(buf[pos+3]>>4);
				raw_y = (unsigned int)(buf[pos+2]<<4) + (unsigned int)(buf[pos+3]&0x0f);
				x = raw_x * SCREEN_MAX_WIDTH / TOUCH_MAX_WIDTH;
				y = raw_y * SCREEN_MAX_HEIGHT / TOUCH_MAX_HEIGHT; 		
				w = (unsigned int)(buf[pos+4])+127;
				p = (unsigned int)(buf[pos+5]);

				if(((buf[pos]&0x03) == 0x01) || ((buf[pos]&0x03) == 0x02))
				{
					TPD_DMESG("tpd_down, x=%d, y=%d\n", x, y);
					points++;
					tpd_down(id, x, y, w, p);
				}
			}
		}

		//TPD_DMESG("points = %d\n", points);

		if(points == 0)
		{
			TPD_DMESG("tpd_up, x=%d, y=%d\n", x, y);
			tpd_up(id, x, y, w, p);
		}
		
		input_sync(tpd->dev);
	}
}


static int tpd_event_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

	TPD_DMESG("tpd_event_handler\n");

    sched_setscheduler(current, SCHED_RR, &param);
	
	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		set_current_state(TASK_INTERRUPTIBLE);
		
		while (tpd_halt)
		{
			tpd_flag = 0;
			msleep(20);
		} 
		
		wait_event_interruptible(waiter, tpd_flag != 0);
		
		tpd_flag = 0;
		TPD_DEBUG_SET_TIME;
		set_current_state(TASK_RUNNING);

		if ( tpd == NULL || tpd->dev == NULL )
		{
			continue;
		}

		// tpd work process function
		tpd_work_func();
		
    }
	while (!kthread_should_stop());

    return TPD_OK;
}


static void nt11004_hw_init()
{
	nt11004_power_switch(POWER_ON);
	//tpd reset
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
	//setup external interrupt
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 0); 
	msleep(5);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
}


static void nt11004_hw_uninit()
{
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	nt11004_power_switch(POWER_OFF);
	//tpd reset pin
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
}



static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0, ret = -1;
    int idx = 0;
    unsigned char buf[] = {0};

	TPD_DMESG("MediaTek touch panel i2c probe\n");
    TPD_DMESG("probe handle -- novatek\n");
	
	i2c_client = client;

	nt11004_hw_init();
	
	ret = i2c_read_bytes(i2c_client, 0, &buf[0], 8);
	if(ret != TPD_OK)
	{
		goto __probe_fail;
	}

	thread = kthread_run(tpd_event_handler, 0, TPD_DEVICE);
	
    if (IS_ERR(thread))
	{
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
	
    tpd_load_status = 1;

    TPD_DMESG("MediaTek touch panel i2c probe success\n");

#ifdef NVT_PROXIMITY_FUNC_SUPPORT
    tpd_proximity_init();
#endif

#ifdef NVT_APK_DRIVER_FUNC_SUPPORT
	nvt_flash_init();
#endif

#ifdef NVT_FW_UPDATE_FUNC_SUPPORT
	nvt_fw_config();
#endif

#ifdef NVT_CTP_ESD_CHECK_SUPPORT
	tp_thread_ptr = kthread_run(nvt_esd_check_main, NULL, "TP_ESD_CHECK");
#endif // NVT_CTP_ESD_CHECK_SUPPORT

    return TPD_OK;
__probe_fail:
	nt11004_power_switch(POWER_OFF);
	return TPD_FAIL;

}

static int tpd_i2c_remove(struct i2c_client *client)
{
	TPD_DMESG("call func tpd_i2c_remove\n");
    return TPD_OK;
}

static int tpd_local_init(void)
{
	TPD_DMESG("tpd_local_init  start 0\n");
    if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
    	TPD_DMESG("unable to add i2c driver.\n");
    	return TPD_FAIL;
    }
	
	TPD_DMESG("tpd_local_init  start 1\n");
    if(tpd_load_status == 0)
    {
    	TPD_DMESG("add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return TPD_FAIL;
    }
	
	TPD_DMESG("tpd_local_init start 2\n");
#ifdef TPD_HAVE_BUTTON
	tpd_nt11004_key_init();
#endif

    TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);

    tpd_type_cap = 1;

    return TPD_OK;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct i2c_client *client, pm_message_t message)
{
	int retval = TPD_OK;
	static char data = 0xAA;
	
	TPD_DEBUG("call function tpd_suspend\n");

#ifdef NVT_PROXIMITY_FUNC_SUPPORT
	if(tpd_proximity_flag == 1)
	{
		return;
	}
#endif

	nt11004_hw_uninit();
	return retval;
}

/* Function to manage power-on resume */
static void tpd_resume(struct i2c_client *client)
{
    TPD_DEBUG("call function tpd_resume\n");

#ifdef NVT_PROXIMITY_FUNC_SUPPORT
	if(tpd_proximity_flag == 1)
	{
		return;
	}
#endif

	tpd_halt = 0;

	nt11004_hw_init();
	
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = NT11004_TS_NAME,
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
static int __init tpd_driver_init(void)
{
    TPD_DMESG("touch panel driver init tpd_driver_init\n");

#if defined(TPD_I2C_NUMBER)
    i2c_register_board_info(TPD_I2C_NUMBER, &i2c_tpd, 1);
#else
	i2c_register_board_info(0, &i2c_tpd, 0);
#endif

    if(tpd_driver_add(&tpd_device_driver) < 0)
    {
    	TPD_DMESG("add generic driver failed\n");
    }

    return TPD_OK;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
    TPD_DMESG("touch panel driver exit tpd_driver_exit\n");

    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

