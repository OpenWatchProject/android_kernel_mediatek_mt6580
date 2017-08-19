/* drivers/hwmon/mt6516/amit/epl2197.c - EPL2197 ALS/PS driver
 *
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include <linux/hwmsen_helper.h>
#include "epl2197.h"
#include <linux/input/mt.h>
#include "pmic_drv.h"

#define MTK_LTE         1

#if MTK_LTE
#include <alsps.h>
#include <linux/batch.h>
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif

#endif


#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#ifdef MT6589
#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef MT6582
//#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef CONFIG_ARCH_MT6580
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#endif


#if defined(MT6735) || defined(MT6752)
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

//add for fix resume issue
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
//add for fix resume issue end


#include <cust_alsps.h>
#include <linux/regulator/consumer.h>


/******************************************************************************
 * extern functions
*******************************************************************************/
#ifdef MT6575
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif


#ifdef MT6589
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif

#ifdef MT6582
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);
//extern void mt_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
//                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
//                                     kal_bool auto_umask);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

#endif

#ifdef CONFIG_ARCH_MT6580
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif

#if defined(MT6735) || defined(MT6752)

extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void mt_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt_eint_set_sens(kal_uint8 eintno, kal_bool sens);
//extern void mt_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
//                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
//                                     kal_bool auto_umask);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);

#endif


/*-------------------------MT6516&MT6575 define-------------------------------*/
#ifdef MT6575
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif
#ifdef MT6589
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif
#ifdef CONFIG_ARCH_MT6580
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif
#ifdef MT6582
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif
#if defined(MT6735) || defined(MT6752)
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

/******************************************************************************
 *  configuration
 ******************************************************************************/
uint8_t hs_integ 		=EPL_INTEG_40; // same as HR_INTEG_MIN 
uint8_t HR_INTEG_MIN   	=EPL_INTEG_40;
uint8_t HR_INTEG_MAX   	=EPL_INTEG_2000;

#define HR_TH_HIGH		58000
#define HR_TH_LOW		25000
/******************************************************************************
*******************************************************************************/

#define TXBYTES 		    2
#define RXBYTES			    2

#define PACKAGE_SIZE 		8
#define I2C_RETRY_COUNT 	2

#define EPL2197_DEV_NAME   	"EPL2197"
#define DRIVER_VERSION      "v1.01"

static struct mutex sensor_mutex;

typedef struct _epl_raw_data
{
    u8 raw_bytes[PACKAGE_SIZE];
    u8 buffer[8];
    u16 renvo;
    u32 hs_data;
} epl_raw_data;

/*----------------------------------------------------------------------------*/
#define APS_TAG                 	"[ALS/PS] "
#define APS_FUN(f)              	printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    	printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    	printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    	printk(KERN_INFO fmt, ##args)

/*----------------------------------------------------------------------------*/
static struct i2c_client *epl2197_i2c_client = NULL;


