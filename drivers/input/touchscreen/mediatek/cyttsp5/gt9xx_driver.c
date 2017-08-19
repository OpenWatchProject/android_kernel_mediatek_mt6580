#include "tpd.h"
#define GUP_FW_INFO
#include "tpd_custom_gt9xx.h"

#include "cust_gpio_usage.h"
#include "mt_pm_ldo.h"

#include <linux/mmprofile.h>
#include <linux/device.h>
#include <linux/proc_fs.h>	/*proc */
extern struct tpd_device *tpd;
struct regulator *tempregs = NULL;

#ifdef VELOCITY_CUSTOM
extern int tpd_v_magnify_x;
extern int tpd_v_magnify_y;
#endif
static int tpd_flag;
int tpd_halt = 0;
static int tpd_eint_mode = 1;
static int tpd_polling_time = 50;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static DEFINE_MUTEX(i2c_access);

static int power_flag=0;// 0 power off,default, 1 power on

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#if GTP_HAVE_TOUCH_KEY
const u16 touch_key_array[] = TPD_KEYS;
#define GTP_MAX_KEY_NUM ( sizeof( touch_key_array )/sizeof( touch_key_array[0] ) )
/*
struct touch_vitual_key_map_t
{
   int point_x;
   int point_y;
};
static struct touch_vitual_key_map_t touch_key_point_maping_array[]=GTP_KEY_MAP_ARRAY;
*/
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT] = TPD_WARP_END;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
/* static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX; */
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

s32 gtp_send_cfg(struct i2c_client *client);
static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
static void tpd_on(void);
static void tpd_off(void);

#if GTP_CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);
#endif

#ifdef GTP_CHARGER_DETECT
extern bool upmu_get_pchr_chrdet(void);
#define TPD_CHARGER_CHECK_CIRCLE    50
static struct delayed_work gtp_charger_check_work;
static struct workqueue_struct *gtp_charger_check_workqueue;
static void gtp_charger_check_func(struct work_struct *);
static u8 gtp_charger_mode;
#endif

#if GTP_ESD_PROTECT
#define TPD_ESD_CHECK_CIRCLE        4*HZ
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
#endif


#ifndef GTP_REG_REFRESH_RATE
#define GTP_REG_REFRESH_RATE		0x8056
#endif

struct i2c_client *i2c_client_point = NULL;
static const struct i2c_device_id tpd_i2c_id[] = { {"gt9xx", 0}, {} };
static unsigned short force[] = { 0, 0x8C, I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short *const forces[] = { force, NULL };

/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */
static struct i2c_board_info i2c_tpd __initdata = { I2C_BOARD_INFO("gt9xx", (0x8c>>1)) };

static struct i2c_driver tpd_i2c_driver = {
	.probe = tpd_i2c_probe,
	.remove = tpd_i2c_remove,
	.detect = tpd_i2c_detect,
	.driver.name = "gt9xx",
	.id_table = tpd_i2c_id,
	.address_list = (const unsigned short *)forces,
};

static u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };

#ifdef GTP_CHARGER_DETECT
static u8 config_charger[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
= { GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff };
#endif
#pragma pack(1)
typedef struct {
	u16 pid;		/* product id   // */
	u16 vid;		/* version id   // */
} st_tpd_info;
#pragma pack()

st_tpd_info tpd_info;
u8 int_type = EINTF_TRIGGER_FALLING;
u32 abs_x_max = 0;
u32 abs_y_max = 0;
u8 gtp_rawdiff_mode = 0;
u8 cfg_len = 0;


#if GTP_SUPPORT_I2C_DMA
    #include <linux/dma-mapping.h>
#endif

#if GTP_SUPPORT_I2C_DMA
static u8 *gpDMABuf_va = NULL;
static u32 gpDMABuf_pa = 0;
#endif


/* proc file system */
s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);
s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *txbuf, int len);
static struct proc_dir_entry *gt91xx_config_proc;


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
    client->addr = client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
    ret = i2c_master_send(client, &buf[0], (len << 8 | 1));
