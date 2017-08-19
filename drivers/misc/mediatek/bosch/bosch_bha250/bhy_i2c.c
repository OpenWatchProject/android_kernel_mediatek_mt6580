/*!
* @section LICENSE
 * (C) Copyright 2011~2015 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
*
* @filename bhy_i2c.c
* @date     "Tue Feb 16 16:57:19 2016 +0800"
* @id       "c391153"
*
* @brief
* The implementation file for BHy I2C bus driver
*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/input.h>

#include "bhy_core.h"
#include "bs_log.h"

#define BHY_MAX_RETRY_I2C_XFER		10
#define BHY_I2C_WRITE_DELAY_TIME	1000
#define BHY_I2C_MAX_BURST_WRITE_LEN	64

#define BHY_SUPPORT_I2C_DMA 1
#if BHY_SUPPORT_I2C_DMA
    #include <linux/dma-mapping.h>

static u8 *gpDMABuf_va = NULL;
static dma_addr_t gpDMABuf_pa = 0;

#define BHY_DMA_MAX_TRANSACTION_LENGTH  255


#endif


static s32 bhy_i2c_read_internal(struct i2c_client *client,
		u8 reg, u8 *data, u16 len)
{
#if 0
	int ret, retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	if (len <= 0)
		return -EINVAL;

	for (retry = 0; retry < BHY_MAX_RETRY_I2C_XFER; retry++) {
		ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (ret >= 0)
			break;
		usleep_range(BHY_I2C_WRITE_DELAY_TIME,
				BHY_I2C_WRITE_DELAY_TIME);
	}

	return ret;
	/*int ret;
	if ((ret = i2c_master_send(client, &reg, 1)) < 0)
		return ret;
	return i2c_master_recv(client, data, len);*/
#else
	int ret,i,retry;
	
#if 0//for normal I2c transfer
	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
#else// for DMA I2c transfer
	if(1)
	{
		//DMA Write
		if(1)//if(writelen < 8  )
		{
			for (retry = 0; retry < 2; ++retry)
			{
				ret= i2c_master_send(client, &reg, 1);
				
				if (ret >= 0)
	        	{
	        		break;
	        	}else
	       		{
	        		printk("@@@@@@@@@@@@DMA  read ERROR@@@@@@,retry=%d,ret=%d\n",retry,ret);
	       		}
			}
			
		}
		else
		{
			for(i = 0 ; i < len; i++)
			{
				gpDMABuf_va[i] = data[i];
			}

			client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
		
			if((ret=i2c_master_send(client, (unsigned char *)gpDMABuf_pa, len))!=len)
				printk("### DMA ERROR %s i2c write len=%d,buffaddr=%x\n", __func__,ret,gpDMABuf_pa);

			client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);

		}
	}
	//DMA Read 
	if(len!=0)
	{
		if(0)//if (readlen <8) {
		{
			ret = i2c_master_recv(client, (unsigned char *)data, len);
		}
		else
		{

			client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
			ret = i2c_master_recv(client, (unsigned char *)gpDMABuf_pa, len);
		//	printk("gpDMABuf_va=%p\n",gpDMABuf_va);

			for(i = 0; i < len; i++)
	        {
	            data[i] = gpDMABuf_va[i];
	        }
		client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);

		}
	}
	#endif
	return ret;
#endif
}

static s32 bhy_i2c_write_internal(struct i2c_client *client,
		u8 reg, const u8 *data, u16 len)
{
#if 0
	int ret, retry;
	u8 buf[BHY_I2C_MAX_BURST_WRITE_LEN + 1];
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
	};

	if (len <= 0 || len > BHY_I2C_MAX_BURST_WRITE_LEN)
		return -EINVAL;

	buf[0] = reg;
	memcpy(&buf[1], data, len);
	msg.len = len + 1;
	msg.buf = buf;

	for (retry = 0; retry < BHY_MAX_RETRY_I2C_XFER; retry++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;
		usleep_range(BHY_I2C_WRITE_DELAY_TIME,
				BHY_I2C_WRITE_DELAY_TIME);
	}

	return ret;