/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl2197_i2c_id[] = {{"EPL2197",0},{}};
static struct i2c_board_info __initdata i2c_EPL2197= { I2C_BOARD_INFO("EPL2197", (0x82>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short epl2197_force[] = {0x00, 0x92, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const epl2197_forces[] = { epl2197_force, NULL };
//static struct i2c_client_address_data epl2197_addr_data = { .forces = epl2197_forces,};


/*----------------------------------------------------------------------------*/
static int epl2197_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int epl2197_i2c_remove(struct i2c_client *client);
static int epl2197_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);


/*----------------------------------------------------------------------------*/
static int epl2197_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int epl2197_i2c_resume(struct i2c_client *client);

static void epl2197_eint_func(void);
static struct epl2197_priv *g_epl2197_ptr = NULL;

/*----------------------------------------------------------------------------*/
typedef enum
{
    CMC_BIT_ALS   	= 1,
    CMC_BIT_PS     	= 2,
    CMC_BIT_HS  	= 4,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct epl2197_i2c_addr      /*define a series of i2c slave address*/
{
    u8  write_addr;
    u8  ps_thd;     /*PS INT threshold*/
};

/*----------------------------------------------------------------------------*/
struct epl2197_priv
{
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct delayed_work  eint_work;
    struct delayed_work  polling_work;
    struct input_dev *gs_input_dev;

    /*i2c address group*/
    struct epl2197_i2c_addr  addr;

    int 	polling_mode_hs;
    int		ir_type;

    /*misc*/
    atomic_t    trace;
    atomic_t	hs_suspend;

    /*data*/
    ulong       enable;         	/*record HAL enalbe status*/
    ulong      	pending_intr;   	/*pending interrupt*/

    /*data*/
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif
};



/*----------------------------------------------------------------------------*/
static struct i2c_driver epl2197_i2c_driver =
{
    .probe     	= epl2197_i2c_probe,
    .remove     = epl2197_i2c_remove,
    .detect     = epl2197_i2c_detect,
    .suspend    = epl2197_i2c_suspend,
    .resume     = epl2197_i2c_resume,
    .id_table   = epl2197_i2c_id,
    //.address_data = &epl2197_addr_data,
    .driver = {
        //.owner          = THIS_MODULE,
        .name           = EPL2197_DEV_NAME,
    },
};


static struct epl2197_priv *epl2197_obj = NULL;
static struct platform_driver epl2197_alsps_driver;
static struct wake_lock ps_lock;
static epl_raw_data	gRawData;


static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
	.polling_mode_ps = 0,		          /* not work, define in epl8882.c */
	.power_id   = MT6325_POWER_LDO_VGP1,      /* LDO is not used */
	.power_vol  = 3300000,            /* LDO is not used */
	.i2c_addr   = {0x82, 0x48, 0x78, 0x00},
	.als_level	= {20, 45, 70, 90, 150, 300, 500, 700, 1150, 2250, 4500, 8000, 15000, 30000, 50000},
	.als_value	= {10, 30, 60, 80, 100, 200, 400, 600, 800, 1500, 3000, 6000, 10000, 20000, 40000, 60000},
	.ps_threshold_low = 1300,
	.ps_threshold_high = 2200,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}




#if MTK_LTE
int alsps_init_flag =-1; // 0<==>OK -1 <==> fail
static int alsps_local_init(void);
static int alsps_remove();
static struct alsps_init_info epl_sensor_init_info = {
		.name = EPL2197_DEV_NAME,
		.init = alsps_local_init,
		.uninit = alsps_remove,

};
#endif




extern struct device *sensor_device_1;;
bool sensor_hwPowerOn(char * powerId, int powerVolt){
    bool ret = false;
	struct regulator * temp = NULL;
	printk("[_hwPowerOn]before get, powerId:%s\n", powerId);
	
	temp = regulator_get(sensor_device_1,powerId);

	printk("[_hwPowerOn]after get, powerId:%s, regVCAM: %p\n", powerId, temp );


 	if(!IS_ERR(temp)){
        if(regulator_set_voltage(temp , powerVolt,powerVolt)!=0 ){
            printk("[_hwPowerOn]fail to regulator_set_voltage, powerVolt:%d, powerID: %s\n", powerVolt, powerId);
        }
        if(regulator_enable(temp)!= 0) {
            printk("[_hwPowerOn]fail to regulator_enable, powerVolt:%d, powerID: %s\n", powerVolt, powerId);
            //regulator_put(regVCAM);
            //regVCAM = NULL;
            return ret;
        }
        ret = true;
    }else
   		 printk("[_hwPowerOn]IS_ERR_OR_NULL regVCAM %s\n",powerId);

    return ret;
}

bool sensor_hwPowerDown(char * powerId){
    bool ret = false;
	struct regulator * temp = NULL;
	temp = regulator_get(sensor_device_1,powerId);
    if(!IS_ERR(temp)){
		#if 1
        if(regulator_is_enabled(temp)) {
            printk("[_hwPowerDown]before disable %s is enabled\n", powerId);
        }
		#endif
		if(regulator_disable(temp)!=0)
			 printk("[_hwPowerDown]fail to regulator_disable, powerID: %s\n", powerId);
		//for SMT stage, put seems always fail?
       // regulator_put(regVCAM);
       // regVCAM = NULL;
        ret = true;
    } else {
        printk("[_hwPowerDown]%s fail to power down  due to regVCAM == NULL regVCAM 0x%p\n", powerId,temp );
    }
    return ret;
}










/*
//====================I2C write operation===============//
//regaddr: ELAN epl2197 Register Address.
//bytecount: How many bytes to be written to epl2197 register via i2c bus.
//txbyte: I2C bus transmit byte(s). Single byte(0X01) transmit only slave address.
//data: setting value.
//
// Example: If you want to write single byte to 0x1D register address, show below
//	      epl2197_I2C_Write(client,0x1D,0x01,0X02,0xff);
//
*/
static int epl2197_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount, uint8_t txbyte, uint8_t data)
{
    uint8_t buffer[2];
    int ret = 0;
    int retry;

    buffer[0] = (regaddr<<3) | bytecount ;
    buffer[1] = data;

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_send(client, buffer, txbyte);

        if (ret == txbyte)
        {
            break;
        }

        APS_ERR("i2c write error,TXBYTES %d\n",ret);
        mdelay(10);
    }


    if(retry>=I2C_RETRY_COUNT)
    {
        APS_ERR("i2c write retry over %d\n", I2C_RETRY_COUNT);
        return -EINVAL;
    }

    return ret;
}



/*
//====================I2C read operation===============//
*/
static int epl2197_I2C_Read(struct i2c_client *client)
{
    uint8_t buffer[RXBYTES];
    int ret = 0, i =0;
    int retry;

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_recv(client, buffer, RXBYTES);

        if (ret == RXBYTES)
            break;

        APS_ERR("i2c read error,RXBYTES %d\r\n",ret);
        mdelay(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        APS_ERR("i2c read retry over %d\n", I2C_RETRY_COUNT);
        return -EINVAL;
    }

    for(i=0; i<RXBYTES; i++)
        gRawData.raw_bytes[i] = buffer[i];

    return ret;
}


static int elan_epl2197_I2C_Read_long(struct i2c_client *client, int bytecount)
{
    uint8_t buffer[bytecount];
    int ret = 0, i =0;
    int retry;

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_recv(client, buffer, bytecount);

        if (ret == bytecount)
            break;

        APS_ERR("i2c read error,RXBYTES %d\r\n",ret);
        mdelay(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        APS_ERR("i2c read retry over %d\n", I2C_RETRY_COUNT);
        return -EINVAL;
    }

    for(i=0; i<bytecount; i++)
        gRawData.raw_bytes[i] = buffer[i];

    return ret;
}

static void epl2197_hs_enable(struct epl2197_priv *epld)
{
    uint8_t regdata = 0;
    struct i2c_client *client = epld->client;

    regdata =  EPL_INT_CH1 | EPL_IR_ENABLE | EPL_INT_DISABLE;
    epl2197_I2C_Write(client,REG_6,W_SINGLE_BYTE,0x02, regdata);

    regdata =  EPL_DRIVE_200MA | EPL_IR_MODE_CURRENT;
    epl2197_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,regdata);