#endif

    return ret;
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
	client->addr = client->addr & I2C_MASK_FLAG;
    ret = i2c_master_send(client, &buf[0], len);
	if(ret<0)
		printk("%s error\n",__func__);
	
	return ret;
#endif
    return ret;
}




#if GTP_SUPPORT_I2C_DMA
static int cyttsp5_i2c_read_default(struct i2c_client *client, void *buf, int size,
		u8 *I2CDMABuf_va, dma_addr_t I2CDMABuf_pa)

{
	int rc;
	printk("cyttsp5_i2c_read_default\n");
	if (!buf || !size || size > GTP_DMA_MAX_TRANSACTION_LENGTH)
		return -EINVAL;
	printk("DMA1\n");


	client->ext_flag = client->ext_flag | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	rc = i2c_master_recv(client, (unsigned char *)I2CDMABuf_pa, size);
	printk("rc=%d\n",rc);
	memcpy(buf, I2CDMABuf_va, size);
	client->ext_flag = client->ext_flag
		& (~I2C_DMA_FLAG) & (~I2C_ENEXT_FLAG);

	//printk(KERN_ERR"[cyttsp5] %s,%d,r=%d\n",__func__,__LINE__,rc);
	return (rc < 0) ? rc : rc != size ? -EIO : 0;
}

#endif

/*******************************************************
Function:
	Write refresh rate

Input:
	rate: refresh rate N (Duration=5+N ms, N=0~15)

Output:
	Executive outcomes.0---succeed.
*******************************************************/
static u8 gtp_set_refresh_rate(u8 rate)
{
	u8 buf[3] = { GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff, rate };

	if (rate > 0xf) {
		GTP_ERROR("Refresh rate is over range (%d)", rate);
		return FAIL;
	}

	GTP_INFO("Refresh rate change to %d", rate);
	return gtp_i2c_write(i2c_client_point, buf, sizeof(buf));
}

/*******************************************************
Function:
	Get refresh rate

Output:
	Refresh rate or error code
*******************************************************/
static u8 gtp_get_refresh_rate(void)
{
	int ret;

	u8 buf[3] = { GTP_REG_REFRESH_RATE >> 8, GTP_REG_REFRESH_RATE & 0xff };
	ret = gtp_i2c_read(i2c_client_point, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	GTP_INFO("Refresh rate is %d", buf[GTP_ADDR_LENGTH]);
	return buf[GTP_ADDR_LENGTH];
}


/* ============================================================= */

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, "mtk-tpd");
	return 0;
}



void gt9xx_power_switch(s32 state)
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
				ret=regulator_set_voltage(tpd->reg, 2800000, 2800000);	// set 2.8v
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





/*******************************************************
Function:
	GTP initialize function.

Input:
	client:	i2c client private struct.

Output:
	Executive outcomes.0---succeed.
*******************************************************/
static s32 gtp_init_panel(struct i2c_client *client)
{
	s32 ret = -1;

	//spyder  Read 2 byte from 00h Reg, I2C address:0x24(7 bit)
	u8 read_len;
	u8 read_buf[8];
	read_len = 2;
	int i;

	//ret = i2c_smbus_read_i2c_block_data(client, 0x00, 1, &(read_buf[0]));
	ret = tpd_i2c_read(client, &read_buf[0], 1, 0x80);
	printk("gtp_init_panel,ret=%d,read_buf[0]=%x,read_buf[1]=%x\n",ret,read_buf[0],read_buf[1]);
	if(ret < 0)
	{
		printk("gtp_init_panel error!\n");
	}else
	{
		printk("gtp_init_panel OK!\n");
	}
	return ret;
}



/*******************************************************
Function:
	Set INT pin  as input for FW sync.

Note:
  If the INT is high, It means there is pull up resistor attached on the INT pin.
  Pull low the INT pin manaully for FW sync.
*******************************************************/
void gtp_int_sync(void)
{
	GTP_DEBUG("There is pull up resisitor attached on the INT pin~!");
	GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
	msleep(50);
	GTP_GPIO_AS_INT(GTP_INT_PORT);
}