#else
	int ret,retry;
	int i = 0;

   client->addr = client->addr & I2C_MASK_FLAG;
  // client->ext_flag |= I2C_DIRECTION_FLAG; 
  // client->timing = 100;
    #if 0
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);
	#else
	
	if(0)//if(writelen < 8)
	{
		
		//MSE_ERR("Sensor non-dma write timing is %x!\r\n", this_client->timing);
		ret = i2c_master_send(client, data, len);
	}
	else
	{
		gpDMABuf_va[0] = reg;
		for(i = 0 ; i < len; i++)
		{
			gpDMABuf_va[i+1] = data[i];
		}

		client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;

	for (retry = 0; retry < 2; ++retry)
	{
		if((ret=i2c_master_send(client, (unsigned char *)gpDMABuf_pa, len+1))!=len+1)
		{
		//	printk("### error  ###%s i2c write len=%d,buffaddr=%x\n", __func__,ret,gpDMABuf_pa);
		}else
		{
		//	printk("bhy_i2c_write_internal,ret=%d\n",ret);
		}
		 if (ret >= 0)
        {
        	break;
        }else
        {
        	printk("@@@@@@@@@@@@DMA write ERROR@@@@@@,retry=%d,ret=%d\n",retry,ret);
        }
		 usleep_range(BHY_I2C_WRITE_DELAY_TIME,
				BHY_I2C_WRITE_DELAY_TIME);
	}
	client->addr = client->addr & I2C_MASK_FLAG &(~ I2C_DMA_FLAG);
	} 
	#endif

	return ret;
#endif
}

static s32 bhy_i2c_read(struct device *dev, u8 reg, u8 *data, u16 len)
{
	struct i2c_client *client;
	client = to_i2c_client(dev);
	return bhy_i2c_read_internal(client, reg, data, len);
}

static s32 bhy_i2c_write(struct device *dev, u8 reg, const u8 *data, u16 len)
{
	struct i2c_client *client;
	client = to_i2c_client(dev);
	return bhy_i2c_write_internal(client, reg, data, len);
}

#ifdef CONFIG_PM
static int bhy_pm_op_suspend(struct device *dev)
{
	return bhy_suspend(dev);
}

static int bhy_pm_op_resume(struct device *dev)
{
	return bhy_resume(dev);
}

static const struct dev_pm_ops bhy_pm_ops = {
	.suspend = bhy_pm_op_suspend,
	.resume = bhy_pm_op_resume,
};
#endif

/*!
 * @brief	bhy version of i2c_probe
 */
 int bhy_probe_result = -1;
static int bhy_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *dev_id) {
	struct bhy_data_bus data_bus = {
		.read = bhy_i2c_read,
		.write = bhy_i2c_write,
		.dev = &client->dev,
		.irq = client->irq,
		.bus_type = BUS_I2C,
	};
	printk("############bhy_i2c_probe 1 \n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PERR("i2c_check_functionality error!");
		return -EIO;
	}
	printk("############bhy_i2c_probe 2 \n");



	#ifdef BHY_SUPPORT_I2C_DMA
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *)dma_alloc_coherent(&(client->dev), BHY_DMA_MAX_TRANSACTION_LENGTH,
			&gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		printk("%s Allocate DMA I2C Buffer failed!\n", __func__);
	}else
	{
		printk("DMA BUFF Allocate OK\n");
		printk("gpDMABuf_va=%p\n",gpDMABuf_va);
	}	
 	memset(gpDMABuf_va, 0, BHY_DMA_MAX_TRANSACTION_LENGTH);
#endif


	bhy_probe_result = bhy_probe(&data_bus);
	return 	bhy_probe_result;
}

static void bhy_i2c_shutdown(struct i2c_client *client)
{
}

static int bhy_i2c_remove(struct i2c_client *client)
{
	return bhy_remove(&client->dev);
}

static const struct i2c_device_id bhy_i2c_id[] = {
	{ SENSOR_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, bhy_i2c_id);

static const struct of_device_id device_of_match[] = {
	{ .compatible = "bst,bhy", },
	{}
};
MODULE_DEVICE_TABLE(of, device_of_match);

static struct i2c_driver bhy_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.of_match_table = device_of_match,
#ifdef CONFIG_PM
		.pm = &bhy_pm_ops,
#endif
	},
	.id_table = bhy_i2c_id,
	.probe = bhy_i2c_probe,
	.shutdown = bhy_i2c_shutdown,
	.remove = (bhy_i2c_remove),
};

static struct i2c_board_info __initdata i2c_BMA222 = {
	I2C_BOARD_INFO(SENSOR_NAME, 0x28)
};

static int __init bhy_i2c_driver_init(void)
{
	printk("################bhy_i2c_driver_init 1 \n");
	i2c_register_board_info(2, &i2c_BMA222, 1);
	printk("################bhy_i2c_driver_init 2 \n");
	return i2c_add_driver(&bhy_i2c_driver);
}

static void __init bhy_i2c_driver_exit(void)
{
	i2c_del_driver(&bhy_i2c_driver);
}

MODULE_AUTHOR("Contact <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("BHY I2C DRIVER");
MODULE_LICENSE("GPL v2");

module_init(bhy_i2c_driver_init);
module_exit(bhy_i2c_driver_exit);