    regdata = EPL_MODE_HR | EPL_OSR_2048 | EPL_L_GAIN;
    epl2197_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

    regdata = hs_integ | EPL_FILTER_4;
    epl2197_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

    epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0X02,EPL_C_RESET);
    epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
}


static void epl2197_read_hs(void)
{
    struct epl2197_priv *epld = epl2197_obj;
    struct i2c_client *client = epld->client;
    uint32_t raw, normalized_raw, ch0;

    epl2197_I2C_Write(client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl2197_I2C_Read_long(client, 2);
    ch0=(gRawData.raw_bytes[1]<<8)|gRawData.raw_bytes[0];

    epl2197_I2C_Write(client,REG_18,R_TWO_BYTE,0x01,0x00);
    elan_epl2197_I2C_Read_long(client, 2);
    raw=(gRawData.raw_bytes[1]<<8)|gRawData.raw_bytes[0];

    normalized_raw = raw >> 4;
    normalized_raw = normalized_raw * INTEG_ARRAY[HR_INTEG_MAX];
    normalized_raw = normalized_raw / INTEG_ARRAY[hs_integ];
	
    APS_LOG("raw %d, normalized raw %d, ch0 %d, int %d\n", raw, normalized_raw, ch0, hs_integ);
    gRawData.hs_data = normalized_raw;
	
    if((raw+ch0) > HR_TH_HIGH && hs_integ>HR_INTEG_MIN)
    {
        hs_integ-=4;
		hs_integ = hs_integ<HR_INTEG_MIN?HR_INTEG_MIN:hs_integ;
        epl2197_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02, hs_integ | EPL_FILTER_4);
        epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0X02,EPL_C_RESET);   
        epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
    }
    else if(raw < HR_TH_LOW && hs_integ<HR_INTEG_MAX)
    {
        hs_integ++;
        epl2197_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,hs_integ | EPL_FILTER_4);
        epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0X02,EPL_C_RESET);  
        epl2197_I2C_Write(client,REG_8,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);		
    } 
}