void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	GTP_INFO("GTP RESET!");
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
	msleep(ms);
//	GTP_GPIO_OUTPUT(GTP_INT_PORT, client->addr == 0x14);
	msleep(2);
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);
	msleep(6);
//	gtp_int_sync();

	return;
}

static int tpd_power_on(struct i2c_client *client)
{
	int ret = 0;
	int reset_count = 0;

 reset_proc:
	GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
	GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
	msleep(10);
	/* power on, need confirm with SA */
	printk("TPd enter power on---begin-\r\n");
	gt9xx_power_switch(SWITCH_ON);
	printk("TPd enter power on---end-\r\n");

	// set INT mode
  	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	msleep(10);	

 out:
	return ret;
}


static s32 tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	s32 err = 0;
	s32 ret = 0;
	u16 version_info;
	struct task_struct *thread = NULL;

	i2c_client_point = client;
	ret = tpd_power_on(client);
	
//spyder 

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		GTP_ERROR(TPD_DEVICE " failed to create kernel thread: %d", err);
		goto out;
	}

#ifdef GTP_SUPPORT_I2C_DMA
	gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, GTP_DMA_MAX_TRANSACTION_LENGTH,
			&gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		printk("%s Allocate DMA I2C Buffer failed!\n", __func__);
	}else
	{
		printk("DMA BUFF Allocate OK\n");
	}	
 	memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#endif

//spyder EINT TRIGGER -->TRIGGER_FALLING

	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_FALLING, tpd_eint_interrupt_handler, 0);	/* disable auto-unmask */
	
#if GTP_ESD_PROTECT
		INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
		gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);


	ret = gtp_init_panel(client);
	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		goto out;
	}
	GTP_DEBUG("gtp_init_panel success");


	tpd_load_status = 1;

	GTP_INFO("%s, success run Done", __func__);
	return 0;
 out:
 	gt9xx_power_switch(SWITCH_OFF);
 	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	return -1;
}

static void tpd_eint_interrupt_handler(void)
{
	printk("###############tpd_eint_interrupt_handler############\n");
	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}

static int tpd_i2c_remove(struct i2c_client *client)
{
#if GTP_CREATE_WR_NODE
//	uninit_wr_node();
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

#if GTP_CHARGER_DETECT
	destroy_workqueue(gtp_charger_check_workqueue);
#endif
	return 0;
}


#if GTP_ESD_PROTECT
 void force_reset_guitar(void)
{
	s32 i;
	int ret = -1;
	u8 read_len;
	u8 read_buf[8];

	GTP_INFO("force_reset_guitar");
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	/* Power off TP */
	gt9xx_power_switch(SWITCH_OFF);
	msleep(30); 
	tpd_power_on(i2c_client_point);
	msleep(30);

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	
	ret = i2c_smbus_read_i2c_block_data(i2c_client_point, 0x00, 2, &(read_buf[0]));
	if(ret <0 )
	{
		printk("@@@@@@@@ERROR: cyttsp5 ESD check,after reset fail@@@@@@@@@\n");
	}else
	{
		printk("OK: cyttsp5 ESD reset OK\n");
	}

}

static void gtp_esd_check_func(struct work_struct *work)
{
	int i;
	int ret = -1;
	u8 read_len;
	u8 read_buf[8];
	struct cyttsp5_xydata xy_data;
	
	printk("gtp_esd_check_func\n");
	if (tpd_halt) {
		return;
	}

	mutex_lock(&i2c_access);
	//spyder  Read 2 byte from 00h Reg, I2C address:0x24(7 bit)
	//memset(&xy_data, 0, sizeof(xy_data));
	ret = i2c_smbus_read_i2c_block_data(i2c_client_point, 0x00, 2, &read_buf);
	printk("gtp_esd_check_func,ret=%d,read_buf[0]=%x,read_buf[1]=%x\n",ret,read_buf[0],read_buf[1]);

	if (ret < 0) {
			printk("@@@@@@@@@I2C transfer error. errno:%d @@@@@@@@@@@@@@\n", ret);
			force_reset_guitar();
	}
	else
	{
		printk("xy_data.len_l=%d\n",read_buf[0]);
		if(read_buf[0] > 2){
			printk("@@@@@@@@cyttsp5 esd check Error,eint not clear@@@@@@@@@@@@\n");
			force_reset_guitar();
		}
		else if(read_buf[0] == 2){
			printk("I2C Read len = 2 ,cyttsp5 idle OK\n");
		}
	}
	mutex_unlock(&i2c_access);

	if (!tpd_halt) {
		printk("Start esk check 2\n");
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work,
				   TPD_ESD_CHECK_CIRCLE);
	}

	return;
}
#endif
static int tpd_history_x = 0, tpd_history_y;
static void tpd_down(s32 x, s32 y, s32 size, s32 id)
{
	printk("tpd_down\n");
	if ((!size) && (!id)) {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, 100);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 100);
	} else {
		input_report_abs(tpd->dev, ABS_MT_PRESSURE, size);
		input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, size);
		/* track id Start 0 */
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);
	}

	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(x, y, x, y, id, 1);
	tpd_history_x = x;
	tpd_history_y = y;

	MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 1, x + y);
#ifdef TPD_HAVE_BUTTON

	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
		tpd_button(x, y, 1);
	}
#endif
}