/*----------------------------------------------------------------------------*/
static void epl2197_dumpReg(struct i2c_client *client)
{
    APS_LOG("chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
    APS_LOG("chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
    APS_LOG("chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
    APS_LOG("chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
    APS_LOG("chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
    APS_LOG("chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
    APS_LOG("chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
    APS_LOG("chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
    APS_LOG("chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
    APS_LOG("chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
    APS_LOG("chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
    APS_LOG("chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
    APS_LOG("chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
    APS_LOG("chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
    APS_LOG("chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));

}


/*----------------------------------------------------------------------------*/
int hw8k_init_device(struct i2c_client *client)
{
    APS_LOG("hw8k_init_device.........\r\n");

    epl2197_i2c_client=client;

    APS_LOG(" I2C Addr==[0x%x],line=%d\n",epl2197_i2c_client->addr,__LINE__);

    return 0;
}

/*----------------------------------------------------------------------------*/
int epl2197_get_addr(struct alsps_hw *hw, struct epl2197_i2c_addr *addr)
{
    if(!hw || !addr)
    {
        return -EFAULT;
    }
    addr->write_addr= hw->i2c_addr[0];
    return 0;
}


/*----------------------------------------------------------------------------*/
static void epl2197_power(struct alsps_hw *hw, unsigned int on)
{
    static unsigned int power_on = 0;

    printk("epl2197_power ,on=%d\n",on);
#if 1 //ndef MT6580
    if(hw->power_id != POWER_NONE_MACRO)
    {
        if(power_on == on)
        {
            APS_LOG("ignore power control: %d\n", on);
        }
        else if(on)
        {
        	 printk("hw->power_id=%s\n",PMIC_APP_PROXIMITY_SENSOR_VDD);
            if(!sensor_hwPowerOn(PMIC_APP_PROXIMITY_SENSOR_VDD, hw->power_vol))
            {
                APS_ERR("power on fails!!\n");
            }
        }
        else
        {
            if(!sensor_hwPowerDown(PMIC_APP_PROXIMITY_SENSOR_VDD))
            {
                APS_ERR("power off fail!!\n");
            }
        }
    }
    power_on = on;

#endif
}



/*----------------------------------------------------------------------------*/
static int epl2197_check_intr(struct i2c_client *client)
{
    APS_LOG("int pin = %d\n", mt_get_gpio_in(GPIO_ALS_EINT_PIN));
    return 0;
}


/*----------------------------------------------------------------------------*/
void epl2197_restart_polling(void)
{
    struct epl2197_priv *obj = epl2197_obj;
    //bool queue_flag = work_busy(&obj->polling_work);
    cancel_delayed_work(&obj->polling_work);
    //APS_LOG("[%s]: queue_flag=%d \r\n", __func__, (int)queue_flag);
    schedule_delayed_work(&obj->polling_work, msecs_to_jiffies(100));
}


void epl2197_polling_work(struct work_struct *work)
{
    struct epl2197_priv *obj = epl2197_obj;
    struct i2c_client *client = obj->client;

    bool enable_hs = test_bit(CMC_BIT_HS, &obj->enable) && atomic_read(&obj->hs_suspend)==0;

    APS_LOG("hs enable: %d \n", (int)enable_hs);

    cancel_delayed_work(&obj->polling_work);

    if(enable_hs)
    {
        schedule_delayed_work(&obj->polling_work, msecs_to_jiffies(100)); // 10 Hz
        epl2197_read_hs();
    }
    else
    {
        APS_LOG("disable sensor\n");
        cancel_delayed_work(&obj->polling_work);
        epl2197_I2C_Write(client,REG_0,W_SINGLE_BYTE,0x02,EPL_MODE_HRS);
    }

}


/*----------------------------------------------------------------------------*/
void epl2197_eint_func(void)
{
    struct epl2197_priv *obj = g_epl2197_ptr;

    // APS_LOG(" interrupt fuc\n");

    if(!obj)
    {
        return;
    }

#if defined(MT6582) || defined(MT6592) || defined(MT6752) || defined(MT6735)
    mt_eint_mask(CUST_EINT_ALS_NUM);
#else
    mt_eint_mask(CUST_EINT_ALS_NUM);
#endif

    schedule_delayed_work(&obj->eint_work, 0);
}



/*----------------------------------------------------------------------------*/
static void epl2197_eint_work(struct work_struct *work)
{
#ifdef MT6575
    mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef MT6589
    mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef MT6582
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef CONFIG_ARCH_MT6580
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#if defined(MT6752) || defined(MT6735)
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif
}



/*----------------------------------------------------------------------------*/
int epl2197_setup_eint(struct i2c_client *client)
{
    struct epl2197_priv *obj = i2c_get_clientdata(client);

    APS_LOG("epl2197_setup_eint\n");


    g_epl2197_ptr = obj;

    /*configure to GPIO function, external interrupt*/

    mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#ifdef  MT6575
    mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
    mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
    mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
    mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2197_eint_func, 0);
    mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef  MT6589
    mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
    mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
    mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
    mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2197_eint_func, 0);
    mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef MT6582
    //mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, epl2197_eint_func, 0);
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#ifdef CONFIG_ARCH_MT6580
    mt_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
    mt_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
    mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl2197_eint_func, 0);
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif

#if defined(MT6752) || defined(MT6735)
    mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, epl2197_eint_func, 0);
    mt_eint_unmask(CUST_EINT_ALS_NUM);
#endif
    return 0;
}




/*----------------------------------------------------------------------------*/
static int epl2197_init_client(struct i2c_client *client)
{
    struct epl2197_priv *obj = i2c_get_clientdata(client);
    int err=0;

    APS_LOG("[Agold spl] I2C Addr==[0x%x],line=%d\n",epl2197_i2c_client->addr,__LINE__);

    /*  interrupt mode */


    APS_FUN();

    if(obj->hw->polling_mode_ps == 0)
    {
#if defined(MT6582) || defined(MT6592) || defined(MT6752) || defined(MT6735)
        mt_eint_mask(CUST_EINT_ALS_NUM);
#else
        mt_eint_mask(CUST_EINT_ALS_NUM);
#endif

        if((err = epl2197_setup_eint(client)))
        {
            APS_ERR("setup eint: %d\n", err);
            return err;
        }
        APS_LOG("epl2197 interrupt setup\n");
    }


    if((err = hw8k_init_device(client)) != 0)
    {
        APS_ERR("init dev: %d\n", err);
        return err;
    }


    if((err = epl2197_check_intr(client)))
    {
        APS_ERR("check/clear intr: %d\n", err);
        return err;
    }

	printk("epl2197_init_client OK \n");
    /*  interrupt mode */
//if(obj->hw->polling_mode_ps == 0)
    //     mt65xx_eint_unmask(CUST_EINT_ALS_NUM);

    return err;
}


/*----------------------------------------------------------------------------*/
static ssize_t epl2197_show_reg(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    struct i2c_client * client;
	
    if(!epl2197_obj)
    {
        APS_ERR("epl2197_obj is null!!\n");
        return 0;
    }
	client = epl2197_obj->client;

    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));
    len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x17 value = %8x\n", i2c_smbus_read_byte_data(client, 0xb8));

    return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl2197_show_status(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;

    if(!epl2197_obj)
    {
        APS_ERR("epl2197_obj is null!!\n");
        return 0;
    }
    len += snprintf(buf+len, PAGE_SIZE-len, "chip is %s, ver is %s \n", EPL2197_DEV_NAME, DRIVER_VERSION);
    len += snprintf(buf+len, PAGE_SIZE-len, "heart int time: %d\n", hs_integ);

    return len;
}


/*----------------------------------------------------------------------------*/
static ssize_t epl2197_show_renvo(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "%x",gRawData.renvo);
    return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2197_store_hs_enable(struct device_driver *ddri, const char *buf, size_t count)
{
    uint16_t mode=0;
    struct epl2197_priv *obj = epl2197_obj;
    APS_FUN();

    sscanf(buf, "%hu",&mode);

    if(mode){
        set_bit(CMC_BIT_HS, &obj->enable);
        epl2197_hs_enable(obj);
    }
    else{
        clear_bit(CMC_BIT_HS, &obj->enable);
    }

    epl2197_restart_polling();
    return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2197_store_hs_int_time(struct device_driver *ddri, const char *buf, size_t count)
{
    APS_FUN();
    sscanf(buf, "%hhx %hhx", &HR_INTEG_MIN, &HR_INTEG_MAX);
    return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl2197_show_hs_raw(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", gRawData.hs_data);
    return len;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(elan_status,				S_IROTH  | S_IWOTH, epl2197_show_status,  	  	NULL				);
static DRIVER_ATTR(elan_reg,    			S_IROTH  | S_IWOTH, epl2197_show_reg,   		NULL				);
static DRIVER_ATTR(elan_renvo,    			S_IROTH  | S_IWOTH, epl2197_show_renvo,   		NULL				);
static DRIVER_ATTR(hs_enable,				S_IROTH  | S_IWOTH, NULL,   				epl2197_store_hs_enable		);
static DRIVER_ATTR(hs_int_time,				S_IROTH  | S_IWOTH, NULL,   				epl2197_store_hs_int_time	);
static DRIVER_ATTR(hs_raw,				S_IROTH  | S_IWOTH, epl2197_show_hs_raw, 	  	NULL				);

/*----------------------------------------------------------------------------*/
static struct driver_attribute * epl2197_attr_list[] =
{
    &driver_attr_elan_status,
    &driver_attr_elan_reg,
    &driver_attr_elan_renvo,
    &driver_attr_hs_enable,
    &driver_attr_hs_int_time,
    &driver_attr_hs_raw,
};

/*----------------------------------------------------------------------------*/
static int epl2197_create_attr(struct device_driver *driver)
{
    int idx, err = 0;
    int num = (int)(sizeof(epl2197_attr_list)/sizeof(epl2197_attr_list[0]));
    if (driver == NULL)
    {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++)
    {
        if((err = driver_create_file(driver, epl2197_attr_list[idx])))
        {
            APS_ERR("driver_create_file (%s) = %d\n", epl2197_attr_list[idx]->attr.name, err);
            break;
        }
    }
    return err;
}



/*----------------------------------------------------------------------------*/
static int epl2197_delete_attr(struct device_driver *driver)
{
    int idx ,err = 0;
    int num = (int)(sizeof(epl2197_attr_list)/sizeof(epl2197_attr_list[0]));

    if (!driver)
        return -EINVAL;

    for (idx = 0; idx < num; idx++)
    {
        driver_remove_file(driver, epl2197_attr_list[idx]);
    }

    return err;
}



/******************************************************************************
 * Function Configuration
******************************************************************************/
static int epl2197_open(struct inode *inode, struct file *file)
{
    file->private_data = epl2197_i2c_client;

    APS_FUN();

    if (!file->private_data)
    {
        APS_ERR("null pointer!!\n");
        return -EINVAL;
    }

    return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int epl2197_release(struct inode *inode, struct file *file)
{
    APS_FUN();
    file->private_data = NULL;
    return 0;
}

/*----------------------------------------------------------------------------*/
static long epl2197_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;
}


/*----------------------------------------------------------------------------*/
static struct file_operations epl2197_fops =
{
    .owner = THIS_MODULE,
    .open = epl2197_open,
    .release = epl2197_release,
    .unlocked_ioctl = epl2197_unlocked_ioctl,
};


/*----------------------------------------------------------------------------*/
static struct miscdevice epl2197_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "als_ps",
    .fops = &epl2197_fops,
};


/*----------------------------------------------------------------------------*/
static int epl2197_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
    APS_FUN();
    return 0;
}

/*----------------------------------------------------------------------------*/
static int epl2197_i2c_resume(struct i2c_client *client)
{
	APS_FUN();
    return 0;
}

/*----------------------------------------------------------------------------*/
static void epl2197_early_suspend(struct early_suspend *h)
{
    APS_FUN();
}

/*----------------------------------------------------------------------------*/
static void epl2197_late_resume(struct early_suspend *h)
{
	APS_FUN();
}

/*----------------------------------------------------------------------------*/
static int epl2197_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    strcpy(info->type, EPL2197_DEV_NAME);
    return 0;
}

/*----------------------------------------------------------------------------*/
static int epl2197_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct epl2197_priv *obj;
    int err = 0;
#if MTK_LTE
    struct als_control_path als_ctl={0};
	struct als_data_path als_data={0};
	struct ps_control_path ps_ctl={0};
	struct ps_data_path ps_data={0};
#endif
    APS_FUN();
	printk("epl2197_i2c_probe\n");

	client->timing = 400;
    epl2197_dumpReg(client);

    if(i2c_smbus_read_byte_data(client, 0xb8) != 0x88){
        printk("elan 2197 ALS/PS sensor is failed. \n");
        goto exit;
    }
	printk("epl2197_i2c_probe OK\n");

    if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
    {
        err = -ENOMEM;
        goto exit;
    }

    memset(obj, 0, sizeof(*obj));

    epl2197_obj = obj;
    obj->hw = get_cust_alsps_hw();

    epl2197_get_addr(obj->hw, &obj->addr);

    INIT_DELAYED_WORK(&obj->eint_work, epl2197_eint_work);
    INIT_DELAYED_WORK(&obj->polling_work, epl2197_polling_work);
    wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");

    obj->client = client;

    mutex_init(&sensor_mutex);

    i2c_set_clientdata(client, obj);

    atomic_set(&obj->trace, 0x00);
    atomic_set(&obj->hs_suspend, 0);

    obj->enable = 0;
    obj->pending_intr = 0;
    obj->ir_type=0;

    epl2197_i2c_client = client;
	printk("1111111111111\n");
    epl2197_I2C_Write(client,REG_0,W_SINGLE_BYTE,0x02,EPL_MODE_HRS);
	printk("222222222222222\n");

    epl2197_I2C_Write(client,0x17,R_TWO_BYTE,0x01,0x00);
	printk("33333333333333333\n");
    epl2197_I2C_Read(client);
	printk("44444444444444\n");
    gRawData.renvo = (gRawData.raw_bytes[1]<<8)|gRawData.raw_bytes[0];

    if((err = epl2197_init_client(client)))
    {
        goto exit_init_failed;
    }

#if 0
    if((err = misc_register(&epl2197_device)))
    {
        APS_ERR("epl2197_device register failed\n");
        goto exit_misc_device_register_failed;
    }
#endif
#if MTK_LTE
    if((err = epl2197_create_attr(&epl_sensor_init_info.platform_diver_addr->driver)))
    {
        APS_ERR("create attribute err = %d\n", err);
        goto exit_create_attr_failed;
    }
#else
    if((err = epl2197_create_attr(&epl2197_alsps_driver.driver)))
    {
        APS_ERR("create attribute err = %d\n", err);
        goto exit_create_attr_failed;
    }
#endif

#if 0 //MTK_LTE
    als_ctl.open_report_data= als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay  = als_set_delay;
	als_ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	als_ctl.is_support_batch = true;
#else
    als_ctl.is_support_batch = false;
#endif

	err = als_register_control_path(&als_ctl);
	if(err)
	{
		APS_ERR("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if(err)
	{
		APS_ERR("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}


	ps_ctl.open_report_data= ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay  = ps_set_delay;
	ps_ctl.is_report_input_direct = obj->hw->polling_mode_ps==0? true:false; //false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ps_ctl.is_support_batch = true;
#else
    ps_ctl.is_support_batch = false;
#endif

	err = ps_register_control_path(&ps_ctl);
	if(err)
	{
		APS_ERR("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if(err)
	{
		APS_ERR("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}
#else


#endif


#if defined(CONFIG_HAS_EARLYSUSPEND)
    obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
                   obj->early_drv.suspend  = epl2197_early_suspend,
                                  obj->early_drv.resume   = epl2197_late_resume,
                                                 register_early_suspend(&obj->early_drv);
#endif

    if(obj->hw->polling_mode_ps ==0 || obj->polling_mode_hs == 0)
        epl2197_setup_eint(client);
#if MTK_LTE
    alsps_init_flag = 0;
#endif
    printk("#######%s: OK########\n", __func__);
    return 0;

exit_create_attr_failed:
    misc_deregister(&epl2197_device);
exit_misc_device_register_failed:
exit_init_failed:
    //i2c_detach_client(client);
//	exit_kfree:
    kfree(obj);
exit:
    epl2197_i2c_client = NULL;
#if MTK_LTE
    alsps_init_flag = -1;
#endif
    APS_ERR("%s: err = %d\n", __func__, err);
    return err;



}


static int epl_sensor_delete_attr(struct device_driver *driver)
{
    int idx ,err = 0;


    return err;
}

/*----------------------------------------------------------------------------*/
static int epl2197_i2c_remove(struct i2c_client *client)
{
    int err;
#if MTK_LTE
    if((err = epl_sensor_delete_attr(&epl_sensor_init_info.platform_diver_addr->driver)))
    {
        APS_ERR("epl_sensor_delete_attr fail: %d\n", err);
    }
#else
    if((err = epl2197_delete_attr(&epl2197_i2c_driver.driver)))
    {
        APS_ERR("epl2197_delete_attr fail: %d\n", err);
    }
#endif
    if((err = misc_deregister(&epl2197_device)))
    {
        APS_ERR("misc_deregister fail: %d\n", err);
    }

    epl2197_i2c_client = NULL;
    i2c_unregister_device(client);
    kfree(i2c_get_clientdata(client));

    return 0;
}


#if !MTK_LTE
/*----------------------------------------------------------------------------*/
static int epl2197_probe(struct platform_device *pdev)
{
    struct alsps_hw *hw = get_cust_alsps_hw();
	printk("epl2197_probe\n");
    epl2197_power(hw, 1);

    //epl2197_force[0] = hw->i2c_num;

    if(i2c_add_driver(&epl2197_i2c_driver))
    {
        printk("add driver error\n");
        return -1;
    }
    return 0;
}



/*----------------------------------------------------------------------------*/
static int epl2197_remove(struct platform_device *pdev)
{
    struct alsps_hw *hw = get_cust_alsps_hw();
    APS_FUN();
    epl2197_power(hw, 0);

    APS_ERR("EPL2197 remove \n");
    i2c_del_driver(&epl2197_i2c_driver);
    return 0;
}



/*----------------------------------------------------------------------------*/
static struct platform_driver epl2197_alsps_driver =
{
    .probe      = epl2197_probe,
    .remove     = epl2197_remove,
    .driver     = {
        .name  = "als_ps",
        //.owner = THIS_MODULE,
    }
};
#endif

#if MTK_LTE
/*----------------------------------------------------------------------------*/
static int alsps_local_init(void)
{
    struct alsps_hw *hw = get_cust_alsps_hw();
	printk("alsps_local_init+++\n");

//	epl2197_power(hw, 1);
	printk("alsps_local_init 22222\n");

	if(i2c_add_driver(&epl2197_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	}
	
	if(-1 == alsps_init_flag)
	{
	printk("alsps_local_init+++ error\n");
	   return -1;
	}
	printk("alsps_local_init+++ OK\n");
	//printk("fwq loccal init---\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int alsps_remove()
{
    struct alsps_hw *hw = get_cust_alsps_hw();
    APS_FUN();
    epl2197_power(hw, 0);

    APS_ERR("epl2197 remove \n");

    i2c_del_driver(&epl2197_i2c_driver);
    return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int __init epl2197_init(void)
{
    struct alsps_hw *hw = get_cust_alsps_hw();
    APS_FUN();
	printk("epl2197_init\n");
	printk("epl2197_init 11\n");
    i2c_register_board_info(hw->i2c_num, &i2c_EPL2197, 1);
	printk("epl2197_init 1\n");
#if MTK_LTE
	epl2197_power(hw, 1);
    alsps_driver_add(&epl_sensor_init_info);
#else
	printk("epl2197_init 2\n");
    if(platform_driver_register(&epl2197_alsps_driver))
    {
    	printk("epl2197_init 3\n");
        printk("failed to register driver");
        return -ENODEV;
    }
	printk("epl2197_init 4\n");
#endif
	printk("epl2197_init 5\n");
    return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit epl2197_exit(void)
{
    APS_FUN();
    //platform_driver_unregister(&epl2197_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(epl2197_init);
module_exit(epl2197_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("renato.pan@eminent-tek.com");
MODULE_DESCRIPTION("EPL2197 ALPsr driver");
MODULE_LICENSE("GPL");