static void tpd_up(s32 x, s32 y, s32 id)
{
	printk("tpd_up\n");
	/* input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0); */
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	/* input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(tpd->dev);
	TPD_DEBUG_SET_TIME;
	TPD_EM_PRINT(tpd_history_x, tpd_history_y, tpd_history_x, tpd_history_y, id, 0);
	tpd_history_x = 0;
	tpd_history_y = 0;
	MMProfileLogEx(MMP_TouchPanelEvent, MMProfileFlagPulse, 0, x + y);

#ifdef TPD_HAVE_BUTTON

	if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
		tpd_button(x, y, 0);
	}
#endif
}



static int touch_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = RTPM_PRIO_TPD };
	u8 end_cmd[3] = { GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0 };
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] =
	    { GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF };
	u8 touch_num = 0;
	u8 finger = 0;
	static u8 pre_touch;
	static u8 pre_key;
	u8 key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i = 0;
	s32 ret = -1;

	unsigned char pucPoint[14];
	unsigned char cPoint[8];
    unsigned char ePoint[6];
	int xraw, yraw;
//spyder
	struct cyttsp5_xydata xy_data;
	u8 num_cur_tch = 0;
	
#if GTP_CHANGE_X2Y
	s32 temp;
#endif

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if (tpd_eint_mode) {
			wait_event_interruptible(waiter, tpd_flag != 0);
			tpd_flag = 0;
		} else {
			msleep(tpd_polling_time);
		}
		set_current_state(TASK_RUNNING);

		mutex_lock(&i2c_access);

		if (tpd_halt) {
			mutex_unlock(&i2c_access);
			GTP_DEBUG("return for interrupt after suspend...  ");
			continue;
		}

//spyder
		printk("touch_event_handler\n");
		//stop esd check first
		#if GTP_ESD_PROTECT
		cancel_delayed_work_sync(&gtp_esd_check_work);
		#endif
		
		memset(&xy_data, 0, sizeof(xy_data));
		
		ret=i2c_smbus_read_i2c_block_data(i2c_client_point, 0x00, 2, &xy_data);
		if (ret < 0) {
			GTP_ERROR("I2C transfer error. errno:%d ", ret);
			goto exit_work_func;
		}
		printk("Get 2 byte OK,xy_data.len_l=%d\n",xy_data.len_l);
		if(xy_data.len_l > 2){
			printk("Get 2 byte OK 1,xy_data.len_l=%d\n",xy_data.len_l);
			
			ret=cyttsp5_i2c_read_default(i2c_client_point, &xy_data, xy_data.len_l, gpDMABuf_va,gpDMABuf_pa);
			if (ret < 0) {
				printk("I2C Read errno:%d ", ret);
				goto exit_work_func;
			}
			printk("Get data 3 data OK,ret=%d\n",ret);
		}
		else{
			printk("I2C Read len = 2 ");
				goto exit_work_func;
		}
		printk("Get data 4 data OK 2 \n");

		num_cur_tch =GET_NUM_TOUCHES(xy_data.rep_stat);
		if (num_cur_tch > MAX_FINGER_NUM) {
		GTP_ERROR("Bad number of fingers!");
		goto exit_work_func;
		}
		printk("num_cur_tch=%d\n",num_cur_tch);
		if (num_cur_tch){
			for (i = 0; i < num_cur_tch; i++) 
			{
				struct cyttsp5_touch *tch = &xy_data.tch[i];

				int e = CY_GET_EVENTID(tch->t);
				int t = CY_GET_TRACKID(tch->t)+1;
				int x = (tch->xh * 256) + tch->xl;
				int y = (tch->yh * 256) + tch->yl;
				int p = tch->z;
				printk("e=%d,t=%d,x=%d,y=%d,p=%d\n",e,t,x,y,p);
				if((e == 1)||(e == 2)){
					tpd_down(x, y, p, t);
				}	
				else
				{
					GTP_DEBUG("Touch Release!");
					tpd_up(0, 0, 0);
				}	
			}	
		}
		else {
			GTP_DEBUG("Touch Release!");
			tpd_up(0, 0, 0);
		} 

		if (tpd != NULL && tpd->dev != NULL) {
			input_sync(tpd->dev);
		}
		


 exit_work_func:

//spyder
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

		//start esd check
		#if GTP_ESD_PROTECT
			printk("start ESD check\n");
			queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
		#endif
		mutex_unlock(&i2c_access);

	}
	while (!kthread_should_stop());

	return 0;
}

static int tpd_local_init(void)
{
#ifdef TPD_POWER_SOURCE_CUSTOM
//#ifdef CONFIG_OF_TOUCH
#ifdef CONFIG_ARCH_MT6580

	tpd->reg=regulator_get(tpd->tpd_dev,TPD_POWER_SOURCE_CUSTOM); // get pointer to regulator structure
	if (IS_ERR(tpd->reg)) {
		GTP_ERROR("regulator_get() failed!\n");
	}
#endif
//#endif
#endif	
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		GTP_INFO("unable to add i2c driver.");
		
	#if GTP_ESD_PROTECT
			cancel_delayed_work_sync(&gtp_esd_check_work);
	#endif
		return -1;
	}

	if (tpd_load_status == 0)	/* if(tpd_load_status == 0) // disable auto load touch driver for linux3.0 porting */
	{
		GTP_INFO("add error touch panel driver.");
		
#if GTP_ESD_PROTECT
				cancel_delayed_work_sync(&gtp_esd_check_work);
#endif
		
		i2c_del_driver(&tpd_i2c_driver);
		return -1;
	}
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (GTP_MAX_TOUCH - 1), 0, 0);
#ifdef TPD_HAVE_BUTTON
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);	/* initialize tpd button data */
#endif

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4);
	memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4);
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy(tpd_calmat, tpd_def_calmat_local, 8 * 4);
	memcpy(tpd_def_calmat, tpd_def_calmat_local, 8 * 4);
#endif

	/* set vendor string */
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = tpd_info.pid;
	tpd->dev->id.version = tpd_info.vid;

	GTP_INFO("end %s, %d", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}


/*******************************************************
Function:
	Eter sleep function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_enter_sleep(struct i2c_client *client)
{
	s8 ret = -1;

	printk("Cyttp send sleep command\n");
	char buf[3] ={0x00,0x01,0x08};
	i2c_smbus_write_i2c_block_data(i2c_client_point, 0x05, 3, buf);

	GTP_INFO("GTP enter sleep!");
	return 0;

}

/*******************************************************
Function:
	Wakeup from sleep mode Function.

Input:
	client:i2c_client.

Output:
	Executive outcomes.0--success,non-0--fail.
*******************************************************/
static s8 gtp_wakeup_sleep(struct i2c_client *client)
{
	u8 retry = 0;
	s8 ret = -1;
	u8 read_len;
	u8 read_buf[8];
	read_len = 2;

	GTP_INFO("GTP wakeup begin.");


	while (retry++ < 1) 
	{
		printk("Cyttp send wakeup command\n");
		char buf[3] ={0x00,0x00,0x08};
		i2c_smbus_write_i2c_block_data(i2c_client_point, 0x05, 3, buf);
		msleep(20);


		
		ret = i2c_smbus_read_i2c_block_data(client, 0x00, 2, &(read_buf[0]));
		printk("gtp_wakeup_sleep,ret=%d,read_buf[0]=%x,read_buf[1]=%x\n",ret,read_buf[0],read_buf[1]);
		msleep(20);

		if (ret > 0) {
			printk("########Wakeup sleep OK################\n");
			return ret;
		}
		else
		{
			printk("########Wakeup sleep fail################\n");
		}
	}


	GTP_ERROR("GTP wakeup sleep failed.");
	return ret;
}

/* Function to manage low power suspend */
static void tpd_suspend(struct early_suspend *h)
{
	s32 ret = -1;
	mutex_lock(&i2c_access);
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	tpd_halt = 1;
	mutex_unlock(&i2c_access);

	ret = gtp_enter_sleep(i2c_client_point);
	if (ret < 0) {
		GTP_ERROR("GTP early suspend failed.");
	}
#if GTP_ESD_PROTECT
	cancel_delayed_work_sync(&gtp_esd_check_work);
#endif

#ifdef GTP_CHARGER_DETECT
	cancel_delayed_work_sync(&gtp_charger_check_work);
#endif
}

/* Function to manage power-on resume */
static void tpd_resume(struct early_suspend *h)
{
	s32 ret = -1;

	mutex_lock(&i2c_access);
	tpd_halt = 0;
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	mutex_unlock(&i2c_access);

	ret = gtp_wakeup_sleep(i2c_client_point);

	if (ret < 0) {
		GTP_ERROR("GTP later resume failed.");
	}

	GTP_INFO("GTP wakeup sleep.");


#if GTP_ESD_PROTECT
	queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, TPD_ESD_CHECK_CIRCLE);
#endif

#ifdef GTP_CHARGER_DETECT
	queue_delayed_work(gtp_charger_check_workqueue, &gtp_charger_check_work,
			   TPD_CHARGER_CHECK_CIRCLE);
#endif

}

static void tpd_off(void)
{

	gt9xx_power_switch(SWITCH_OFF);

	GTP_INFO("GTP enter sleep!");

	tpd_halt = 1;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
}

static void tpd_on(void)
{
	s32 ret = -1, retry = 0;

	while (retry++ < 5) {
		ret = tpd_power_on(i2c_client_point);

		if (ret < 0) {
			GTP_ERROR("I2C Power on ERROR!");
		}

		ret = gtp_send_cfg(i2c_client_point);

		if (ret > 0) {
			GTP_DEBUG("Wakeup sleep send config success.");
		}
	}
	if (ret < 0) {
		GTP_ERROR("GTP later resume failed.");
	}

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	tpd_halt = 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gt9xx",
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
	GTP_INFO("MediaTek gt91xx touch panel driver init");
#if defined(TPD_I2C_NUMBER)
	i2c_register_board_info(TPD_I2C_NUMBER, &i2c_tpd, 1);
#else
	i2c_register_board_info(1, &i2c_tpd, 1);
#endif
	if (tpd_driver_add(&tpd_device_driver) < 0)
		GTP_INFO("add generic driver failed");

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	GTP_INFO("MediaTek gt91xx touch panel driver exit");
	/* input_unregister_device(tpd->dev); */
	tpd_driver_remove(&tpd_device_driver);
}
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
