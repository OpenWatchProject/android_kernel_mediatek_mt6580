/*!
* @section LICENSE
 * (C) Copyright 2011~2015 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
*
* @filename bhy_core.c
* @date     "Tue Feb 16 16:57:19 2016 +0800"
* @id       "c391153"
*
* @brief
* The implementation file for BHy driver core
*/

#define DRIVER_VERSION "1.3.11.0"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/string.h>

#include <linux/time.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/swab.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/firmware.h>
#include <linux/hrtimer.h>

#include "bhy_core.h"
#include "bhy_host_interface.h"
#include "bs_log.h"

#include <mach/mt_gpio.h>
#include <mach/eint.h>
#include "cust_gpio_usage.h"
#include "cust_eint.h"


struct bhy_client_data *client_data_1 = NULL;

#ifdef BHY_DEBUG
static s64 g_ts[4]; /* For fw load time test */
#endif /*~ BHY_DEBUG */

static int bhy_read_reg(struct bhy_client_data *client_data,
		u8 reg, u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return client_data->data_bus.read(client_data->data_bus.dev,
		reg, data, len);
}

static int bhy_write_reg(struct bhy_client_data *client_data,
		u8 reg, const u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return client_data->data_bus.write(client_data->data_bus.dev,
		reg, data, len);
}

static int bhy_read_parameter(struct bhy_client_data *client_data,
		u8 page_num, u8 param_num, u8 *data, u8 len)
{
	int ret, ret2;
	int retry;
	u8 ack, u8_val;

	/* Select page */
	ret = bhy_write_reg(client_data, BHY_REG_PARAM_PAGE_SEL, &page_num, 1);
	if (ret < 0) {
		PERR("Write page request failed");
		goto bhy_read_parameter_exit;
	}
	/* Select param */
	ret = bhy_write_reg(client_data, BHY_REG_PARAM_REQ, &param_num, 1);
	if (ret < 0) {
		PERR("Write param request failed");
		goto bhy_read_parameter_exit;
	}
	/* Wait for ack */
	retry = BHY_PARAM_ACK_WAIT_RETRY;
	while (retry--) {
		ret = bhy_read_reg(client_data, BHY_REG_PARAM_ACK, &ack, 1);
		if (ret < 0) {
			PERR("Read ack reg failed");
			goto bhy_read_parameter_exit;
		}
		if (ack == 0x80) {
			PERR("Param is not accepted");
			ret = -EINVAL;
			goto bhy_read_parameter_exit;
		}
		if (ack == param_num)
			break;
		usleep_range(10000, 20000);
	}
	if (retry == -1) {
		PERR("Wait for ack failed[%d, %d]", page_num, param_num);
		ret = -EBUSY;
		goto bhy_read_parameter_exit;
	}
	/* Fetch param data */
	ret = bhy_read_reg(client_data, BHY_REG_SAVED_PARAM_0, data, len);
	if (ret < 0) {
		PERR("Read saved parameter failed");
		goto bhy_read_parameter_exit;
	}
bhy_read_parameter_exit:
	/* Clear up */
	u8_val = 0;
	ret2 = bhy_write_reg(client_data, BHY_REG_PARAM_PAGE_SEL, &u8_val, 1);
	if (ret2 < 0) {
		PERR("Write page sel failed on clear up");
		return ret2;
	}
	u8_val = 0;
	ret2 = bhy_write_reg(client_data, BHY_REG_PARAM_REQ, &u8_val, 1);
	if (ret2 < 0) {
		PERR("Write param_req failed on clear up");
		return ret2;
	}
	retry = BHY_PARAM_ACK_WAIT_RETRY;
	while (retry--) {
		ret2 = bhy_read_reg(client_data, BHY_REG_PARAM_ACK, &ack, 1);
		if (ret2 < 0) {
			PERR("Read ack reg failed");
			return ret2;
		}
		if (ack == 0)
			break;
		udelay(1000);
	}
	if (retry == 0)
		PWARN("BHY_REG_PARAM_ACK cannot revert to 0 after clear up");
	if (ret < 0)
		return ret;
	return len;
}

static int bhy_write_parameter(struct bhy_client_data *client_data,
		u8 page_num, u8 param_num, const u8 *data, u8 len)
{
	int ret, ret2;
	int retry = BHY_PARAM_ACK_WAIT_RETRY;
	u8 param_num_mod, ack, u8_val;

	/* Write param data */
	ret = bhy_write_reg(client_data, BHY_REG_LOAD_PARAM_0, data, len);
	if (ret < 0) {
		PERR("Write load parameter failed");
		goto bhy_write_parameter_exit;
	}
	/* Select page */
	ret = bhy_write_reg(client_data, BHY_REG_PARAM_PAGE_SEL, &page_num, 1);
	if (ret < 0) {
		PERR("Write page request failed");
		goto bhy_write_parameter_exit;
	}
	/* Select param */
	param_num_mod = param_num | 0x80;
	ret = bhy_write_reg(client_data, BHY_REG_PARAM_REQ, &param_num_mod, 1);
	if (ret < 0) {
		PERR("Write param request failed");
		goto bhy_write_parameter_exit;
	}
	/* Wait for ack */
	while (retry--) {
		ret = bhy_read_reg(client_data, BHY_REG_PARAM_ACK, &ack, 1);
		if (ret < 0) {
			PERR("Read ack reg failed");
			goto bhy_write_parameter_exit;
		}
		if (ack == 0x80) {
			PERR("Param is not accepted");
			ret = -EINVAL;
			goto bhy_write_parameter_exit;
		}
		if (ack == param_num_mod)
			break;
		usleep_range(10000, 20000);
	}
	if (retry == -1) {
		PERR("Wait for ack failed[%d, %d]", page_num, param_num);
		ret = -EBUSY;
		goto bhy_write_parameter_exit;
	}
bhy_write_parameter_exit:
	/* Clear up */
	u8_val = 0;
	ret2 = bhy_write_reg(client_data, BHY_REG_PARAM_PAGE_SEL, &u8_val, 1);
	if (ret2 < 0) {
		PERR("Write page sel failed on clear up");
		return ret2;
	}
	u8_val = 0;
	ret2 = bhy_write_reg(client_data, BHY_REG_PARAM_REQ, &u8_val, 1);
	if (ret2 < 0) {
		PERR("Write param_req failed on clear up");
		return ret2;
	}
	retry = BHY_PARAM_ACK_WAIT_RETRY;
	while (retry--) {
		ret2 = bhy_read_reg(client_data, BHY_REG_PARAM_ACK, &ack, 1);
		if (ret2 < 0) {
			PERR("Read ack reg failed");
			return ret2;
		}
		if (ack == 0)
			break;
		udelay(1000);
	}
	if (retry == 0)
		PWARN("BHY_REG_PARAM_ACK cannot revert to 0 after clear up");
	if (ret < 0)
		return ret;
	return len;
}

/* Soft pass thru op, support max length of 4 */
static int bhy_soft_pass_thru_read_reg(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int ret;
	u8 temp[8];
	int retry = BHY_SOFT_PASS_THRU_READ_RETRY;

	if (len > 4 || len <= 0) {
		PERR("Unsupported read len %d", len);
		return -EINVAL;
	}
	temp[0] = slave_addr;
	temp[1] = reg;
	temp[2] = len;
	ret = bhy_write_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
		BHY_PARAM_SOFT_PASS_THRU_READ, temp, 8);
	if (ret < 0) {
		PERR("Write BHY_PARAM_SOFT_PASS_THRU_READ parameter failed");
		return -EIO;
	}
	do {
		udelay(50);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
			BHY_PARAM_SOFT_PASS_THRU_READ, temp, 8);
		if (ret < 0) {
			PERR("Read SOFT_PASS_THRU_READ parameter failed");
			return -EIO;
		}
		if (temp[3])
			break;
	} while (--retry);
	if (retry == 0) {
		PERR("Soft pass thru reg read timed out");
		return -EIO;
	}
	memcpy(data, temp + 4, len);

	return 0;
}

static int bhy_soft_pass_thru_write_reg(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int ret;
	u8 temp[8];
	int retry = BHY_SOFT_PASS_THRU_READ_RETRY;

	if (len > 4 || len <= 0) {
		PERR("Unsupported write len %d", len);
		return -EINVAL;
	}
	temp[0] = slave_addr;
	temp[1] = reg;
	temp[2] = len;
	memcpy(temp + 4, data, len);
	ret = bhy_write_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
		BHY_PARAM_SOFT_PASS_THRU_WRITE, temp, 8);
	if (ret < 0) {
		PERR("Write BHY_PARAM_SOFT_PASS_THRU_WRITE parameter failed");
		return -EIO;
	}
	do {
		udelay(50);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
			BHY_PARAM_SOFT_PASS_THRU_WRITE, temp, 8);
		if (ret < 0) {
			PERR("Read SOFT_PASS_THRU_WRITE parameter failed");
			return -EIO;
		}
		if (temp[3])
			break;
	} while (--retry);
	if (retry == 0) {
		PERR("Soft pass thru reg read timed out");
		return -EIO;
	}

	return 0;
}

static int bhy_soft_pass_thru_read_reg_m(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int i;
	int ret;
	for (i = 0; i < len; ++i) {
		ret = bhy_soft_pass_thru_read_reg(client_data, slave_addr,
			reg + i, &data[i], 1);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int bhy_soft_pass_thru_write_reg_m(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int i;
	int ret;
	for (i = 0; i < len; ++i) {
		ret = bhy_soft_pass_thru_write_reg(client_data, slave_addr,
			reg + i, &data[i], 1);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* Soft pass thru op(non-bust version), support max length of 4 */
#ifdef BHY_RESERVE_FOR_LATER_USE
static int bhy_soft_pass_thru_read_reg_nb(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int ret;
	u8 temp[8];
	int retry = BHY_SOFT_PASS_THRU_READ_RETRY;

	if (len > 4 || len <= 0) {
		PERR("Unsupported read len %d", len);
		return -EINVAL;
	}
	temp[0] = slave_addr;
	temp[1] = reg;
	temp[2] = len;
	ret = bhy_write_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
		BHY_PARAM_SOFT_PASS_THRU_READ_NONBURST, temp, 8);
	if (ret < 0) {
		PERR("Write BHY_PARAM_SOFT_PASS_THRU_READ parameter failed");
		return -EIO;
	}
	do {
		udelay(50);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
			BHY_PARAM_SOFT_PASS_THRU_READ_NONBURST, temp, 8);
		if (ret < 0) {
			PERR("Read SOFT_PASS_THRU_READ parameter failed");
			return -EIO;
		}
		if (temp[3])
			break;
	} while (--retry);
	if (retry == 0) {
		PERR("Soft pass thru reg read timed out");
		return -EIO;
	}
	memcpy(data, temp + 4, len);

	return 0;
}
#endif /*~ BHY_RESERVE_FOR_LATER_USE */

#ifdef BHY_RESERVE_FOR_LATER_USE
/* Still not working for now */
static int bhy_soft_pass_thru_write_reg_nb(struct bhy_client_data *client_data,
	u8 slave_addr, u8 reg, u8 *data, u8 len)
{
	int ret;
	u8 temp[8];
	int retry = BHY_SOFT_PASS_THRU_READ_RETRY;

	if (len > 4 || len <= 0) {
		PERR("Unsupported write len %d", len);
		return -EINVAL;
	}
	temp[0] = slave_addr;
	temp[1] = reg;
	temp[2] = len;
	memcpy(temp + 4, data, len);
	ret = bhy_write_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
		BHY_PARAM_SOFT_PASS_THRU_WRITE_NONBURST, temp, 8);
	if (ret < 0) {
		PERR("Write BHY_PARAM_SOFT_PASS_THRU_WRITE parameter failed");
		return -EIO;
	}
	do {
		udelay(50);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SOFT_PASS_THRU,
			BHY_PARAM_SOFT_PASS_THRU_WRITE_NONBURST, temp, 8);
		if (ret < 0) {
			PERR("Read SOFT_PASS_THRU_WRITE parameter failed");
			return -EIO;
		}
		if (temp[3])
			break;
	} while (--retry);
	if (retry == 0) {
		PERR("Soft pass thru reg read timed out");
		return -EIO;
	}

	return 0;
}
#endif /*~ BHY_RESERVE_FOR_LATER_USE */

static int bmi160_read_reg(struct bhy_client_data *client_data,
	u8 reg, u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return bhy_soft_pass_thru_read_reg(client_data, BHY_SLAVE_ADDR_BMI160,
		reg, data, len);
}

static int bmi160_write_reg(struct bhy_client_data *client_data,
	u8 reg, u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return bhy_soft_pass_thru_write_reg_m(client_data,
		BHY_SLAVE_ADDR_BMI160, reg, data, len);
}

static int bma2x2_read_reg(struct bhy_client_data *client_data,
	u8 reg, u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return bhy_soft_pass_thru_read_reg(client_data, BHY_SLAVE_ADDR_BMA2X2,
		reg, data, len);
}

static int bma2x2_write_reg(struct bhy_client_data *client_data,
	u8 reg, u8 *data, u16 len)
{
	if (client_data == NULL)
		return -EIO;
	return bhy_soft_pass_thru_write_reg_m(client_data,
		BHY_SLAVE_ADDR_BMA2X2, reg, data, len);
}

static void bhy_get_ap_timestamp(s64 *ts_ap)
{
	struct timespec ts;
	get_monotonic_boottime(&ts);
	*ts_ap = ts.tv_sec;
	*ts_ap = *ts_ap * 1000000000 + ts.tv_nsec;
}

static void bhy_clear_flush_queue(struct flush_queue *q)
{
	q->head = q->tail = 0;
	q->cur = -1;
}

static int bhy_hardware_flush(struct bhy_client_data *client_data, u8 sel)
{
	int ret = 0;
	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	if (sel <= 0 || (sel > BHY_SENSOR_HANDLE_MAX
		&& sel != BHY_FLUSH_DISCARD_ALL
		&& sel != BHY_FLUSH_FLUSH_ALL)) {
		PERR("Invalid sensor sel: %d", sel);
		return -EINVAL;
	}
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_reg(client_data, BHY_REG_FIFO_FLUSH, &sel, 1);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write flush sensor reg error");
		return ret;
	}
	return ret;
}

static int bhy_enqueue_flush(struct bhy_client_data *client_data, u8 sensor_sel)
{
	int ret = 0;
	struct flush_queue *q = NULL;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	q = &client_data->flush_queue;
	mutex_lock(&q->lock);
	if (q->cur == -1) {
		ret = bhy_hardware_flush(client_data, sensor_sel);
		if (ret < 0) {
			mutex_unlock(&q->lock);
			PERR("Write sensor flush failed");
			return ret;
		}
		q->cur = sensor_sel;
	} else {
		q->queue[q->head] = sensor_sel;
		q->head = q->head >= BHY_FLUSH_QUEUE_SIZE ? 0 : q->head + 1;
		if (q->head == q->tail) {
			bhy_clear_flush_queue(q);
			mutex_unlock(&q->lock);
			PERR("Flush queue full!!!");
			return -EIO;
		}
	}
	mutex_unlock(&q->lock);

	return ret;
}

static void bhy_dequeue_flush(struct bhy_client_data *client_data,
	u8 sensor_sel)
{
	struct flush_queue *q = NULL;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return;
	}

	q = &client_data->flush_queue;
	mutex_lock(&q->lock);
	if (q->cur == sensor_sel) {
		/* Flush queue empty */
		if (q->head == q->tail) {
			q->cur = -1;
			mutex_unlock(&q->lock);
			return;
		} else {
			q->cur = q->queue[q->tail];
			q->tail = q->tail >= BHY_FLUSH_QUEUE_SIZE ?
				0 : q->tail + 1;
		}
	} else /* Flush logic error */
		bhy_clear_flush_queue(q);
	if (q->cur == -1) {
		mutex_unlock(&q->lock);
		return;
	}
	if (bhy_hardware_flush(client_data, (u8)q->cur) < 0) {
		bhy_clear_flush_queue(q);
		mutex_unlock(&q->lock);
		PERR("Flush next sensor failed");
		return;
	}
	mutex_unlock(&q->lock);
}

static int bhy_check_chip_id(struct bhy_data_bus *data_bus)
{
	int ret;
	u8 prod_id;
	ret = data_bus->read(data_bus->dev, BHY_REG_PRODUCT_ID, &prod_id,
		sizeof(u8));
	if (ret < 0) {
		PERR("Read prod id failed");
		return ret;
	}
	switch (prod_id) {
	case BST_FPGA_PRODUCT_ID_7181:
		printk("BST FPGA 7181 detected");
		break;
	case BHY_C1_PRODUCT_ID:
		printk("BHy C1 sample detected");
		break;
	case BST_FPGA_PRODUCT_ID_7183:
		printk("BST FPGA 7183 detected");
		break;
	default:
		printk("Unknown product ID: 0X%02X", prod_id);
		return -ENODEV;
	}
	return 0;
}

static int bhy_request_firmware(struct bhy_client_data *client_data)
{
	ssize_t ret;
	u8 u8_val;
	__le16 le16_val;
	__le32 le32_val;
	u16 u16_val;
	int retry = BHY_RESET_WAIT_RETRY;
	int reset_flag_copy;
	struct ram_patch_header header;
	struct  ram_patch_cds cds;
	u32 cds_offset;
	ssize_t read_len;
	char data_buf[64]; /* Must be less than burst write max buf */
	u16 remain;
	int i;
	const struct firmware *fw;
	size_t pos;
		printk("########bhy_request_firmware \n");
#ifdef BHY_DEBUG
	bhy_get_ap_timestamp(&g_ts[0]);
#endif /*~ BHY_DEBUG */

	mutex_lock(&client_data->mutex_bus_op);

	/* Reset BHy */
	reset_flag_copy = atomic_read(&client_data->reset_flag);
	if (reset_flag_copy != RESET_FLAG_READY) {
		atomic_set(&client_data->reset_flag, RESET_FLAG_TODO);
		u8_val = 1;
		ret = bhy_write_reg(client_data, BHY_REG_RESET_REQ, &u8_val,
			sizeof(u8));
		printk("########bhy_request_firmware 1 \n");
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write reset reg failed");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return ret;
		}
	printk("########bhy_request_firmware 2 \n");
#ifdef BHY_LEVEL_TRIGGERED_IRQ_SUPPORT
	#if 0
		/* Enable IRQ for reset detection */
		if (client_data->irq_enabled == BHY_FALSE) {
			client_data->irq_enabled = BHY_TRUE;
			enable_irq(client_data->data_bus.irq);
		}
	#else
		mt_eint_unmask(CUST_EINT_GSE_1_NUM);
	#endif
#endif /*~ BHY_LEVEL_TRIGGERED_IRQ_SUPPORT */
		printk("########bhy_request_firmware 3 \n");

		while (retry--) {
			reset_flag_copy = atomic_read(&client_data->reset_flag);
			if (reset_flag_copy == RESET_FLAG_READY)
				break;
			usleep_range(1000,1000);
		}
		printk("########bhy_request_firmware 4 \n");
		if (retry <= 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Reset ready status wait failed");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return -EIO;
		}
		printk("########bhy_request_firmware 5 \n");
		PINFO("BHy reset successfully");
	}
	printk("########bhy_request_firmware 6 \n");
	/* Check chip status */
	retry = 1000;
	while (retry--) {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&u8_val, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return -EIO;
		}
		if (u8_val & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE)
			break;
		udelay(50);
	}
	printk("########bhy_request_firmware 7 \n");
	if (retry <= 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Chip status error after reset");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 8 \n");

#ifdef BHY_DEBUG
	bhy_get_ap_timestamp(&g_ts[1]);
#endif /*~ BHY_DEBUG */
	printk("########bhy_request_firmware 9 \n");
	/* Init upload addr */
	le16_val = __cpu_to_le16(0);
	if (bhy_write_reg(client_data, BHY_REG_UPLOAD_ADDR_0,
		(u8 *)&le16_val, 2) < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Init upload addr failed");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 10 \n");

	/* Write upload request */
	u8_val = BHY_CHIP_CTRL_BIT_UPLOAD_ENABLE;
	if (bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &u8_val, 1) < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Set chip ctrl failed");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 11 \n");

	/* Request firmware data */
	ret = request_firmware(&fw, "ram_patch.fw", client_data->data_bus.dev);
	if (ret < 0) {
		PERR("Request firmware failed");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	pos = 0;
	printk("########bhy_request_firmware 12\n");

	PDEBUG("Firmware size is %u", fw->size);

	/* Upload data */
	if (fw->size < sizeof(header)) {
		release_firmware(fw);
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("firmware size error");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	printk("########bhy_request_firmware 13 \n");
	memcpy(&header, fw->data, sizeof(header));
	pos += sizeof(header);
	u16_val = le16_to_cpu(header.magic);
	if (u16_val != BHY_RAM_PATCH_HEADER_MAGIC) {
		release_firmware(fw);
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Header magic mismatch: %d vs %d", u16_val,
			BHY_RAM_PATCH_HEADER_MAGIC);
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	printk("########bhy_request_firmware 14 \n");
	u16_val = le16_to_cpu(header.flags);
	u16_val &= BHY_FLAG_EXP_ROM_VER_MASK;
	u16_val >>= BHY_FLAG_EXP_ROM_VER_SHIFT;
	if (u16_val != BHY_EXPECTED_ROM_VERSION) {
		release_firmware(fw);
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Expected rom version mismatch: %d vs %d", u16_val,
			BHY_EXPECTED_ROM_VERSION);
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	remain = le16_to_cpu(header.data_length);
	if (remain % 4 != 0) {
		release_firmware(fw);
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("data length cannot be divided by 4");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	printk("########bhy_request_firmware 15 \n");
	if (fw->size < (size_t)(sizeof(header)+remain)) {
		release_firmware(fw);
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("firmware size error: %d vs %d", fw->size,
			sizeof(header) + remain + sizeof(cds));
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	printk("########bhy_request_firmware 16 \n");
le32_val = *(__le32 *)(fw->data + BHY_CDS_OFFSET_POS);
	cds_offset = le32_to_cpu(le32_val);
	/* Evan's proposal to distinguish old firmware */
	cds.sig = 0;
	if (cds_offset >= BHY_CDS_INVALID_OFFSET) {
		PINFO("Old firmware format detected.");
		memcpy(&cds, fw->data + fw->size - sizeof(cds), sizeof(cds));
	} else if (fw->size < (size_t)(sizeof(header) +
		cds_offset + sizeof(cds)))
		PWARN("cds_offset is invalid: %d vs %d", fw->size,
			sizeof(header) + cds_offset + sizeof(cds));
	else
		memcpy(&cds, fw->data + sizeof(header)+cds_offset, sizeof(cds));
	u16_val = le16_to_cpu(cds.sig);
	if (u16_val != BHY_CDS_SIGNATURE)
		PWARN("CDS signature mismatch: %d vs %d", u16_val,
			BHY_CDS_SIGNATURE);
	else
		printk("Ram version read from patch is %d",
			le16_to_cpu(cds.ram_version));
	while (remain > 0) {
		read_len = remain > sizeof(data_buf) ? sizeof(data_buf) :
			remain;
		memcpy(data_buf, fw->data + pos, read_len);
		pos += read_len;
		for (i = 0; i < read_len; i += 4)
			*(u32 *)(data_buf + i) = swab32(*(u32 *)(data_buf + i));
		if (bhy_write_reg(client_data, BHY_REG_UPLOAD_DATA,
			(u8 *)data_buf, read_len) < 0) {
			release_firmware(fw);
			mutex_unlock(&client_data->mutex_bus_op);
			printk("Write ram patch data failed\n");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return -EIO;
		}
		remain -= read_len;
	}
	printk("########bhy_request_firmware 17 \n");

	/* Release firmware */
	release_firmware(fw);
	printk("########bhy_request_firmware 18 \n");

	/* Check CRC */
	if (bhy_read_reg(client_data, BHY_REG_DATA_CRC_0,
		(u8 *)&le32_val, 4) < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read CRC failed");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 19 \n");
	if (le32_val != header.crc) {
		mutex_unlock(&client_data->mutex_bus_op);
		printk("@@@@@@@@CRC mismatch 0X%08X vs 0X%08X@@@@@@@@@@@\n", le32_to_cpu(le32_val),
			le32_to_cpu(header.crc));
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 20 \n");

	/* Clear upload mode bit */
	u8_val = 0;
	if (bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &u8_val, 1) < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip ctrl reg failed");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	udelay(50);
	printk("########bhy_request_firmware 21 \n");

	/* Check chip status */
	retry = 1000;
	while (retry--) {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&u8_val, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return -EIO;
		}
		if (u8_val & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE)
			break;
		udelay(50);
	}
	if (retry <= 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Chip status error after upload patch");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 22 \n");

#ifdef BHY_DEBUG
	bhy_get_ap_timestamp(&g_ts[2]);
#endif /*~ BHY_DEBUG */

	/* Enable cpu run */
	u8_val = BHY_CHIP_CTRL_BIT_CPU_RUN_REQ;
	if (bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &u8_val, 1) < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip ctrl reg failed #2");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 23 \n");

	/* Check chip status */
	retry = 1000;
	while (retry--) {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&u8_val, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
			return -EIO;
		}
		if (!(u8_val & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE))
			break;
		udelay(50);
	}
	if (retry <= 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Chip status error after CPU run request");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EIO;
	}
	printk("########bhy_request_firmware 24 \n");

	mutex_unlock(&client_data->mutex_bus_op);
	PINFO("Ram patch loaded successfully.");

#ifdef BHY_LEVEL_TRIGGERED_IRQ_SUPPORT
	#if 0
	/* Enable IRQ to read data after ram patch loaded */
	if (client_data->irq_enabled == BHY_FALSE) {
		client_data->irq_enabled = BHY_TRUE;
		enable_irq(client_data->data_bus.irq);
	}
	#else
	mt_eint_unmask(CUST_EINT_GSE_1_NUM);
	#endif
#endif /*~ BHY_LEVEL_TRIGGERED_IRQ_SUPPORT */
	printk("########bhy_request_firmware 25 \n");

	return 0;
}

static int bhy_reinit(struct bhy_client_data *client_data)
{
	int retry;
	u8 reg_data;
	int ret;

	PDEBUG("Reinit after self-test");
	mutex_lock(&client_data->mutex_bus_op);

	reg_data = 0;
	ret = bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip control reg failed");
		return -EIO;
	}
	retry = 1000;
	do {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			return -EIO;
		}
		if (reg_data & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE)
			break;
		usleep_range(10000, 10000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Wait for chip idle status timed out");
		return -EBUSY;
	}
	/* Clear self test bit */
	ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read host ctrl reg failed");
		return -EIO;
	}
	reg_data &= ~HOST_CTRL_MASK_SELF_TEST_REQ;
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host ctrl reg failed");
		return -EIO;
	}
	/* Enable CPU run from chip control */
	reg_data = 1;
	ret = bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip control reg failed");
		return -EIO;
	}
	retry = 1000;
	do {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			return -EIO;
		}
		if (!(reg_data & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE))
			break;
		usleep_range(10000, 10000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Wait for chip running status timed out");
		return -EBUSY;
	}

	mutex_unlock(&client_data->mutex_bus_op);

	return 0;
}

static int bhy_get_sensor_conf(struct bhy_client_data *client_data,
	int handle, u8 *conf)
{
	int i;
	__le16 swap_data;
	u8 data[8];
	int ret;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SENSOR,
		BHY_PARAM_SENSOR_CONF_0 + client_data->sensor_sel,
		data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read parameter error");
		return ret;
	}

	for (i = 0; i < 4; ++i) {
		swap_data = cpu_to_le16(*(u16 *)(data + i * 2));
		memcpy(conf + i * 2, &swap_data, sizeof(swap_data));
	}

	return 8;
}

static int bhy_set_sensor_conf(struct bhy_client_data *client_data,
	int handle, const u8 *conf)
{
	int i;
	__le16 swap_data;
	u8 data[8];
	int ret;

	for (i = 0; i < 4; ++i) {
		swap_data = cpu_to_le16(*(u16 *)(conf + i * 2));
		memcpy(data + i * 2, &swap_data, sizeof(swap_data));
	}
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_SENSOR,
		BHY_PARAM_SENSOR_CONF_0 + handle,
		(u8 *)data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write parameter error");
		return ret;
	}

	return 0;
}

static int bhy_set_mapping_matrix(struct bhy_client_data *client_data,
	int index)
{
	u8 data[8] = { 0, };
	int i;
	struct physical_sensor_context *pct;
	int handle;
	int ret;

	switch (index) {
	case PHYSICAL_SENSOR_INDEX_ACC:
		handle = BHY_SENSOR_HANDLE_ACCELEROMETER;
		break;
	case PHYSICAL_SENSOR_INDEX_MAG:
		handle = BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED;
		break;
	case PHYSICAL_SENSOR_INDEX_GYRO:
		handle = BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED;
		break;
	default:
		return -EINVAL;
	}

	pct = &client_data->ps_context[index];
	for (i = 0; i < 5; ++i) {
		switch (pct->mapping_matrix[2 * i]) {
		case 0:
			data[i] = 0;
			break;
		case 1:
			data[i] = 1;
			break;
		case -1:
			data[i] = 0xF;
			break;
		default:
			return -EINVAL;
		}
		if (i == 4)
			break;
		switch (pct->mapping_matrix[2 * i + 1]) {
		case 0:
			break;
		case 1:
			data[i] |= 0x10;
			break;
		case -1:
			data[i] |= 0xF0;
			break;
		default:
			return -EINVAL;
		}
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_SYSTEM,
		BHY_PARAM_SYSTEM_PHYSICAL_SENSOR_DETAIL_0 + handle,
		data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write mapping matrix failed");
		return ret;
	}

	return 0;
}

static int bhy_set_meta_event_ctrl(struct bhy_client_data *client_data,
	int type, int for_wake_up, int event_en, int irq_en)
{
	int num, bit;
	u8 param;
	u8 data[8];
	int ret;

	/*get the num for META_EVENT_CONTORL_BYTE*/
	num = (type - 1) / 4;
	/*get the bit for META_EVENT_CONTROL_BYTE*/
	bit = (type - 1) % 4;
	param = for_wake_up == BHY_TRUE ?
		BHY_PARAM_SYSTEM_WAKE_UP_META_EVENT_CTRL :
		BHY_PARAM_SYSTEM_META_EVENT_CTRL;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM, param,
		data, 8);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read meta event failed");
		return -EIO;
	}

	if (event_en == BHY_TRUE)
		data[num] |= (1 << (bit * 2 + 1));
	else
		data[num] &= ~(1 << (bit * 2 + 1));
	if (irq_en == BHY_TRUE)
		data[num] |= (1 << (bit * 2));
	else
		data[num] &= ~(1 << (bit * 2));
	ret = bhy_write_parameter(client_data, BHY_PAGE_SYSTEM, param,
		data, 8);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write meta event ctrl failed");
		return -EIO;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return 0;
}

static int bhy_set_fifo_ctrl(struct bhy_client_data *client_data, u8 *buf)
{
	int ret;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_SYSTEM,
		BHY_PARAM_SYSTEM_FIFO_CTRL, buf, BHY_FIFO_CTRL_PARAM_LEN);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write fifo ctrl failed");
		return -EIO;
	}

	return ret;
}

static int bhy_set_calib_profile(struct bhy_client_data *client_data,
	const u8 *buf)
{
	int ret;
	u8 param_num;

	mutex_lock(&client_data->mutex_bus_op);
#ifdef BHY_CALIB_PROFILE_OP_IN_FUSER_CORE
	switch (client_data->sensor_sel) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		param_num = BHY_PARAM_OFFSET_ACC_2;
		break;
	case BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD:
		param_num = BHY_PARAM_OFFSET_MAG_2;
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE:
		param_num = BHY_PARAM_OFFSET_GYRO_2;
		break;
	default:
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Invalid sensor sel");
		return -EINVAL;
	}
#else
	switch (client_data->sensor_sel) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		param_num = BHY_PARAM_OFFSET_ACC;
		break;
	case BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD:
		param_num = BHY_PARAM_OFFSET_MAG;
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE:
		param_num = BHY_PARAM_OFFSET_GYRO;
		break;
	default:
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Invalid sensor sel");
		return -EINVAL;
	}
#endif /*~ BHY_CALIB_PROFILE_OP_IN_FUSER_CORE */
	ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
		param_num, (u8 *)buf, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write parameter error");
		return ret;
	}

	return 0;
}

/* Returns whether hw watchdog was detected */
static int check_watchdog_reset(struct bhy_client_data *client_data)
{
	int ret;
	u8 reg_data;
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_HOST_STATUS,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read host status failed");
		return BHY_FALSE;
	}
	if (!(reg_data & BHY_HOST_STATUS_MASK_RESET)) {
		mutex_unlock(&client_data->mutex_bus_op);
		PDEBUG("Host status is still good");
		return BHY_FALSE;
	}
	ret = bhy_read_reg(client_data, BHY_REG_CHIP_CTRL,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read chip control failed");
		return BHY_FALSE;
	}
	if (reg_data & BHY_CHIP_CTRL_BIT_CPU_RUN_REQ) {
		mutex_unlock(&client_data->mutex_bus_op);
		PDEBUG("Chip control indicates CPU still running");
		return BHY_FALSE;
	}
	mutex_unlock(&client_data->mutex_bus_op);
	PDEBUG("Hardware watch dog bark!!!");
	atomic_set(&client_data->reset_flag, RESET_FLAG_READY);
	bhy_request_firmware(client_data);
	client_data->recover_from_disaster = BHY_TRUE;

	return BHY_TRUE;
}

/* Check if irq status/bytes remaining register update inconsistency happened.
   returns BHY_TRUE if this inconsistency happened and abort stucked data */
static int check_mcu_inconsistency(struct bhy_client_data *client_data)
{
	int ret;
	u8 reg_data;
	mutex_lock(&client_data->mutex_bus_op);
	/* Check IRQ status */
	ret = bhy_read_reg(client_data, BHY_REG_INT_STATUS, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read irq status failed");
		return BHY_FALSE;
	}
	if (reg_data == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		return BHY_FALSE;
	}
	reg_data = HOST_CTRL_MASK_ABORT_TRANSFER;
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host control failed");
		return BHY_FALSE;
	}
	udelay(1000);
	reg_data = 0; /* Clear host control reg */
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host control failed");
		return BHY_FALSE;
	}
	mutex_unlock(&client_data->mutex_bus_op);
	return BHY_TRUE;
}

static void bhy_init_sensor_context(struct bhy_client_data *client_data)
{
	int i;
	struct bhy_sensor_context *ct;

	ct = client_data->sensor_context;
	for (i = 1; i <= BHY_SENSOR_HANDLE_REAL_MAX; ++i) {
		ct[i].handlle = i;
		ct[i].data_len = -1;
		ct[i].type = SENSOR_TYPE_INVALID;
		ct[i].is_wakeup = BHY_FALSE;
		ct[i].sample_rate = 0;
		ct[i].report_latency = 0;
#ifdef BHY_AR_HAL_SUPPORT
		ct[i].for_ar_hal = BHY_FALSE;
#endif /*~ BHY_AR_HAL_SUPPORT */
	}
	for (i = BHY_SENSOR_HANDLE_WAKEUP_BEGIN; i <= BHY_SENSOR_HANDLE_MAX;
		++i)
		ct[i].is_wakeup = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_ACCELEROMETER].data_len =
		BHY_SENSOR_DATA_LEN_ACCELEROMETER;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD].data_len =
		BHY_SENSOR_DATA_LEN_GEOMAGNETIC_FIELD;
	ct[BHY_SENSOR_HANDLE_ORIENTATION].data_len =
		BHY_SENSOR_DATA_LEN_ORIENTATION;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE].data_len =
		BHY_SENSOR_DATA_LEN_GYROSCOPE;
	ct[BHY_SENSOR_HANDLE_LIGHT].data_len =
		BHY_SENSOR_DATA_LEN_LIGHT;
	ct[BHY_SENSOR_HANDLE_PRESSURE].data_len =
		BHY_SENSOR_DATA_LEN_PRESSURE;
	ct[BHY_SENSOR_HANDLE_TEMPERATURE].data_len =
		BHY_SENSOR_DATA_LEN_TEMPERATURE;
	ct[BHY_SENSOR_HANDLE_PROXIMITY].data_len =
		BHY_SENSOR_DATA_LEN_PROXIMITY;
	ct[BHY_SENSOR_HANDLE_GRAVITY].data_len =
		BHY_SENSOR_DATA_LEN_GRAVITY;
	ct[BHY_SENSOR_HANDLE_LINEAR_ACCELERATION].data_len =
		BHY_SENSOR_DATA_LEN_LINEAR_ACCELERATION;
	ct[BHY_SENSOR_HANDLE_ROTATION_VECTOR].data_len =
		BHY_SENSOR_DATA_LEN_ROTATION_VECTOR;
	ct[BHY_SENSOR_HANDLE_RELATIVE_HUMIDITY].data_len =
		BHY_SENSOR_DATA_LEN_RELATIVE_HUMIDITY;
	ct[BHY_SENSOR_HANDLE_AMBIENT_TEMPERATURE].data_len =
		BHY_SENSOR_DATA_LEN_AMBIENT_TEMPERATURE;
	ct[BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED].data_len =
		BHY_SENSOR_DATA_LEN_MAGNETIC_FIELD_UNCALIBRATED;
	ct[BHY_SENSOR_HANDLE_GAME_ROTATION_VECTOR].data_len =
		BHY_SENSOR_DATA_LEN_GAME_ROTATION_VECTOR;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED].data_len =
		BHY_SENSOR_DATA_LEN_GYROSCOPE_UNCALIBRATED;
	ct[BHY_SENSOR_HANDLE_SIGNIFICANT_MOTION].data_len =
		BHY_SENSOR_DATA_LEN_SIGNIFICANT_MOTION;
	ct[BHY_SENSOR_HANDLE_STEP_DETECTOR].data_len =
		BHY_SENSOR_DATA_LEN_STEP_DETECTOR;
	ct[BHY_SENSOR_HANDLE_STEP_COUNTER].data_len =
		BHY_SENSOR_DATA_LEN_STEP_COUNTER;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_ROTATION_VECTOR].data_len =
		BHY_SENSOR_DATA_LEN_GEOMAGNETIC_ROTATION_VECTOR;
	ct[BHY_SENSOR_HANDLE_HEART_RATE].data_len =
		BHY_SENSOR_DATA_LEN_HEART_RATE;
	ct[BHY_SENSOR_HANDLE_ACCELEROMETER_WU].data_len =
		BHY_SENSOR_DATA_LEN_ACCELEROMETER_WU;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD_WU].data_len =
		BHY_SENSOR_DATA_LEN_GEOMAGNETIC_FIELD_WU;
	ct[BHY_SENSOR_HANDLE_ORIENTATION_WU].data_len =
		BHY_SENSOR_DATA_LEN_ORIENTATION_WU;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_WU].data_len =
		BHY_SENSOR_DATA_LEN_GYROSCOPE_WU;
	ct[BHY_SENSOR_HANDLE_LIGHT_WU].data_len =
		BHY_SENSOR_DATA_LEN_LIGHT_WU;
	ct[BHY_SENSOR_HANDLE_PRESSURE_WU].data_len =
		BHY_SENSOR_DATA_LEN_PRESSURE_WU;
	ct[BHY_SENSOR_HANDLE_TEMPERATURE_WU].data_len =
		BHY_SENSOR_DATA_LEN_TEMPERATURE_WU;
	ct[BHY_SENSOR_HANDLE_PROXIMITY_WU].data_len =
		BHY_SENSOR_DATA_LEN_PROXIMITY_WU;
	ct[BHY_SENSOR_HANDLE_GRAVITY_WU].data_len =
		BHY_SENSOR_DATA_LEN_GRAVITY_WU;
	ct[BHY_SENSOR_HANDLE_LINEAR_ACCELERATION_WU].data_len =
		BHY_SENSOR_DATA_LEN_LINEAR_ACCELERATION_WU;
	ct[BHY_SENSOR_HANDLE_ROTATION_VECTOR_WU].data_len =
		BHY_SENSOR_DATA_LEN_ROTATION_VECTOR_WU;
	ct[BHY_SENSOR_HANDLE_RELATIVE_HUMIDITY_WU].data_len =
		BHY_SENSOR_DATA_LEN_RELATIVE_HUMIDITY_WU;
	ct[BHY_SENSOR_HANDLE_AMBIENT_TEMPERATURE_WU].data_len =
		BHY_SENSOR_DATA_LEN_AMBIENT_TEMPERATURE_WU;
	ct[BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED_WU].data_len =
		BHY_SENSOR_DATA_LEN_MAGNETIC_FIELD_UNCALIBRATED_WU;
	ct[BHY_SENSOR_HANDLE_GAME_ROTATION_VECTOR_WU].data_len =
		BHY_SENSOR_DATA_LEN_GAME_ROTATION_VECTOR_WU;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED_WU].data_len =
		BHY_SENSOR_DATA_LEN_GYROSCOPE_UNCALIBRATED_WU;
	ct[BHY_SENSOR_HANDLE_STEP_DETECTOR_WU].data_len =
		BHY_SENSOR_DATA_LEN_STEP_DETECTOR_WU;
	ct[BHY_SENSOR_HANDLE_STEP_COUNTER_WU].data_len =
		BHY_SENSOR_DATA_LEN_STEP_COUNTER_WU;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_ROTATION_VECTOR_WU].data_len =
		BHY_SENSOR_DATA_LEN_GEOMAGNETIC_ROTATION_VECTOR_WU;
	ct[BHY_SENSOR_HANDLE_HEART_RATE_WU].data_len =
		BHY_SENSOR_DATA_LEN_HEART_RATE_WU;
	ct[BHY_SENSOR_HANDLE_TILT_DETECTOR].data_len =
		BHY_SENSOR_DATA_LEN_TILT_DETECTOR;
	ct[BHY_SENSOR_HANDLE_WAKE_GESTURE].data_len =
		BHY_SENSOR_DATA_LEN_WAKE_GESTURE;
	ct[BHY_SENSOR_HANDLE_GLANCE_GESTURE].data_len =
		BHY_SENSOR_DATA_LEN_GLANCE_GESTURE;
	ct[BHY_SENSOR_HANDLE_PICK_UP_GESTURE].data_len =
		BHY_SENSOR_DATA_LEN_PICK_UP_GESTURE;
	ct[BHY_SENSOR_HANDLE_ACTIVITY_RECOGNITION].data_len =
		BHY_SENSOR_DATA_LEN_ACTIVITY_RECOGNITION;
	ct[BHY_SENSOR_HANDLE_BSX_C].data_len =
		BHY_SENSOR_DATA_LEN_BSX_C;
	ct[BHY_SENSOR_HANDLE_BSX_B].data_len =
		BHY_SENSOR_DATA_LEN_BSX_B;
	ct[BHY_SENSOR_HANDLE_BSX_A].data_len =
		BHY_SENSOR_DATA_LEN_BSX_A;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_LSW].data_len =
		BHY_SENSOR_DATA_LEN_TIMESTAMP_LSW;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_MSW].data_len =
		BHY_SENSOR_DATA_LEN_TIMESTAMP_MSW;
	ct[BHY_SENSOR_HANDLE_META_EVENT].data_len =
		BHY_SENSOR_DATA_LEN_META_EVENT;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_LSW_WU].data_len =
		BHY_SENSOR_DATA_LEN_TIMESTAMP_LSW_WU;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_MSW_WU].data_len =
		BHY_SENSOR_DATA_LEN_TIMESTAMP_MSW_WU;
	ct[BHY_SENSOR_HANDLE_META_EVENT_WU].data_len =
		BHY_SENSOR_DATA_LEN_META_EVENT_WU;
	ct[BHY_SENSOR_HANDLE_DEBUG].data_len =
		BHY_SENSOR_DATA_LEN_DEBUG;
	ct[BHY_SENSOR_HANDLE_CUSTOM_1].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_1;
	ct[BHY_SENSOR_HANDLE_CUSTOM_2].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_2;
	ct[BHY_SENSOR_HANDLE_CUSTOM_3].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_3;
	ct[BHY_SENSOR_HANDLE_CUSTOM_4].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_4;
	ct[BHY_SENSOR_HANDLE_CUSTOM_5].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_5;
	ct[BHY_SENSOR_HANDLE_CUSTOM_1_WU].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_1_WU;
	ct[BHY_SENSOR_HANDLE_CUSTOM_2_WU].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_2_WU;
	ct[BHY_SENSOR_HANDLE_CUSTOM_3_WU].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_3_WU;
	ct[BHY_SENSOR_HANDLE_CUSTOM_4_WU].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_4_WU;
	ct[BHY_SENSOR_HANDLE_CUSTOM_5_WU].data_len =
		BHY_SENSOR_DATA_LEN_CUSTOM_5_WU;

	ct[BHY_SENSOR_HANDLE_ACCELEROMETER].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_ORIENTATION].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_LIGHT].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_PRESSURE].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_TEMPERATURE].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_PROXIMITY].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_GRAVITY].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_LINEAR_ACCELERATION].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_ROTATION_VECTOR].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_RELATIVE_HUMIDITY].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_AMBIENT_TEMPERATURE].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GAME_ROTATION_VECTOR].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_SIGNIFICANT_MOTION].type =
		SENSOR_TYPE_ONE_SHOT;
	ct[BHY_SENSOR_HANDLE_STEP_DETECTOR].type =
		SENSOR_TYPE_SPECIAL;
	ct[BHY_SENSOR_HANDLE_STEP_COUNTER].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_ROTATION_VECTOR].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_HEART_RATE].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_ACCELEROMETER_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_ORIENTATION_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_LIGHT_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_PRESSURE_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_TEMPERATURE_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_PROXIMITY_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_GRAVITY_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_LINEAR_ACCELERATION_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_ROTATION_VECTOR_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_RELATIVE_HUMIDITY_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_AMBIENT_TEMPERATURE_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GAME_ROTATION_VECTOR_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_STEP_DETECTOR_WU].type =
		SENSOR_TYPE_SPECIAL;
	ct[BHY_SENSOR_HANDLE_STEP_COUNTER_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_GEOMAGNETIC_ROTATION_VECTOR_WU].type =
		SENSOR_TYPE_CONTINUOUS;
	ct[BHY_SENSOR_HANDLE_HEART_RATE_WU].type =
		SENSOR_TYPE_ON_CHANGE;
	ct[BHY_SENSOR_HANDLE_TILT_DETECTOR].type =
		SENSOR_TYPE_SPECIAL;
	ct[BHY_SENSOR_HANDLE_WAKE_GESTURE].type =
		SENSOR_TYPE_ONE_SHOT;
	ct[BHY_SENSOR_HANDLE_GLANCE_GESTURE].type =
		SENSOR_TYPE_ONE_SHOT;
	ct[BHY_SENSOR_HANDLE_PICK_UP_GESTURE].type =
		SENSOR_TYPE_ONE_SHOT;
	ct[BHY_SENSOR_HANDLE_ACTIVITY_RECOGNITION].type =
		SENSOR_TYPE_ON_CHANGE;
#ifdef BHY_AR_HAL_SUPPORT
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_LSW].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_MSW].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_META_EVENT].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_LSW_WU].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_TIMESTAMP_MSW_WU].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_META_EVENT_WU].for_ar_hal = BHY_TRUE;
	ct[BHY_SENSOR_HANDLE_ACTIVITY_RECOGNITION].for_ar_hal = BHY_TRUE;
#endif /*~ BHY_AR_HAL_SUPPORT */
}

#ifdef BHY_DEBUG
static void bhy_dump_fifo_data(const u8 *data, int len)
{
	int i, j;
	char buf[256];
	int line_char = 0;
	const int bytes_per_line = 8;
	PDEBUG("Data is");
	for (i = j = 0; i < len; ++i) {
		j += snprintf(buf + j, 16, "%02X ", *(data + i));
		if (++line_char == bytes_per_line) {
			buf[j - 1] = '\0';
			PDEBUG("%s", buf);
			line_char = 0;
			j = 0;
		}
	}
	if (line_char > 0) {
		buf[j - 1] = '\0';
		PDEBUG("%s", buf);
	}
}
#endif /*~ BHY_DEBUG */

static void bhy_recover_sensor_activity(struct bhy_client_data *client_data)
{
	int i;
	struct bhy_sensor_context *ct;
	u8 conf[8];
	struct bhy_meta_event_context *mct, *wmct;

	mutex_lock(&client_data->mutex_sw_watchdog);
	/* Recover fifo control settings */
	bhy_set_fifo_ctrl(client_data, client_data->fifo_ctrl_cfg);
	/* Recover step counter */
	client_data->step_count_base = client_data->step_count_latest;
	/* Recover axis mapping */
	for (i = 0; i < PHYSICAL_SENSOR_COUNT; ++i) {
		if (client_data->ps_context[i].use_mapping_matrix == BHY_TRUE)
			bhy_set_mapping_matrix(client_data, i);
	}
	/* Recover meta event */
	mct = client_data->me_context;
	wmct = client_data->mew_context;
	for (i = 1; i <= BHY_META_EVENT_MAX; ++i) {
		if (mct[i].event_en != BHY_STATUS_DEFAULT)
			bhy_set_meta_event_ctrl(client_data, i, BHY_FALSE,
			mct[i].event_en == BHY_STATUS_ENABLED ?
		BHY_TRUE : BHY_FALSE,
				   mct[i].irq_en == BHY_STATUS_ENABLED ?
			   BHY_TRUE : BHY_FALSE);
		if (wmct[i].event_en != BHY_STATUS_DEFAULT)
			bhy_set_meta_event_ctrl(client_data, i, BHY_TRUE,
			wmct[i].event_en == BHY_STATUS_ENABLED ?
		BHY_TRUE : BHY_FALSE,
				   wmct[i].irq_en == BHY_STATUS_ENABLED ?
			   BHY_TRUE : BHY_FALSE);
	}
	/* Recover calibration profile */
	mutex_lock(&client_data->mutex_bus_op);
	client_data->sensor_sel = BHY_SENSOR_HANDLE_ACCELEROMETER;
	mutex_unlock(&client_data->mutex_bus_op);
	bhy_set_calib_profile(client_data, client_data->calibprofile_acc);
	mutex_lock(&client_data->mutex_bus_op);
	client_data->sensor_sel = BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD;
	mutex_unlock(&client_data->mutex_bus_op);
	bhy_set_calib_profile(client_data, client_data->calibprofile_mag);
	mutex_lock(&client_data->mutex_bus_op);
	client_data->sensor_sel = BHY_SENSOR_HANDLE_GYROSCOPE;
	mutex_unlock(&client_data->mutex_bus_op);
	bhy_set_calib_profile(client_data, client_data->calibprofile_gyro);
	/* Recover sensor activity */
	ct = client_data->sensor_context;
	for (i = 1; i <= BHY_SENSOR_HANDLE_MAX; ++i) {
		if (ct[i].type != SENSOR_TYPE_INVALID &&
			ct[i].sample_rate > 0) {
			memcpy(conf, &ct[i].sample_rate, 2);
			memcpy(conf + 2, &ct[i].report_latency, 2);
			memset(conf + 4, 0, 4);
			bhy_set_sensor_conf(client_data, i, conf);
		}
	}
	mutex_unlock(&client_data->mutex_sw_watchdog);
}

static void bhy_read_fifo_data(struct bhy_client_data *client_data,
	int reset_flag)
{
	int ret;
	u8 *data = client_data->fifo_buf;
	u8 *pbuff;
	u16 offset;
	int tmp_len;
	u8 event = 0;
	u8 sensor_sel = 0;
	u16 bytes_remain;
	int sensor_type;
	int parse_index, data_len;
	struct frame_queue *q = &client_data->data_queue;
	int idx; /* For self test index */
	int init_event_detected = BHY_FALSE;
	int self_result_detected = BHY_FALSE;
	int sigmo_detected = BHY_FALSE;
	int wake_gesture_detected = BHY_FALSE;
	int glance_gesture_detected = BHY_FALSE;
	int pick_up_gesture_detected = BHY_FALSE;
	struct bhy_sensor_context *ct;
	__le16 le16_val;
#ifdef BHY_AR_HAL_SUPPORT
	struct frame_queue *qa = &client_data->data_queue_ar;
#endif /*~ BHY_AR_HAL_SUPPORT */

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_BYTES_REMAIN_0,
		(u8 *)&bytes_remain, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read bytes remain reg failed");
		return;
	}
//	printk("bytes_remain length: %d\n", bytes_remain);
#ifdef BHY_DEBUG
	if (client_data->enable_irq_log)
		PDEBUG("Fifo length: %d", bytes_remain);
#endif /*~ BHY_DEBUG */
	if (bytes_remain == 0) {
		PDEBUG("Zero length FIFO detected");
		if (check_watchdog_reset(client_data) == BHY_TRUE)
			return;
		if (check_mcu_inconsistency(client_data) == BHY_TRUE)
			PERR("MCU inconsistency happened.");
		return;
	}
//	printk("############6-3###########\n");

	/* Feed the software watchdog */
	mutex_lock(&client_data->mutex_sw_watchdog);
	client_data->inactive_count = 0;
	mutex_unlock(&client_data->mutex_sw_watchdog);
//	printk("############6-4###########\n");

	mutex_lock(&client_data->mutex_bus_op);
	//when data > 250
	pbuff = data;
	tmp_len = (int)bytes_remain;
	offset = 0;
//	printk("tmp_len=%d\n",tmp_len);
	do{
		if(tmp_len >= 250)
		{
			ret = bhy_read_reg(client_data, BHY_REG_FIFO_BUFFER_0,
					pbuff, 250);

			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
//				printk("#######Read fifo data failed 1 ######\n");
				return;
			}else
			{
//				printk("ret1=%d\n",ret);
			}			
		offset = offset + 250;
		tmp_len = tmp_len - 250;
		pbuff =   data + offset;
		}else
		{	
			ret = bhy_read_reg(client_data, BHY_REG_FIFO_BUFFER_0,
					pbuff, tmp_len);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
//				printk("#######Read fifo data failed 2 ######\n");
				return;
			}else
			{
//				printk("ret2=%d\n",ret);
			}
			offset = offset + ret;
			tmp_len = tmp_len - ret;
			pbuff =   data + offset;
			
		}
	}while(tmp_len > 0);

//	printk("############6-5###########\n");
	mutex_unlock(&client_data->mutex_bus_op);
#ifdef BHY_DEBUG
	if (client_data->enable_fifo_log)
		bhy_dump_fifo_data(data, bytes_remain);
#endif /*~ BHY_DEBUG */
//	printk("############6-6###########\n");

	mutex_lock(&q->lock);
#ifdef BHY_AR_HAL_SUPPORT
	mutex_lock(&qa->lock);
#endif /*~ BHY_AR_HAL_SUPPORT */
	for (parse_index = 0; parse_index < bytes_remain;
		parse_index += data_len + 1) {
		sensor_type = data[parse_index];
		/* FIFO parsing should end with a 0 sensor type */
		if (sensor_type == 0)
			break;
		data_len = client_data->sensor_context[sensor_type].data_len;
		if (data_len < 0)
			break;
		if (parse_index + data_len >= bytes_remain) {
			PERR("Invalid FIFO data detected for sensor_type %d",
				sensor_type);
			break;
		}
		if (reset_flag == RESET_FLAG_READY) {
			if (sensor_type == BHY_SENSOR_HANDLE_META_EVENT &&
				data[parse_index + 1] ==
				META_EVENT_INITIALIZED) {
				atomic_set(&client_data->reset_flag,
					RESET_FLAG_INITIALIZED);
				init_event_detected = BHY_TRUE;
#ifdef BHY_DEBUG
				bhy_get_ap_timestamp(&g_ts[3]);
				PDEBUG("ts-0: %lld", g_ts[0]);
				PDEBUG("ts-1: %lld", g_ts[1]);
				PDEBUG("ts-2: %lld", g_ts[2]);
				PDEBUG("ts-3: %lld", g_ts[3]);
#endif /*~ BHY_DEBUG */
			}
		} else if (reset_flag == RESET_FLAG_SELF_TEST) {
			if (sensor_type == BHY_SENSOR_HANDLE_META_EVENT &&
				data[parse_index + 1] ==
				META_EVENT_SELF_TEST_RESULTS) {
				idx = -1;
				switch (data[parse_index + 2]) {
				case BHY_SENSOR_HANDLE_ACCELEROMETER:
					idx = PHYSICAL_SENSOR_INDEX_ACC;
					break;
				case BHY_SENSOR_HANDLE_MAG_UNCAL:
					idx = PHYSICAL_SENSOR_INDEX_MAG;
					break;
				case BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED:
					idx = PHYSICAL_SENSOR_INDEX_GYRO;
					break;
				}
				if (idx != -1)
					client_data->self_test_result[idx] =
					(s8)data[parse_index + 3];
				self_result_detected = BHY_TRUE;
			}
		}
		switch (sensor_type) {
		case BHY_SENSOR_HANDLE_SIGNIFICANT_MOTION:
			sigmo_detected = BHY_TRUE;
			break;
		case BHY_SENSOR_HANDLE_WAKE_GESTURE:
			wake_gesture_detected = BHY_TRUE;
			break;
		case BHY_SENSOR_HANDLE_GLANCE_GESTURE:
			glance_gesture_detected = BHY_TRUE;
			break;
		case BHY_SENSOR_HANDLE_PICK_UP_GESTURE:
			pick_up_gesture_detected = BHY_TRUE;
			break;
		case BHY_SENSOR_HANDLE_META_EVENT:
		case BHY_SENSOR_HANDLE_META_EVENT_WU:
			event = data[parse_index + 1];
			sensor_sel = data[parse_index + 2];
			if (event == META_EVENT_FLUSH_COMPLETE)
				bhy_dequeue_flush(client_data, sensor_sel);
			break;
		case BHY_SENSOR_HANDLE_STEP_COUNTER:
		case BHY_SENSOR_HANDLE_STEP_COUNTER_WU:
			client_data->step_count_latest =
				client_data->step_count_base +
				le16_to_cpu(*(__le16 *)&data[parse_index + 1]);
			le16_val = cpu_to_le16(client_data->step_count_latest);
			memcpy(&data[parse_index + 1], &le16_val, 2);
			break;
		}
		q->frames[q->head].handle = sensor_type;
		memcpy(q->frames[q->head].data, &data[parse_index + 1],
			data_len);
		if (q->head == BHY_FRAME_SIZE - 1)
			q->head = 0;
		else
			++q->head;
		if (q->head == q->tail) {
			PDEBUG("One frame data lost!!!");
			if (q->tail == BHY_FRAME_SIZE - 1)
				q->tail = 0;
			else
				++q->tail;
		}
#ifdef BHY_AR_HAL_SUPPORT
		if (client_data->sensor_context[sensor_type].for_ar_hal ==
			BHY_TRUE) {
			qa->frames[qa->head].handle = sensor_type;
			memcpy(qa->frames[qa->head].data,
				&client_data->fifo_buf[parse_index + 1],
				data_len);
			if (qa->head == BHY_FRAME_SIZE_AR - 1)
				qa->head = 0;
			else
				++qa->head;
			if (qa->head == qa->tail) {
				if (qa->tail == BHY_FRAME_SIZE_AR - 1)
					qa->tail = 0;
				else
					++qa->tail;
			}
		}
#endif /*~ BHY_AR_HAL_SUPPORT */
	}
#ifdef BHY_AR_HAL_SUPPORT
	mutex_unlock(&qa->lock);
#endif /*~ BHY_AR_HAL_SUPPORT */
	mutex_unlock(&q->lock);

	/* Fix status for one shot sensor */
	ct = client_data->sensor_context;
	if (sigmo_detected == BHY_TRUE) {
		mutex_lock(&client_data->mutex_sw_watchdog);
		ct[BHY_SENSOR_HANDLE_SIGNIFICANT_MOTION].sample_rate = 0;
		mutex_unlock(&client_data->mutex_sw_watchdog);
	}
	if (wake_gesture_detected == BHY_TRUE) {
		mutex_lock(&client_data->mutex_sw_watchdog);
		ct[BHY_SENSOR_HANDLE_WAKE_GESTURE].sample_rate = 0;
		mutex_unlock(&client_data->mutex_sw_watchdog);
	}
	if (glance_gesture_detected == BHY_TRUE) {
		mutex_lock(&client_data->mutex_sw_watchdog);
		ct[BHY_SENSOR_HANDLE_GLANCE_GESTURE].sample_rate = 0;
		mutex_unlock(&client_data->mutex_sw_watchdog);
	}
	if (pick_up_gesture_detected == BHY_TRUE) {
		mutex_lock(&client_data->mutex_sw_watchdog);
		ct[BHY_SENSOR_HANDLE_PICK_UP_GESTURE].sample_rate = 0;
		mutex_unlock(&client_data->mutex_sw_watchdog);
	}

	/* Recovery on disaster */
	if (init_event_detected == BHY_TRUE &&
		client_data->recover_from_disaster == BHY_TRUE) {
		bhy_recover_sensor_activity(client_data);
		client_data->recover_from_disaster = BHY_FALSE;
	}

	/* Re-init sensors */
	if (self_result_detected == BHY_TRUE) {
		bhy_reinit(client_data);
		client_data->recover_from_disaster = BHY_TRUE;
		atomic_set(&client_data->reset_flag, RESET_FLAG_READY);
	}
}

#if 0
static irqreturn_t bhy_irq_handler(int irq, void *handle)
{
	struct bhy_client_data *client_data = handle;

	if (client_data == NULL)
		return IRQ_HANDLED;

	bhy_get_ap_timestamp(&client_data->timestamp_irq);

	schedule_work(&client_data->irq_work);


#ifdef BHY_LEVEL_TRIGGERED_IRQ_SUPPORT
	/* Disable IRQ to prevent additional entry */
	if (client_data->irq_enabled == BHY_TRUE) {
		client_data->irq_enabled = BHY_FALSE;
		disable_irq_nosync(client_data->data_bus.irq);
	}
#endif /*~ BHY_LEVEL_TRIGGERED_IRQ_SUPPORT */

	return IRQ_HANDLED;
}
#else
	static void bhy_irq_handler(void)
{
	struct bhy_client_data *client_data = client_data_1;

//	printk("########bhy_irq_handler ###########\n");
	if (client_data == NULL)
		return IRQ_HANDLED;
//	printk("########bhy_irq_handler 1 ########### \n");

	bhy_get_ap_timestamp(&client_data->timestamp_irq);

	schedule_work(&client_data->irq_work);

	mt_eint_mask(CUST_EINT_GSE_1_NUM);
}
#endif

static void bhy_irq_work_func(struct work_struct *work)
{
	struct bhy_client_data *client_data = container_of(work
			, struct bhy_client_data, irq_work);
	int reset_flag_copy, in_suspend_copy;
	int ret;
	u8 timestamp_fw[4];
	struct frame_queue *q = &client_data->data_queue;
#ifdef BHY_AR_HAL_SUPPORT
	struct frame_queue *qa = &client_data->data_queue_ar;
#endif /*~ BHY_AR_HAL_SUPPORT */
#ifdef BHY_DEBUG
	u8 irq_status;
#endif /*~ BHY_DEBUG */


	printk("############bhy_irq_work_func###########\n");
	/* Detect reset event */
	reset_flag_copy = atomic_read(&client_data->reset_flag);
	if (reset_flag_copy == RESET_FLAG_TODO) {
		atomic_set(&client_data->reset_flag, RESET_FLAG_READY);
		mt_eint_unmask(CUST_EINT_GSE_1_NUM);
		return;
	}
//	printk("############1###########\n");

	in_suspend_copy = atomic_read(&client_data->in_suspend);
	if (in_suspend_copy) {
		wake_lock(&client_data->wlock);
		msleep(20);
	}
//	printk("############2###########\n");

#ifdef BHY_DEBUG
	if (client_data->enable_irq_log) {
		irq_status = 0;
		mutex_lock(&client_data->mutex_bus_op);
		ret = bhy_read_reg(client_data, BHY_REG_INT_STATUS,
			&irq_status, 1);
		mutex_unlock(&client_data->mutex_bus_op);
		if (ret < 0)
			PERR("Read IRQ status failed");
		PDEBUG("In IRQ, timestamp: %llu, irq_type: 0x%02X",
			client_data->timestamp_irq, irq_status);
	}
#endif /*~ BHY_DEBUG */
//	printk("############3###########\n");

	/* Report timestamp sync */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_HOST_IRQ_TIMESTAMP_1,
		timestamp_fw, 4);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0)
		PERR("Get firmware timestamp failed");
	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_TIMESTAMP_SYNC;
	memcpy(q->frames[q->head].data,
		&client_data->timestamp_irq, sizeof(u64));
	memcpy(q->frames[q->head].data + sizeof(u64),
		&timestamp_fw, sizeof(timestamp_fw));
#ifdef BHY_TS_LOGGING_SUPPORT
	++client_data->irq_count;
	memcpy(q->frames[q->head].data + sizeof(u64) + sizeof(timestamp_fw),
		&client_data->irq_count, sizeof(u32));
#endif /*~ BHY_TS_LOGGING_SUPPORT */
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
//	printk("############4###########\n");
	mutex_unlock(&q->lock);
#ifdef BHY_AR_HAL_SUPPORT
	mutex_lock(&qa->lock);
	qa->frames[qa->head].handle = BHY_SENSOR_HANDLE_TIMESTAMP_SYNC;
	memcpy(qa->frames[qa->head].data,
		&client_data->timestamp_irq, sizeof(u64));
	memcpy(qa->frames[qa->head].data + sizeof(u64),
		&timestamp_fw, sizeof(timestamp_fw));
	if (qa->head == BHY_FRAME_SIZE_AR - 1)
		qa->head = 0;
	else
		++qa->head;
	if (qa->head == qa->tail) {
		if (qa->tail == BHY_FRAME_SIZE_AR - 1)
			qa->tail = 0;
		else
			++qa->tail;
	}
//	printk("############5###########\n");
	mutex_unlock(&qa->lock);
#endif /*~ BHY_AR_HAL_SUPPORT */
//	printk("############6###########\n");

	/* Read FIFO data */
	bhy_read_fifo_data(client_data, reset_flag_copy);
//	printk("############7###########\n");

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);
//	printk("############8###########\n");

#ifdef BHY_AR_HAL_SUPPORT
	input_event(client_data->input_ar, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input_ar);
#endif /*~ BHY_AR_HAL_SUPPORT */

	if (in_suspend_copy)
		wake_unlock(&client_data->wlock);
#ifdef BHY_LEVEL_TRIGGERED_IRQ_SUPPORT
#if 0
	/* Enable IRQ after data processing */
	if (client_data->irq_enabled == BHY_FALSE) {
		client_data->irq_enabled = BHY_TRUE;
		enable_irq(client_data->data_bus.irq);
	}
#else
		mt_eint_unmask(CUST_EINT_GSE_1_NUM);
#endif
#endif /*~ BHY_LEVEL_TRIGGERED_IRQ_SUPPORT */
}

#if 0
static int bhy_request_irq(struct bhy_client_data *client_data)
{
	struct bhy_data_bus *data_bus = &client_data->data_bus;
	int ret;
	int irq_gpio;
	irq_gpio = of_get_named_gpio_flags(data_bus->dev->of_node,
		"bhy,gpio_irq", 0, NULL);
	irq_gpio = 6;
	printk("############irq_gpio=%d\n", irq_gpio);
	ret = gpio_request_one(irq_gpio, GPIOF_IN, "bhy_int");
	if (ret < 0)
		return ret;
	ret = gpio_direction_input(irq_gpio);
	if (ret < 0)
		return ret;
	data_bus->irq = mt_gpio_to_irq(irq_gpio);
	printk("#######data_bus->irq=%d\n########\n",data_bus->irq);
	INIT_WORK(&client_data->irq_work, bhy_irq_work_func);
#ifdef BHY_LEVEL_TRIGGERED_IRQ_SUPPORT
	client_data->irq_enabled = BHY_TRUE;
	ret = request_irq(data_bus->irq, bhy_irq_handler, IRQF_TRIGGER_HIGH,
		SENSOR_NAME, client_data);
	PINFO("Use level triggered IRQ");
#else
	ret = request_irq(data_bus->irq, bhy_irq_handler, IRQF_TRIGGER_RISING,
		SENSOR_NAME, client_data);
#endif /*~ BHY_LEVEL_TRIGGERED_IRQ_SUPPORT */
	if (ret < 0)
		return ret;
	ret = device_init_wakeup(data_bus->dev, 1);
	if (ret < 0) {
		PDEBUG("Init device wakeup failed");
		return ret;
	}
	return 0;
}
#else
	static int bhy_request_irq(struct bhy_client_data *client_data)
{
	int ret;
	client_data_1 = client_data;
	struct bhy_data_bus *data_bus = &client_data->data_bus;
	
	INIT_WORK(&client_data->irq_work, bhy_irq_work_func);
	printk("######bhy_request_irq ############\n");


	mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN, GPIO_GSE_1_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_GSE_1_EINT_PIN, GPIO_DIR_IN);
    //mt_set_gpio_pull_enable(GPIO_GSE_1_EINT_PIN, GPIO_PULL_ENABLE);
    //mt_set_gpio_pull_select(GPIO_GSE_1_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_GSE_1_NUM, CUST_EINT_GSE_1_DEBOUNCE_CN);
	
	mt_eint_registration(CUST_EINT_GSE_1_NUM, CUST_EINT_POLARITY_HIGH, bhy_irq_handler, 0);	/* disable auto-unmask */
	//mt_eint_unmask(CUST_EINT_GSE_1_NUM);

	ret = device_init_wakeup(data_bus->dev, 1);
	if (ret < 0) {
		PDEBUG("Init device wakeup failed");
		return ret;
	}
	return 0;
}
#endif

static int bhy_init_input_dev(struct bhy_client_data *client_data)
{
	struct input_dev *dev;
	int ret;

	dev = input_allocate_device();
	if (dev == NULL) {
		PERR("Allocate input device failed");
		return -ENOMEM;
	}

	dev->name = SENSOR_INPUT_DEV_NAME;
	dev->id.bustype = client_data->data_bus.bus_type;

	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_drvdata(dev, client_data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		PERR("Register input device failed");
		return ret;
	}
	client_data->input = dev;

#ifdef BHY_AR_HAL_SUPPORT
	dev = input_allocate_device();
	if (dev == NULL) {
		PERR("Allocate input device failed for AR");
		return -ENOMEM;
	}

	dev->name = SENSOR_AR_INPUT_DEV_NAME;
	dev->id.bustype = client_data->data_bus.bus_type;

	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_drvdata(dev, client_data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		PERR("Register input device for AR failed");
		return ret;
	}
	client_data->input_ar = dev;
#endif /*~ BHY_AR_HAL_SUPPORT */

	return 0;
}

static ssize_t bhy_show_rom_id(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	__le16 reg_data;
	u16 rom_id;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_ROM_VERSION_0,
		(u8 *)&reg_data, 2);
	mutex_unlock(&client_data->mutex_bus_op);

	if (ret < 0)
		return ret;
	rom_id = __le16_to_cpu(reg_data);
	ret = snprintf(buf, 32, "%d\n", (int)rom_id);

	return ret;
}

static ssize_t bhy_show_ram_id(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	__le16 reg_data;
	u16 ram_id;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_RAM_VERSION_0,
		(u8 *)&reg_data, 2);
	mutex_unlock(&client_data->mutex_bus_op);

	if (ret < 0)
		return ret;
	ram_id = __le16_to_cpu(reg_data);
	ret = snprintf(buf, 32, "%d\n", (int)ram_id);

	return ret;
}

static ssize_t bhy_show_status_bank(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	int i;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	for (i = BHY_PARAM_SYSTEM_STAUS_BANK_0;
			i <= BHY_PARAM_SYSTEM_STAUS_BANK_3; ++i) {
		ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM, i,
				(u8 *)(buf + (i - BHY_PARAM_SYSTEM_STAUS_BANK_0)
				* 16), 16);
		if (ret < 0) {
			PERR("Read BHY_PARAM_SYSTEM_STAUS_BANK_%d error",
					i - BHY_PARAM_SYSTEM_STAUS_BANK_0);
			mutex_unlock(&client_data->mutex_bus_op);
			return ret;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return BHY_SENSOR_STATUS_BANK_LEN;
}

static ssize_t bhy_store_sensor_sel(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	client_data->sensor_sel = buf[0];

	return count;
}

static ssize_t bhy_show_sensor_info(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	if (client_data->sensor_sel <= 0 ||
			client_data->sensor_sel > BHY_SENSOR_HANDLE_MAX) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Invalid sensor sel");
		return -EINVAL;
	}
	ret = bhy_read_parameter(client_data, BHY_PAGE_SENSOR,
			BHY_PARAM_SENSOR_INFO_0 + client_data->sensor_sel,
			(u8 *)buf, 16);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read parameter error");
		return ret;
	}

	return 16;
}

static ssize_t bhy_show_sensor_conf(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int sel;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	sel = client_data->sensor_sel;
	mutex_unlock(&client_data->mutex_bus_op);
	if (sel <= 0 || sel > BHY_SENSOR_HANDLE_MAX) {
		PERR("Invalid sensor sel");
		return -EINVAL;
	}

	return bhy_get_sensor_conf(client_data, sel, buf);
}

static ssize_t bhy_store_sensor_conf(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int sel;
	struct bhy_sensor_context *ct;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	sel = client_data->sensor_sel;
	mutex_unlock(&client_data->mutex_bus_op);
	if (sel <= 0 || sel > BHY_SENSOR_HANDLE_MAX) {
		PERR("Invalid sensor sel");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_sw_watchdog);
	ct = client_data->sensor_context;
	ct[sel].sample_rate = *(u16 *)buf;
	ct[sel].report_latency = *(u16 *)(buf + 2);
	client_data->inactive_count = 0;
	mutex_unlock(&client_data->mutex_sw_watchdog);
	/* Clear flush queue if sensor is de-activated */
	if (ct[sel].sample_rate == 0) {
		mutex_lock(&client_data->flush_queue.lock);
		if (client_data->flush_queue.cur == sel)
			bhy_clear_flush_queue(&client_data->flush_queue);
		mutex_unlock(&client_data->flush_queue.lock);
	}
	if (bhy_set_sensor_conf(client_data, sel, buf) < 0)
		return -EIO;

	return count;
}

static ssize_t bhy_store_sensor_flush(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 sensor_sel = buf[0];
	ret = bhy_enqueue_flush(client_data, sensor_sel);
	if (ret < 0) {
		PERR("Write sensor flush failed");
		return ret;
	}

	return count;
}

static ssize_t bhy_show_calib_profile(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	u8 param_num;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
#ifdef BHY_CALIB_PROFILE_OP_IN_FUSER_CORE
	switch (client_data->sensor_sel) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		param_num = BHY_PARAM_OFFSET_ACC_2;
		break;
	case BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD:
		param_num = BHY_PARAM_OFFSET_MAG_2;
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE:
		param_num = BHY_PARAM_OFFSET_GYRO_2;
		break;
	default:
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Invalid sensor sel");
		return -EINVAL;
	}
#else
	switch (client_data->sensor_sel) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		param_num = BHY_PARAM_OFFSET_ACC;
		break;
	case BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD:
		param_num = BHY_PARAM_OFFSET_MAG;
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE:
		param_num = BHY_PARAM_OFFSET_GYRO;
		break;
	default:
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Invalid sensor sel");
		return -EINVAL;
	}
#endif /*~ BHY_CALIB_PROFILE_OP_IN_FUSER_CORE */
	ret = bhy_read_parameter(client_data, BHY_PAGE_ALGORITHM,
		param_num, (u8 *)buf, BHY_CALIB_PROFILE_LEN);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read parameter error");
		return ret;
	}

	return BHY_CALIB_PROFILE_LEN;
}

static ssize_t bhy_store_calib_profile(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = bhy_set_calib_profile(client_data, (const u8 *)buf);
	if (ret < 0) {
		PERR("bhy_set_calib_profile failed");
		return ret;
	}

	/* Save calibration profile for disaster recovery */
	mutex_lock(&client_data->mutex_bus_op);
	switch (client_data->sensor_sel) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		memcpy(client_data->calibprofile_acc, buf, 8);
		break;
	case BHY_SENSOR_HANDLE_GEOMAGNETIC_FIELD:
		memcpy(client_data->calibprofile_mag, buf, 8);
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE:
		memcpy(client_data->calibprofile_gyro, buf, 8);
		break;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return count;
}

static ssize_t bhy_show_sic_matrix(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	u8 data[36];
	int i;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	for (i = BHY_PARAM_SIC_MATRIX_0_1; i <= BHY_PARAM_SIC_MATRIX_8; ++i) {
		ret = bhy_read_parameter(client_data, BHY_PAGE_ALGORITHM,
				i, (u8 *)(data + (i - 1) * 8),
				i == BHY_PARAM_SIC_MATRIX_8 ? 4 : 8);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read parameter error");
			return ret;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);
	ret = 0;
	for (i = 0; i < 9; ++i)
		ret += snprintf(buf + ret, 16, "%02X %02X %02X %02X\n",
				data[i * 4], data[i * 4 + 1],
				data[i * 4 + 2], data[i * 4 + 3]);

	return ret;
}

static ssize_t bhy_store_sic_matrix(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	int i;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	for (i = BHY_PARAM_SIC_MATRIX_0_1; i <= BHY_PARAM_SIC_MATRIX_8; ++i) {
		ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
				i, (u8 *)(buf + (i - 1) * 8),
				i == BHY_PARAM_SIC_MATRIX_8 ? 4 : 8);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write parameter error");
			return ret;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return count;
}

static ssize_t bhy_show_meta_event_ctrl(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	u8 data[8];
	int i, j;
	ssize_t len;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
			BHY_PARAM_SYSTEM_META_EVENT_CTRL, data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read meta event ctrl failed");
		return -EIO;
	}
	len = 0;
	len += snprintf(buf + len, 64, "Non wake up meta event\n");
	for (i = 0; i < 8; ++i) {
		for (j = 0; j < 4; ++j)
			len += snprintf(buf + len, 64,
					"Meta event #%d: event_en=%d, irq_en=%d\n",
					i * 4 + j + 1,
					(data[i] >> (j * 2 + 1)) & 1,
					(data[i] >> (j * 2)) & 1);
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
			BHY_PARAM_SYSTEM_WAKE_UP_META_EVENT_CTRL, data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read wake up meta event ctrl failed");
		return -EIO;
	}
	len += snprintf(buf + len, 64, "Wake up meta event\n");
	for (i = 0; i < 8; ++i) {
		for (j = 0; j < 4; ++j)
			len += snprintf(buf + len, 64,
					"Meta event #%d: event_en=%d, irq_en=%d\n",
					i * 4 + j + 1,
					(data[i] >> (j * 2 + 1)) & 1,
					(data[i] >> (j * 2)) & 1);
	}

	return len;
}

/* Byte0: meta event type; Byte1: event enable; Byte2: IRQ enable;
   Byte3: 0 for non-wakeup, 1 for wakeup */
static ssize_t bhy_store_meta_event_ctrl(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	int type, event_en, irq_en, for_wake_up;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	type = buf[0];
	if (type <= 0 || type > BHY_META_EVENT_MAX) {
		PERR("Invalid meta event type");
		return -EINVAL;
	}
	event_en = buf[1] & 0x1 ? BHY_TRUE : BHY_FALSE;
	irq_en = buf[2] & 0x1 ? BHY_TRUE : BHY_FALSE;
	for_wake_up = buf[3] ? BHY_TRUE : BHY_FALSE;

	ret = bhy_set_meta_event_ctrl(client_data, type, for_wake_up,
		event_en, irq_en);
	if (ret < 0)
		return -EIO;

	if (for_wake_up == BHY_TRUE) {
		client_data->mew_context[type].event_en =
			event_en == BHY_TRUE ? BHY_STATUS_ENABLED :
			BHY_STATUS_DISABLED;
		client_data->mew_context[type].irq_en =
			irq_en == BHY_TRUE ? BHY_STATUS_ENABLED :
			BHY_STATUS_DISABLED;
	} else {
		client_data->me_context[type].event_en =
			event_en == BHY_TRUE ? BHY_STATUS_ENABLED :
			BHY_STATUS_DISABLED;
		client_data->me_context[type].irq_en =
			irq_en == BHY_TRUE ? BHY_STATUS_ENABLED :
			BHY_STATUS_DISABLED;
	}

	return count;
}

static ssize_t bhy_show_fifo_ctrl(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
			BHY_PARAM_SYSTEM_FIFO_CTRL, buf,
			BHY_FIFO_CTRL_PARAM_LEN);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read fifo ctrl failed");
		return -EIO;
	}

	return BHY_FIFO_CTRL_PARAM_LEN;
}

static ssize_t bhy_store_fifo_ctrl(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = bhy_set_fifo_ctrl(client_data, (u8 *)buf);
	if (ret < 0) {
		PERR("Set fifo ctrl failed");
		return ret;
	}

	/* Save fifo control cfg for future recovery */
	memcpy(client_data->fifo_ctrl_cfg, buf, BHY_FIFO_CTRL_PARAM_LEN);

	return count;
}

#ifdef BHY_AR_HAL_SUPPORT
static ssize_t bhy_store_activate_ar_hal(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	long req;
	struct frame_queue *qa = &client_data->data_queue_ar;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = kstrtol(buf, 10, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid request");
		return -EINVAL;
	}

	mutex_lock(&qa->lock);
	qa->frames[qa->head].handle = BHY_AR_ACTIVATE;
	if (qa->head == BHY_FRAME_SIZE_AR - 1)
		qa->head = 0;
	else
		++qa->head;
	if (qa->head == qa->tail) {
		if (qa->tail == BHY_FRAME_SIZE_AR - 1)
			qa->tail = 0;
		else
			++qa->tail;
	}
	mutex_unlock(&qa->lock);

	input_event(client_data->input_ar, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input_ar);
	PDEBUG("AR HAL activate message sent");

	return count;
}
#endif /*~ BHY_AR_HAL_SUPPORT */

static ssize_t bhy_show_reset_flag(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int reset_flag_copy;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	reset_flag_copy = atomic_read(&client_data->reset_flag);
	buf[0] = (u8)reset_flag_copy;

	return 1;
}

/* 16-bit working mode value */
static ssize_t bhy_show_working_mode(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_WORKING_MODE_ENABLE, buf, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read working mode mask failed");
		return -EIO;
	}

	return BHY_FIFO_CTRL_PARAM_LEN;
}

static ssize_t bhy_store_working_mode(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_WORKING_MODE_ENABLE, (u8 *)buf, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write working mode mask failed");
		return -EIO;
	}

	return count;
}

static ssize_t bhy_show_op_mode(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	u8 data[2];
	char op_mode[64];
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_OPERATING_MODE, data, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read op mode failed");
		return -EIO;
	}

	switch (data[1]) {
	case 0:
		strlcpy(op_mode, "SLEEP", 64);
		break;
	case 1:
		strlcpy(op_mode, "ACCONLY", 64);
		break;
	case 2:
		strlcpy(op_mode, "GYROONLY", 64);
		break;
	case 3:
		strlcpy(op_mode, "MAGONLY", 64);
		break;
	case 4:
		strlcpy(op_mode, "ACCGYRO", 64);
		break;
	case 5:
		strlcpy(op_mode, "ACCMAG", 64);
		break;
	case 6:
		strlcpy(op_mode, "MAGGYRO", 64);
		break;
	case 7:
		strlcpy(op_mode, "AMG", 64);
		break;
	case 8:
		strlcpy(op_mode, "IMUPLUS", 64);
		break;
	case 9:
		strlcpy(op_mode, "COMPASS", 64);
		break;
	case 10:
		strlcpy(op_mode, "M4G", 64);
		break;
	case 11:
		strlcpy(op_mode, "NDOF", 64);
		break;
	case 12:
		strlcpy(op_mode, "NDOF_FMC_OFF", 64);
		break;
	case 13:
		strlcpy(op_mode, "NDOF_GEORV", 64);
		break;
	case 14:
		strlcpy(op_mode, "NDOF_GEORV_FMC_OFF", 64);
		break;
	default:
		snprintf(op_mode, 64, "Unrecoginized op mode[%d]",
				data[1]);
		break;
	}

	ret = snprintf(buf, 128, "Current op mode: %s, odr: %dHz\n",
			op_mode, data[0]);

	return ret;
}

static ssize_t bhy_show_bsx_version(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	u8 data[8];
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_BSX_VERSION, data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read BSX version failed");
		return -EIO;
	}

	ret = snprintf(buf, 128, "%d.%d.%d.%d\n",
			*(u16 *)data, *(u16 *)(data + 2),
			*(u16 *)(data + 4), *(u16 *)(data + 6));

	return ret;
}

static ssize_t bhy_show_driver_version(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);

	return ret;
}

#ifdef BHY_AR_HAL_SUPPORT
static ssize_t bhy_show_fifo_frame_ar(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	struct frame_queue *qa = &client_data->data_queue_ar;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&qa->lock);
	if (qa->tail == qa->head) {
		mutex_unlock(&qa->lock);
		return 0;
	}
	memcpy(buf, &qa->frames[qa->tail], sizeof(struct fifo_frame));
	if (qa->tail == BHY_FRAME_SIZE_AR - 1)
		qa->tail = 0;
	else
		++qa->tail;
	mutex_unlock(&qa->lock);

	return sizeof(struct fifo_frame);
}
#endif /*~ BHY_AR_HAL_SUPPORT */

static ssize_t bhy_show_bmi160_foc_offset_acc(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	u8 data[3];

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bmi160_read_reg(client_data, BMI160_REG_ACC_OFFSET_X,
		data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read hw reg failed");
		return ret;
	}

	return snprintf(buf, 64, "%11d %11d %11d\n",
		*(s8 *)data, *(s8 *)(data + 1), *(s8 *)(data + 2));
}

static ssize_t bhy_store_bmi160_foc_offset_acc(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int temp[3];
	s8 data[3];
	u8 val;
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = sscanf(buf, "%11d %11d %11d", &temp[0], &temp[1], &temp[2]);
	if (ret != 3) {
		PERR("Invalid input");
		return -EINVAL;
	}
	data[0] = temp[0];
	data[1] = temp[1];
	data[2] = temp[2];
	mutex_lock(&client_data->mutex_bus_op);
	/* Write offset */
	ret = bmi160_write_reg(client_data, BMI160_REG_ACC_OFFSET_X,
		(u8 *)data, sizeof(data));
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write hw reg failed");
		return ret;
	}
	/* Write offset enable bit */
	ret = bmi160_read_reg(client_data, BMI160_REG_OFFSET_6,
		&val, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read hw reg for enable bit failed");
		return ret;
	}
	val |= BMI160_OFFSET_6_BIT_ACC_EN;
	ret = bmi160_write_reg(client_data, BMI160_REG_OFFSET_6,
		&val, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write hw reg for enable bit failed");
		return ret;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return count;
}

static ssize_t bhy_show_bmi160_foc_offset_gyro(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	s8 data[4];
	s16 x, y, z, h;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bmi160_read_reg(client_data, BMI160_REG_GYRO_OFFSET_X,
		(u8 *)data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read hw reg failed");
		return ret;
	}

	/* left shift 6 bits to make sign bit msb, then shift back */
	h = (data[3] & BMI160_OFFSET_6_MASK_GYRO_X) >>
		BMI160_OFFSET_6_OFFSET_GYRO_X;
	x = ((h << 8) | data[0]) << 6;
	x >>= 6;
	h = (data[3] & BMI160_OFFSET_6_MASK_GYRO_Y) >>
		BMI160_OFFSET_6_OFFSET_GYRO_Y;
	y = ((h << 8) | data[1]) << 6;
	y >>= 6;
	h = (data[3] & BMI160_OFFSET_6_MASK_GYRO_Z) >>
		BMI160_OFFSET_6_OFFSET_GYRO_Z;
	z = ((h << 8) | data[2]) << 6;
	z >>= 6;

	return snprintf(buf, 64, "%11d %11d %11d\n", x, y, z);
}

static ssize_t bhy_store_bmi160_foc_offset_gyro(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int x, y, z;
	u8 data[3];
	u8 val;
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = sscanf(buf, "%11d %11d %11d", &x, &y, &z);
	if (ret != 3) {
		PERR("Invalid input");
		return -EINVAL;
	}

	data[0] = x & 0xFF;
	data[1] = y & 0xFF;
	data[2] = z & 0xFF;
	mutex_lock(&client_data->mutex_bus_op);
	/* Set low 8-bit */
	ret = bmi160_write_reg(client_data, BMI160_REG_GYRO_OFFSET_X,
		data, sizeof(data));
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write hw reg failed");
		return ret;
	}
	/* Set high bit, extract 9th bit and 10th bit from x, y, z
	* Set enable bit */
	ret = bmi160_read_reg(client_data, BMI160_REG_OFFSET_6,
		&val, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read hw reg for enable bit failed");
		return ret;
	}
	val &= ~BMI160_OFFSET_6_MASK_GYRO_X;
	val |= ((x >> 8) & 0x03) << BMI160_OFFSET_6_OFFSET_GYRO_X;
	val &= ~BMI160_OFFSET_6_MASK_GYRO_Y;
	val |= ((y >> 8) & 0x03) << BMI160_OFFSET_6_OFFSET_GYRO_Y;
	val &= ~BMI160_OFFSET_6_MASK_GYRO_Z;
	val |= ((z >> 8) & 0x03) << BMI160_OFFSET_6_OFFSET_GYRO_Z;
	val |= BMI160_OFFSET_6_BIT_GYRO_EN;
	ret = bmi160_write_reg(client_data, BMI160_REG_OFFSET_6,
		&val, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write hw reg for enable bit failed");
		return ret;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return count;
}

static ssize_t bhy_show_bmi160_foc_conf(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int x, y, z, g;
	int out[3], in[3], i;
	const char *disp[4] = {
		"disabled",
		"1g",
		"-1g",
		"0"
	};
	u8 conf;
	ssize_t ret = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	conf = client_data->bmi160_foc_conf;

	x = (conf & BMI160_FOC_CONF_MASK_ACC_X) >> BMI160_FOC_CONF_OFFSET_ACC_X;
	y = (conf & BMI160_FOC_CONF_MASK_ACC_Y) >> BMI160_FOC_CONF_OFFSET_ACC_Y;
	z = (conf & BMI160_FOC_CONF_MASK_ACC_Z) >> BMI160_FOC_CONF_OFFSET_ACC_Z;
	g = (conf & BMI160_FOC_CONF_MASK_GYRO) >> BMI160_FOC_CONF_OFFSET_GYRO;

	out[0] = x;
	out[1] = y;
	out[2] = z;
	for (i = 0; i < 3; ++i) {
		in[i] = out[0] * client_data->mapping_matrix_acc_inv[0][i] +
			out[1] * client_data->mapping_matrix_acc_inv[1][i] +
			out[2] * client_data->mapping_matrix_acc_inv[2][i];
		switch (in[i]) {
		case -1:
			in[i] = 2;
			break;
		case -2:
			in[i] = 1;
			break;
		case -3:
			in[i] = 3;
			break;
		default:
			break;
		}
	}

	ret += snprintf(buf + ret, 128, "Acc conf: %s %s %s Gyro: %s\n",
		disp[x], disp[y], disp[z], g ? "enabled" : "disabled");
	ret += snprintf(buf + ret, 128, "Original acc conf: %s %s %s\n",
		disp[in[0]], disp[in[1]], disp[in[2]]);

	return ret;
}

static ssize_t bhy_store_bmi160_foc_conf(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i;
	int mask, offset;
	u8 conf = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	for (i = 0; i < count; ++i) {
		mask = 0;
		switch (buf[i]) {
		case 'x':
		case 'X':
			mask = BMI160_FOC_CONF_MASK_ACC_X;
			offset = BMI160_FOC_CONF_OFFSET_ACC_X;
			break;
		case 'y':
		case 'Y':
			mask = BMI160_FOC_CONF_MASK_ACC_Y;
			offset = BMI160_FOC_CONF_OFFSET_ACC_Y;
			break;
		case 'z':
		case 'Z':
			mask = BMI160_FOC_CONF_MASK_ACC_Z;
			offset = BMI160_FOC_CONF_OFFSET_ACC_Z;
			break;
		case 'g':
		case 'G':
			mask = BMI160_FOC_CONF_MASK_GYRO;
			offset = BMI160_FOC_CONF_OFFSET_GYRO;
			break;
		}
		if (mask == 0)
			continue;
		if (i >= count - 1)
			break;
		conf &= ~mask;
		++i;
		switch (buf[i]) {
		case 'x': /* Set to disable */
		case 'X':
			conf |= BMI160_FOC_CONF_DISABLE << offset;
			break;
		case 'g': /* set to 1g, enable for gyro */
		case 'G':
			conf |= BMI160_FOC_CONF_1G << offset;
			break;
		case 'n': /* set to -1g */
		case 'N':
			if (offset == BMI160_FOC_CONF_OFFSET_GYRO)
				break;
			conf |= BMI160_FOC_CONF_N1G << offset;
			break;
		case '0': /* set to 0 */
			if (offset == BMI160_FOC_CONF_OFFSET_GYRO)
				break;
			conf |= BMI160_FOC_CONF_0 << offset;
			break;
		}
	}
	client_data->bmi160_foc_conf = conf;

	return count;
}

static ssize_t bhy_show_bmi160_foc_exec(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64,
		"Use echo 1 > bmi160_foc_exec to begin foc\n");
}

static ssize_t bhy_store_bmi160_foc_exec(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	long req;
	int for_acc, for_gyro;
	int pmu_status_acc = 0, pmu_status_gyro = 0;
	u8 conf;
	u8 reg_data;
	int retry;
	int in[3], out[3], i;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 16, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid input");
		return -EINVAL;
	}
	conf = client_data->bmi160_foc_conf;

	/* Recalc acc conf according to real axis mapping */
	out[0] = (conf & BMI160_FOC_CONF_MASK_ACC_X) >>
		BMI160_FOC_CONF_OFFSET_ACC_X;
	out[1] = (conf & BMI160_FOC_CONF_MASK_ACC_Y) >>
		BMI160_FOC_CONF_OFFSET_ACC_Y;
	out[2] = (conf & BMI160_FOC_CONF_MASK_ACC_Z) >>
		BMI160_FOC_CONF_OFFSET_ACC_Z;
	for (i = 0; i < 3; ++i) {
		in[i] = out[0] * client_data->mapping_matrix_acc_inv[0][i] +
			out[1] * client_data->mapping_matrix_acc_inv[1][i] +
			out[2] * client_data->mapping_matrix_acc_inv[2][i];
		switch (in[i]) {
		case -1:
			in[i] = 2;
			break;
		case -2:
			in[i] = 1;
			break;
		case -3:
			in[i] = 3;
			break;
		default:
			break;
		}
	}
	conf &= ~BMI160_FOC_CONF_MASK_ACC_X;
	conf |= in[0] << BMI160_FOC_CONF_OFFSET_ACC_X;
	conf &= ~BMI160_FOC_CONF_MASK_ACC_Y;
	conf |= in[1] << BMI160_FOC_CONF_OFFSET_ACC_Y;
	conf &= ~BMI160_FOC_CONF_MASK_ACC_Z;
	conf |= in[2] << BMI160_FOC_CONF_OFFSET_ACC_Z;

	for_acc = (conf & 0x3F) ? 1 : 0;
	for_gyro = (conf & 0xC0) ? 1 : 0;
	if (for_acc == 0 && for_gyro == 0) {
		PERR("No need to do foc");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_bus_op);
	/* Set acc range to 4g */
	if (for_acc) {
		ret = bmi160_read_reg(client_data, BMI160_REG_ACC_RANGE,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read acc range failed");
			return -EIO;
		}
		if ((reg_data & BMI160_ACC_RANGE_MASK) != BMI160_ACC_RANGE_4G) {
			reg_data = BMI160_ACC_RANGE_4G;
			ret = bmi160_write_reg(client_data,
				BMI160_REG_ACC_RANGE,
				&reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Write acc range failed");
				return -EIO;
			}
			retry = BMI160_OP_RETRY;
			do {
				ret = bmi160_read_reg(client_data,
					BMI160_REG_ACC_RANGE, &reg_data, 1);
				if (ret < 0) {
					mutex_unlock(
						&client_data->mutex_bus_op);
					PERR("Read acc range #2 failed");
					return -EIO;
				}
				if ((reg_data & BMI160_ACC_RANGE_MASK) ==
					BMI160_ACC_RANGE_4G)
					break;
				udelay(50);
			} while (--retry);
			if (retry == 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Wait for acc 4g range failed");
				return -EBUSY;
			}
		}
	}
	/* Set normal power mode */
	ret = bmi160_read_reg(client_data, BMI160_REG_PMU_STATUS,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read acc pmu status failed");
		return -EIO;
	}
	pmu_status_acc = (reg_data & BMI160_PMU_STATUS_MASK_ACC)
		>> BMI160_PMU_STATUS_OFFSET_ACC;
	pmu_status_gyro = (reg_data & BMI160_PMU_STATUS_MASK_GYRO)
		>> BMI160_PMU_STATUS_OFFSET_GYRO;
	if (for_acc && pmu_status_acc != BMI160_PMU_STATUS_NORMAL) {
		reg_data = BMI160_CMD_PMU_BASE_ACC + BMI160_PMU_STATUS_NORMAL;
		ret = bmi160_write_reg(client_data, BMI160_REG_CMD,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu cmd failed");
			return -EIO;
		}
		retry = BMI160_OP_RETRY;
		do {
			ret = bmi160_read_reg(client_data,
				BMI160_REG_PMU_STATUS, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu status #2 failed");
				return -EIO;
			}
			reg_data = (reg_data & BMI160_PMU_STATUS_MASK_ACC)
				>> BMI160_PMU_STATUS_OFFSET_ACC;
			if (reg_data == BMI160_PMU_STATUS_NORMAL)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for acc normal mode status failed");
			return -EBUSY;
		}
	}
	if (for_gyro && pmu_status_gyro != BMI160_PMU_STATUS_NORMAL) {
		reg_data = BMI160_CMD_PMU_BASE_GYRO + BMI160_PMU_STATUS_NORMAL;
		ret = bmi160_write_reg(client_data, BMI160_REG_CMD,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write gyro pmu cmd failed");
			return -EIO;
		}
		retry = BMI160_OP_RETRY;
		do {
			ret = bmi160_read_reg(client_data,
				BMI160_REG_PMU_STATUS, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read gyro pmu status #2 failed");
				return -EIO;
			}
			reg_data = (reg_data & BMI160_PMU_STATUS_MASK_GYRO)
				>> BMI160_PMU_STATUS_OFFSET_GYRO;
			if (reg_data == BMI160_PMU_STATUS_NORMAL)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for gyro normal mode status failed");
			return -EBUSY;
		}
	}
	/* Write offset enable bits */
	ret = bmi160_read_reg(client_data, BMI160_REG_OFFSET_6, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read offset config failed");
		return -EIO;
	}
	if (for_acc)
		reg_data |= BMI160_OFFSET_6_BIT_ACC_EN;
	if (for_gyro)
		reg_data |= BMI160_OFFSET_6_BIT_GYRO_EN;
	ret = bmi160_write_reg(client_data, BMI160_REG_OFFSET_6, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write offset enable failed");
		return ret;
	}
	/* Write configuration status */
	ret = bmi160_write_reg(client_data, BMI160_REG_FOC_CONF, &conf, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write configuration status failed");
		return ret;
	}
	/* Execute FOC command */
	reg_data = BMI160_CMD_START_FOC;
	ret = bmi160_write_reg(client_data, BMI160_REG_CMD, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Execute FOC failed");
		return ret;
	}
	reg_data = 0;
	retry = BMI160_OP_RETRY;
	do {
		ret = bmi160_read_reg(client_data, BMI160_REG_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read status after exec FOC failed");
			return ret;
		}
		if (reg_data & BMI160_STATUS_BIT_FOC_RDY)
			break;
		usleep_range(2000, 2000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Cannot read the right status after exec FOC");
		return -EBUSY;
	}
	/* Restore old power mode */
	if (for_acc && pmu_status_acc != BMI160_PMU_STATUS_NORMAL) {
		reg_data = BMI160_CMD_PMU_BASE_ACC
			+ pmu_status_acc;
		ret = bmi160_write_reg(client_data, BMI160_REG_CMD,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu cmd #2 failed");
			return -EIO;
		}
		retry = BMI160_OP_RETRY;
		do {
			ret = bmi160_read_reg(client_data,
				BMI160_REG_PMU_STATUS, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu status #2 failed");
				return -EIO;
			}
			reg_data = (reg_data & BMI160_PMU_STATUS_MASK_ACC)
				>> BMI160_PMU_STATUS_OFFSET_ACC;
			if (reg_data == pmu_status_acc)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for acc normal mode status #2 failed");
			return -EBUSY;
		}
	}
	if (for_gyro && pmu_status_gyro != BMI160_PMU_STATUS_NORMAL) {
		reg_data = BMI160_CMD_PMU_BASE_GYRO
			+ pmu_status_gyro;
		ret = bmi160_write_reg(client_data, BMI160_REG_CMD,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write gyro pmu cmd #2 failed");
			return -EIO;
		}
		retry = BMI160_OP_RETRY;
		do {
			ret = bmi160_read_reg(client_data,
				BMI160_REG_PMU_STATUS, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read gyro pmu status #2 failed");
				return -EIO;
			}
			reg_data = (reg_data & BMI160_PMU_STATUS_MASK_GYRO)
				>> BMI160_PMU_STATUS_OFFSET_GYRO;
			if (reg_data == pmu_status_gyro)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for gyro normal mode status #2 failed");
			return -EBUSY;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);
	/* Reset foc conf*/
	client_data->bmi160_foc_conf = 0;

	PINFO("FOC executed successfully");

	return count;
}

static ssize_t bhy_show_bmi160_foc_save_to_nvm(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64,
		"Use echo 1 > bmi160_foc_save_to_nvm to save to nvm\n");
}

static ssize_t bhy_store_bmi160_foc_save_to_nvm(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	long req;
	u8 reg_data;
	int retry;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 16, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid input");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bmi160_read_reg(client_data, BMI160_REG_CONF, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read conf failed");
		return ret;
	}
	reg_data |= BMI160_CONF_BIT_NVM;
	ret = bmi160_write_reg(client_data, BMI160_REG_CONF, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Enable NVM writing failed");
		return ret;
	}
	reg_data = BMI160_CMD_PROG_NVM;
	ret = bmi160_write_reg(client_data, BMI160_REG_CMD, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Execute NVM prog failed");
		return ret;
	}
	reg_data = 0;
	retry = BMI160_OP_RETRY;
	do {
		ret = bmi160_read_reg(client_data, BMI160_REG_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read status after exec FOC failed");
			return ret;
		}
		if (reg_data & BMI160_STATUS_BIT_NVM_RDY)
			break;
		usleep_range(2000, 2000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Cannot read the right status after write to NVM");
		return -EBUSY;
	}
	ret = bmi160_read_reg(client_data, BMI160_REG_CONF, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read conf after exec nvm prog failed");
		return ret;
	}
	reg_data &= ~BMI160_CONF_BIT_NVM;
	ret = bmi160_write_reg(client_data, BMI160_REG_CONF, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Disable NVM writing failed");
		return ret;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	PINFO("NVM successfully written");

	return count;
}

static ssize_t bhy_show_bma2x2_foc_offset(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	s8 data[3];

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_read_reg(client_data, BMA2X2_REG_OFC_OFFSET_X,
		(u8 *)data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read hw reg failed");
		return ret;
	}

	return snprintf(buf, 64, "%11d %11d %11d\n", data[0], data[1], data[2]);
}

static ssize_t bhy_store_bma2x2_foc_offset(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int x, y, z;
	s8 data[3];
	int ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = sscanf(buf, "%11d %11d %11d", &x, &y, &z);
	if (ret != 3) {
		PERR("Invalid input");
		return -EINVAL;
	}
	data[0] = x & 0xFF;
	data[1] = y & 0xFF;
	data[2] = z & 0xFF;
	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_write_reg(client_data, BMA2X2_REG_OFC_OFFSET_X,
		(u8 *)data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write hw reg failed");
		return ret;
	}

	return count;
}

static ssize_t bhy_show_bma2x2_foc_conf(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int x, y, z;
	int out[3], in[3], i;
	const char *disp[4] = {
		"disabled",
		"1g",
		"-1g",
		"0"
	};
	u8 conf;
	ssize_t ret = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	conf = client_data->bma2x2_foc_conf;

	x = (conf & BMA2X2_OFC_CONF_MASK_X) >> BMA2X2_OFC_CONF_OFFSET_X;
	y = (conf & BMA2X2_OFC_CONF_MASK_Y) >> BMA2X2_OFC_CONF_OFFSET_Y;
	z = (conf & BMA2X2_OFC_CONF_MASK_Z) >> BMA2X2_OFC_CONF_OFFSET_Z;

	out[0] = x;
	out[1] = y;
	out[2] = z;
	for (i = 0; i < 3; ++i) {
		in[i] = out[0] * client_data->mapping_matrix_acc_inv[0][i] +
			out[1] * client_data->mapping_matrix_acc_inv[1][i] +
			out[2] * client_data->mapping_matrix_acc_inv[2][i];
		switch (in[i]) {
		case -1:
			in[i] = 2;
			break;
		case -2:
			in[i] = 1;
			break;
		case -3:
			in[i] = 3;
			break;
		default:
			break;
		}
	}

	ret += snprintf(buf + ret, 128, "Acc conf: %s %s %s\n",
		disp[x], disp[y], disp[z]);
	ret += snprintf(buf + ret, 128, "Original acc conf: %s %s %s\n",
		disp[in[0]], disp[in[1]], disp[in[2]]);

	return ret;
}

static ssize_t bhy_store_bma2x2_foc_conf(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i;
	int mask, offset;
	u8 conf = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	for (i = 0; i < count; ++i) {
		mask = 0;
		switch (buf[i]) {
		case 'x':
		case 'X':
			mask = BMA2X2_OFC_CONF_MASK_X;
			offset = BMA2X2_OFC_CONF_OFFSET_X;
			break;
		case 'y':
		case 'Y':
			mask = BMA2X2_OFC_CONF_MASK_Y;
			offset = BMA2X2_OFC_CONF_OFFSET_Y;
			break;
		case 'z':
		case 'Z':
			mask = BMA2X2_OFC_CONF_MASK_Z;
			offset = BMA2X2_OFC_CONF_OFFSET_Z;
			break;
		}
		if (mask == 0)
			continue;
		if (i >= count - 1)
			break;
		conf &= ~mask;
		++i;
		switch (buf[i]) {
		case 'x': /* Set to disable */
		case 'X':
			conf |= BMA2X2_OFC_CONF_DISABLE << offset;
			break;
		case 'g': /* set to 1g, enable for gyro */
		case 'G':
			conf |= BMA2X2_OFC_CONF_1G << offset;
			break;
		case 'n': /* set to -1g */
		case 'N':
			conf |= BMA2X2_OFC_CONF_N1G << offset;
			break;
		case '0': /* set to 0 */
			conf |= BMA2X2_OFC_CONF_0 << offset;
			break;
		}
	}
	client_data->bma2x2_foc_conf = conf;

	return count;
}

static ssize_t bhy_show_bma2x2_foc_exec(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64,
		"Use echo 1 > bma2x2_foc_exec to begin foc\n");
}

static ssize_t bhy_store_bma2x2_foc_exec(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	long req;
	u8 pmu_status_old;
	u8 conf;
	u8 reg_data;
	int retry;
	int in[3], out[3], i;
	u8 trigger_axis[3] = {
		BMA2X2_CAL_TRIGGER_X,
		BMA2X2_CAL_TRIGGER_Y,
		BMA2X2_CAL_TRIGGER_Z
	};

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 16, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid input");
		return -EINVAL;
	}
	conf = client_data->bma2x2_foc_conf;

	/* Recalc acc conf according to real axis mapping */
	out[0] = (conf & BMA2X2_OFC_CONF_MASK_X) >>
		BMA2X2_OFC_CONF_OFFSET_X;
	out[1] = (conf & BMA2X2_OFC_CONF_MASK_Y) >>
		BMA2X2_OFC_CONF_OFFSET_Y;
	out[2] = (conf & BMA2X2_OFC_CONF_MASK_Z) >>
		BMA2X2_OFC_CONF_OFFSET_Z;
	for (i = 0; i < 3; ++i) {
		in[i] = out[0] * client_data->mapping_matrix_acc_inv[0][i] +
			out[1] * client_data->mapping_matrix_acc_inv[1][i] +
			out[2] * client_data->mapping_matrix_acc_inv[2][i];
		switch (in[i]) {
		case -1:
			in[i] = 2;
			break;
		case -2:
			in[i] = 1;
			break;
		case -3:
			in[i] = 3;
			break;
		default:
			break;
		}
	}
	conf &= ~BMA2X2_OFC_CONF_MASK_X;
	conf |= in[0] << BMA2X2_OFC_CONF_OFFSET_X;
	conf &= ~BMA2X2_OFC_CONF_MASK_Y;
	conf |= in[1] << BMA2X2_OFC_CONF_OFFSET_Y;
	conf &= ~BMA2X2_OFC_CONF_MASK_Z;
	conf |= in[2] << BMA2X2_OFC_CONF_OFFSET_Z;

	/* Set 2g range */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_RANGE,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read acc pmu range failed");
		return -EIO;
	}
	if (reg_data != BMA2X2_PMU_RANGE_2G) {
		reg_data = BMA2X2_PMU_RANGE_2G;
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_PMU_RANGE,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu range failed");
			return -EIO;
		}
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_RANGE,
				&reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu range failed");
				return -EIO;
			}
			if (reg_data == BMA2X2_PMU_RANGE_2G)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for 4g range failed");
			return -EBUSY;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	/* Set BW to 62.5Hz if it is greater than 125Hz */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_BW,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read acc pmu range failed");
		return -EIO;
	}
	if (reg_data > BMA2X2_PMU_BW_125) {
		reg_data = BMA2X2_PMU_BW_62_5;
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_PMU_BW,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu bw failed");
			return -EIO;
		}
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_BW,
				&reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu range failed");
				return -EIO;
			}
			if (reg_data == BMA2X2_PMU_BW_62_5)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for 62.5Hz BW failed");
			return -EBUSY;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	/* Set normal power mode */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_LPW,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read acc pmu status failed");
		return -EIO;
	}
	pmu_status_old = reg_data;
	reg_data &= BMA2X2_PMU_CONF_MASK;
	if (reg_data != BMA2X2_PMU_CONF_NORMAL) {
		reg_data = BMA2X2_PMU_CONF_NORMAL;
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_PMU_LPW,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu cmd failed");
			return -EIO;
		}
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data,
				BMA2X2_REG_PMU_LPW, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu status #2 failed");
				return -EIO;
			}
			reg_data &= BMA2X2_PMU_CONF_MASK;
			if (reg_data == BMA2X2_PMU_CONF_NORMAL)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for acc normal mode status failed");
			return -EBUSY;
		}
	}
	/* Write configuration status */
	ret = bma2x2_write_reg(client_data, BMA2X2_REG_OFC_SETTING, &conf, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write configuration status failed");
		return ret;
	}
	/* Execute FOC command */
	ret = bma2x2_read_reg(client_data,
		BMA2X2_REG_OFC_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read ofc_ctrl failed");
		return -EIO;
	}
	if ((reg_data & BMA2X2_CAL_RDY_MASK) == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("OFC cal rdy status error!");
		return -EIO;
	}
	for (i = 0; i < 3; ++i) {
		if (in[i] == 0) /* disabled */
			continue;
		reg_data = trigger_axis[i];
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_OFC_CTRL,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Execute FOC failed");
			return ret;
		}
		reg_data = 0;
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data,
				BMA2X2_REG_OFC_CTRL, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read ofc_ctrl failed");
				return -EIO;
			}
			if (reg_data & BMA2X2_CAL_RDY_MASK)
				break;
			usleep_range(2000, 2000);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Cannot read the right status after exec FOC");
			return -EBUSY;
		}
	}
	/* Restore old power mode */
	reg_data = pmu_status_old;
	reg_data &= BMA2X2_PMU_CONF_MASK;
	if (reg_data != BMA2X2_PMU_CONF_NORMAL) {
		reg_data = pmu_status_old;
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_PMU_LPW,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu cmd #2 failed");
			return -EIO;
		}
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data,
				BMA2X2_REG_PMU_LPW, &reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu status #2 failed");
				return -EIO;
			}
			if (reg_data == pmu_status_old)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for acc normal mode status #2 failed");
			return -EBUSY;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	/* Restore 4g range */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_RANGE,
		&reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read acc pmu range failed");
		return -EIO;
	}
	if (reg_data != BMA2X2_PMU_RANGE_4G) {
		reg_data = BMA2X2_PMU_RANGE_4G;
		ret = bma2x2_write_reg(client_data, BMA2X2_REG_PMU_RANGE,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Write acc pmu range failed");
			return -EIO;
		}
		retry = BMA2X2_OP_RETRY;
		do {
			ret = bma2x2_read_reg(client_data, BMA2X2_REG_PMU_RANGE,
				&reg_data, 1);
			if (ret < 0) {
				mutex_unlock(&client_data->mutex_bus_op);
				PERR("Read acc pmu range failed");
				return -EIO;
			}
			if (reg_data == BMA2X2_PMU_RANGE_4G)
				break;
			udelay(50);
		} while (--retry);
		if (retry == 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Wait for 4g range failed");
			return -EBUSY;
		}
	}
	mutex_unlock(&client_data->mutex_bus_op);

	/* Reset foc conf*/
	client_data->bma2x2_foc_conf = 0;

	PINFO("FOC executed successfully");

	return count;
}

static ssize_t bhy_show_self_test(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64,
		"Use echo 1 > self_test to do self-test\n");
}

static ssize_t bhy_store_self_test(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	long req;
	u8 reg_data;
	int retry;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 16, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid input");
		return -EINVAL;
	}

	atomic_set(&client_data->reset_flag, RESET_FLAG_SELF_TEST);

	mutex_lock(&client_data->mutex_bus_op);
	/* Disable CPU run from chip control */
	reg_data = 0;
	ret = bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip control reg failed");
		return -EIO;
	}
	retry = 1000;
	do {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			return -EIO;
		}
		if (reg_data & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE)
			break;
		usleep_range(10000, 10000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Wait for chip idle status timed out");
		return -EBUSY;
	}
	/* Write self test bit */
	ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read host ctrl reg failed");
		return -EIO;
	}
	reg_data |= HOST_CTRL_MASK_SELF_TEST_REQ;
	/*reg_data &= ~HOST_CTRL_MASK_ALGORITHM_STANDBY;*/
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host ctrl reg failed");
		return -EIO;
	}
	/* Enable CPU run from chip control */
	reg_data = 1;
	ret = bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &reg_data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write chip control reg failed");
		return -EIO;
	}
	retry = 1000;
	do {
		ret = bhy_read_reg(client_data, BHY_REG_CHIP_STATUS,
			&reg_data, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Read chip status failed");
			return -EIO;
		}
		if (!(reg_data & BHY_CHIP_STATUS_BIT_FIRMWARE_IDLE))
			break;
		usleep_range(10000, 10000);
	} while (--retry);
	if (retry == 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Wait for chip running status timed out");
		return -EBUSY;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	return count;
}

static ssize_t bhy_show_self_test_result(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i, handle, count;
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = 0;
	count = 0;
	for (i = 0; i < PHYSICAL_SENSOR_COUNT; ++i) {
		if (client_data->self_test_result[i] != -1) {
			switch (i) {
			case PHYSICAL_SENSOR_INDEX_ACC:
				handle = BHY_PHYS_HANDLE_ACC;
				break;
			case PHYSICAL_SENSOR_INDEX_MAG:
				handle = BHY_PHYS_HANDLE_MAG;
				break;
			case PHYSICAL_SENSOR_INDEX_GYRO:
				handle = BHY_PHYS_HANDLE_GYRO;
				break;
			}
			ret += snprintf(buf + ret, 64,
				"Result for sensor[%d]: %d\n",
				handle, client_data->self_test_result[i]);
			++count;
		}
	}
	ret += snprintf(buf + ret, 64, "Totally %d sensor(s) tested.\n", count);

	return ret;
}

static ssize_t bhy_store_update_device_info(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	int i;
	u8 id[4];

	/* Set device type */
	for (i = 0; i < sizeof(client_data->dev_type) - 1 && buf[i]; ++i)
		client_data->dev_type[i] = buf[i];
	client_data->dev_type[i] = '\0';
	/* Set rom & ram ID */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_ROM_VERSION_0, id, 4);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read rom id failed");
		return -EIO;
	}
	client_data->rom_id = *(u16 *)id;
	client_data->ram_id = *((u16 *)id + 1);

	return count;
}

static ssize_t bhy_show_mapping_matrix_acc(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i, j;
	ssize_t ret = 0;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret += snprintf(buf + ret, 64, "Matrix:\n");
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			ret += snprintf(buf + ret, 16, "%d ",
			client_data->mapping_matrix_acc[i][j]);
		buf[ret++] = '\n';
	}

	ret += snprintf(buf + ret, 64, "Inverse:\n");
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j)
			ret += snprintf(buf + ret, 16, "%d ",
			client_data->mapping_matrix_acc_inv[i][j]);
		buf[ret++] = '\n';
	}
	buf[ret++] = '\0';

	return ret;
}

static ssize_t bhy_store_mapping_matrix_acc(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	long req;
	u8 data[16];
	int i, j, k;
	s8 m[3][6], tmp;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 16, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid input");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
		BHY_PARAM_SYSTEM_PHYSICAL_SENSOR_DETAIL_ACC,
		data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read param failed");
		return ret;
	}
	for (i = 0; i < 3; ++i)
		for (j = 0; j < 3; ++j) {
			k = i * 3 + j;
			client_data->mapping_matrix_acc[i][j] =
				k % 2 == 0 ? data[11 + k / 2] & 0xF :
				data[11 + k / 2] >> 4;
			if (client_data->mapping_matrix_acc[i][j] == 0xF)
				client_data->mapping_matrix_acc[i][j] = -1;
		}

	for (i = 0; i < 3; ++i)
		for (j = 0; j < 3; ++j) {
			m[i][j] = client_data->mapping_matrix_acc[i][j];
			m[i][j + 3] = i == j ? 1 : 0;
		}
	for (i = 0; i < 3; ++i) {
		if (m[i][i] == 0) {
			for (j = i + 1; j < 3; ++j) {
				if (m[j][i]) {
					for (k = 0; k < 6; ++k) {
						tmp = m[j][k];
						m[j][k] = m[i][k];
						m[i][k] = tmp;
					}
					break;
				}
			}
			if (j >= 3) { /* Fail check */
				PERR("Matrix invalid");
				break;
			}
		}
		if (m[i][i] < 0) {
			for (j = 0; j < 6; ++j)
				m[i][j] = -m[i][j];
		}
	}

	for (i = 0; i < 3; ++i)
		for (j = 0; j < 3; ++j)
			client_data->mapping_matrix_acc_inv[i][j] = m[i][j + 3];

	return count;
}

static ssize_t bhy_store_sensor_data_size(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int handle;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	handle = (u8)buf[0];
	if (handle >= 255) {
		PERR("Invalid handle");
		return -EIO;
	}
	client_data->sensor_context[handle].data_len = (s8)buf[1];

	return count;
}

static ssize_t bhy_show_mapping_matrix(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i, j, k, ii;
	int ret;
	ssize_t len = 0;
	u8 data[16];
	u8 map[32];
	u8 handle[3] = {
		BHY_SENSOR_HANDLE_ACCELEROMETER,
		BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED,
		BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED,
	};
	char *name[3] = {
		"Accelerometer",
		"Magnetometer",
		"Gyroscope"
	};
	u8 param;
	s8 mapping[3][3];

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	/* Check sensor existance */
	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
		BHY_PARAM_SYSTEM_PHYSICAL_SENSOR_PRESENT,
		data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read param failed");
		return ret;
	}
	for (i = 0; i < 4; ++i) {
		for (j = 0; j < 8; ++j) {
			if (data[i] & (1 << j))
				map[i * 8 + j] = 1;
			else
				map[i * 8 + j] = 0;
		}
	}

	/* Get orientation matrix */
	for (ii = 0; ii < 3; ++ii) {
		if (!map[handle[ii]])
			continue;
		param = BHY_PARAM_SYSTEM_PHYSICAL_SENSOR_DETAIL_0 + handle[ii];
		mutex_lock(&client_data->mutex_bus_op);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
			param, data, sizeof(data));
		mutex_unlock(&client_data->mutex_bus_op);
		if (ret < 0) {
			PERR("Read param failed #2");
			return ret;
		}
		for (i = 0; i < 3; ++i)
			for (j = 0; j < 3; ++j) {
				k = i * 3 + j;
				mapping[i][j] =
					k % 2 == 0 ? data[11 + k / 2] & 0xF :
					data[11 + k / 2] >> 4;
				if (mapping[i][j] == 0xF)
					mapping[i][j] = -1;
			}
		len += snprintf(buf + len, 128, "Matrix for %s:\n", name[ii]);
		for (i = 0; i < 3; ++i) {
			for (j = 0; j < 3; ++j)
				len += snprintf(buf + len, 16, "%d ",
					mapping[i][j]);
			buf[len++] = '\n';
		}
		buf[len++] = '\n';
		buf[len++] = '\0';
	}

	return len;
}

static ssize_t bhy_store_mapping_matrix(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int i;
	int handle;
	int matrix[9];
	int ret;
	int index;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11d %11d %11d %11d %11d %11d %11d %11d %11d %11d",
		&handle, &matrix[0], &matrix[1], &matrix[2], &matrix[3],
		&matrix[4], &matrix[5], &matrix[6], &matrix[7], &matrix[8]);
	if (ret != 10) {
		PERR("Invalid input for matrix");
		return -EINVAL;
	}

	for (i = 0; i < 9; ++i) {
		if (matrix[i] < -1 || matrix[i] > 1) {
			PERR("Invalid matrix data: %d", matrix[i]);
			return -EINVAL;
		}
	}

	switch (handle) {
	case BHY_SENSOR_HANDLE_ACCELEROMETER:
		index = PHYSICAL_SENSOR_INDEX_ACC;
		break;
	case BHY_SENSOR_HANDLE_MAGNETIC_FIELD_UNCALIBRATED:
		index = PHYSICAL_SENSOR_INDEX_MAG;
		break;
	case BHY_SENSOR_HANDLE_GYROSCOPE_UNCALIBRATED:
		index = PHYSICAL_SENSOR_INDEX_GYRO;
		break;
	default:
		PERR("Invalid sensor handle: %d", handle);
		return -EINVAL;
	}
	for (i = 0; i < 9; ++i)
		client_data->ps_context[index].mapping_matrix[i] = matrix[i];

	if (bhy_set_mapping_matrix(client_data, index) < 0)
		return -EIO;

	client_data->ps_context[index].use_mapping_matrix = BHY_TRUE;

	return count;
}

static ssize_t bhy_show_custom_version(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	int ret;
	ssize_t len = 0;
	u8 data[16];
	int custom_version;
	int year, month, day, hour, minute, second;
	__le16 reg_data;
	int rom_id, ram_id;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_ROM_VERSION_0,
		(u8 *)&reg_data, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read rom version failed");
		return ret;
	}
	rom_id = (u16)__le16_to_cpu(reg_data);
	len += snprintf(buf + len, 64, "Rom version: %d\n", rom_id);

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_RAM_VERSION_0,
		(u8 *)&reg_data, 2);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read ram version failed");
		return ret;
	}
	ram_id = (u16)__le16_to_cpu(reg_data);
	len += snprintf(buf + len, 64, "Ram version: %d\n", ram_id);

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
		BHY_PARAM_SYSTEM_CUSTOM_VERSION, data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read custom version failed");
		return ret;
	}

	reg_data = *(__le16 *)data;
	custom_version = (u16)__le16_to_cpu(reg_data);
	reg_data = *(__le16 *)(data + 2);
	year = (u16)__le16_to_cpu(reg_data);
	month = *(u8 *)(data + 4);
	day = *(u8 *)(data + 5);
	hour = *(u8 *)(data + 6);
	minute = *(u8 *)(data + 7);
	second = *(u8 *)(data + 8);

	len += snprintf(buf + len, 64, "Custom version: %d\n", custom_version);
	len += snprintf(buf + len, 64,
		"Build date: %04d-%02d-%02d %02d:%02d:%02d\n",
		year, month, day, hour, minute, second);

	return len;
}

static ssize_t bhy_store_req_fw(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	long req;
	ssize_t ret;

		printk("########bhy_store_req_fw \n");
	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	printk("########bhy_store_req_fw 1\n");

	ret = kstrtol(buf, 10, &req);
	if (ret < 0 || req != 1) {
		PERR("Invalid request");
		atomic_set(&client_data->reset_flag, RESET_FLAG_ERROR);
		return -EINVAL;
	}
	printk("########bhy_store_req_fw 2\n");

#if 0
	ret = bhy_request_firmware(client_data);
	if (ret < 0)
		return ret;
#else
	ret = -1;
	int retry_count = 0;
	/* retry again */
	while (ret < 0)
	{
	
		ret = bhy_request_firmware(client_data);
		retry_count++;
			printk("############ Send FW %d times,ret=%d ##########\n",retry_count,ret);
		if (retry_count > 3 && ret < 0)
		{
			return ret;
		}
	}

#endif

	printk("########bhy_store_req_fw 3\n");

	return count;
}

#ifdef BHY_DEBUG

static ssize_t bhy_show_reg_sel(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "reg=0X%02X, len=%d\n",
		client_data->reg_sel, client_data->reg_len);
}

static ssize_t bhy_store_reg_sel(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11X %11d",
		&client_data->reg_sel, &client_data->reg_len);
	if (ret != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}

	return count;
}

static ssize_t bhy_show_reg_val(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 reg_data[128], i;
	int pos;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	if (client_data->reg_len <= 0) {
		PERR("Invalid register length");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, client_data->reg_sel,
		reg_data, client_data->reg_len);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Reg op failed");
		return ret;
	}

	pos = 0;
	for (i = 0; i < client_data->reg_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';

	return pos;
}

static ssize_t bhy_store_reg_val(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 reg_data[32] = { 0, };
	int i, j, status, digit;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	if (client_data->reg_len <= 0) {
		PERR("Invalid register length");
		return -EINVAL;
	}

	status = 0;
	for (i = j = 0; i < count && j < client_data->reg_len; ++i) {
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		/*PDEBUG("digit is %d", digit);*/
		switch (status) {
		case 2:
			++j; /* Fall thru */
			if (j >= (int)sizeof(reg_data))
				break;
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->reg_len)
		j = client_data->reg_len;
	else if (j < client_data->reg_len) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	/*PDEBUG("Reg data read as");*/
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_reg(client_data, client_data->reg_sel,
		reg_data, client_data->reg_len);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Reg op failed");
		return ret;
	}

	return count;
}

static ssize_t bhy_show_param_sel(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "Page=%d, param=%d\n",
		client_data->page_sel, client_data->param_sel);
}

static ssize_t bhy_store_param_sel(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11d %11d",
		&client_data->page_sel, &client_data->param_sel);
	if (ret != 2) {
		PERR("Invalid argument");
		return -EINVAL;
	}

	return count;
}

static ssize_t bhy_show_param_val(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 data[16];
	int pos, i;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_parameter(client_data, client_data->page_sel,
		client_data->param_sel, data, sizeof(data));
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Read param failed");
		return ret;
	}

	pos = 0;
	for (i = 0; i < 16; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", data[i]);
		buf[pos++] = ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';

	return pos;
}

static ssize_t bhy_store_param_val(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 data[8];
	int i, j, status, digit;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	status = 0;
	for (i = j = 0; i < count && j < 8; ++i) {
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		switch (status) {
		case 2:
			++j; /* Fall thru */
			if (j >= (int)sizeof(data))
				break;
		case 0:
			data[j] = digit;
			status = 1;
			break;
		case 1:
			data[j] = data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j == 0) {
		PERR("Invalid argument");
		return -EINVAL;
	} else if (j > 8)
		j = 8;
	/* Alway write 8 bytes, the bytes is 0 if not provided*/
	for (i = j; i < 8; ++i)
		data[i] = 0;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, client_data->page_sel,
		client_data->param_sel, data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write param failed");
		return ret;
	}

	return count;
}

static ssize_t bhy_store_log_raw_data(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	long req;
	u8 param_data[8];
	struct frame_queue *q;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 10, &req);
	if (ret < 0) {
		PERR("Invalid request");
		return -EINVAL;
	}
	q = &client_data->data_queue;

	memset(param_data, 0, sizeof(param_data));
	if (req)
		param_data[0] = param_data[1] = param_data[2] = 1;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_VIRTUAL_BSX_ENABLE, param_data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write raw data cfg failed");
		return ret;
	}

	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_DATA_LOG_TYPE;
	q->frames[q->head].data[0] = BHY_DATA_LOG_TYPE_RAW;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);

	return count;
}

static ssize_t bhy_store_log_input_data_gesture(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	long req;
	u8 param_data[8];
	struct frame_queue *q;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 10, &req);
	if (ret < 0) {
		PERR("Invalid request");
		return -EINVAL;
	}
	q = &client_data->data_queue;

	memset(param_data, 0, sizeof param_data);
	if (req)
		param_data[3] = param_data[4] = param_data[5] = 1;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_VIRTUAL_BSX_ENABLE, param_data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write raw data cfg failed");
		return ret;
	}

	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_DATA_LOG_TYPE;
	q->frames[q->head].data[0] = BHY_DATA_LOG_TYPE_INPUT_GESTURE;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);

	return count;
}

static ssize_t bhy_store_log_input_data_tilt_ar(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	long req;
	u8 param_data[8];
	struct frame_queue *q;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 10, &req);
	if (ret < 0) {
		PERR("Invalid request");
		return -EINVAL;
	}
	q = &client_data->data_queue;

	memset(param_data, 0, sizeof param_data);
	if (req)
		param_data[6] = param_data[7] = 1;

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_write_parameter(client_data, BHY_PAGE_ALGORITHM,
			BHY_PARAM_VIRTUAL_BSX_ENABLE, param_data, 8);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Write raw data cfg failed");
		return ret;
	}

	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_DATA_LOG_TYPE;
	q->frames[q->head].data[0] = BHY_DATA_LOG_TYPE_INPUT_TILT_AR;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);

	return count;
}

static ssize_t bhy_store_log_fusion_data(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	long req;
	struct frame_queue *q;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtol(buf, 10, &req);
	if (ret < 0) {
		PERR("Invalid request");
		return -EINVAL;
	}
	q = &client_data->data_queue;

	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_LOG_FUSION_DATA;
	q->frames[q->head].data[0] = req ? BHY_FUSION_DATA_LOG_ENABLE :
		BHY_FUSION_DATA_LOG_NONE;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);

	return count;
}

static ssize_t bhy_store_enable_pass_thru(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 u8_val;
	int enable;
	int retry;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtoint(buf, 10, &enable);
	if (ret < 0) {
		PERR("invalid input");
		return ret;
	}

	mutex_lock(&client_data->mutex_bus_op);

	if (enable) {
		/* Make algorithm standby */
		ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &u8_val, 1);
		if (ret < 0) {
			PERR("Read algorithm standby reg failed");
			goto _exit;
		}
		u8_val |= HOST_CTRL_MASK_ALGORITHM_STANDBY;
		ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &u8_val, 1);
		if (ret < 0) {
			PERR("Write algorithm standby reg failed");
			goto _exit;
		}
		retry = 10;
		do {
			ret = bhy_read_reg(client_data, BHY_REG_HOST_STATUS,
					&u8_val, 1);
			if (ret < 0) {
				PERR("Read host status again failed");
				goto _exit;
			}
			if (u8_val & BHY_HOST_STATUS_MASK_ALGO_STANDBY)
				break;
			msleep(1000);
		} while (--retry);
		if (retry == 0) {
			ret = -EIO;
			PERR("Algo standby does not take effect");
			goto _exit;
		}

		/* Enable pass thru mode */
		u8_val = 1;
		ret = bhy_write_reg(client_data, BHY_REG_PASS_THRU_CFG,
				&u8_val, 1);
		if (ret < 0) {
			PERR("Write pass thru cfg reg failed");
			goto _exit;
		}
		retry = 1000;
		do {
			ret = bhy_read_reg(client_data, BHY_REG_PASS_THRU_READY,
					&u8_val, 1);
			if (ret < 0) {
				PERR("Read pass thru ready reg failed");
				goto _exit;
			}
			if (u8_val & 1)
				break;
			usleep_range(1000, 1000);
		} while (--retry);
		if (retry == 0) {
			ret = -EIO;
			PERR("Pass thru does not take effect");
			goto _exit;
		}
	} else {
		/* Disable pass thru mode */
		u8_val = 0;
		ret = bhy_write_reg(client_data, BHY_REG_PASS_THRU_CFG,
				&u8_val, 1);
		if (ret < 0) {
			PERR("Write pass thru cfg reg failed");
			goto _exit;
		}
		retry = 1000;
		do {
			ret = bhy_read_reg(client_data, BHY_REG_PASS_THRU_READY,
					&u8_val, 1);
			if (ret < 0) {
				PERR("Read pass thru ready reg failed");
				goto _exit;
			}
			if (!(u8_val & 1))
				break;
			usleep_range(1000, 1000);
		} while (--retry);
		if (retry == 0) {
			ret = -EIO;
			PERR("Pass thru disable does not take effect");
			goto _exit;
		}

		/* Make algorithm standby */
		ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &u8_val, 1);
		if (ret < 0) {
			PERR("Read algorithm standby reg failed");
			goto _exit;
		}
		u8_val &= ~HOST_CTRL_MASK_ALGORITHM_STANDBY;
		ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL,
				&u8_val, 1);
		if (ret < 0) {
			PERR("Write algorithm standby reg failed");
			goto _exit;
		}
		retry = 10;
		do {
			ret = bhy_read_reg(client_data, BHY_REG_HOST_STATUS,
					&u8_val, 1);
			if (ret < 0) {
				PERR("Read host status again failed");
				goto _exit;
			}
			if (!(u8_val & BHY_HOST_STATUS_MASK_ALGO_STANDBY))
				break;
			msleep(1000);
		} while (--retry);
		if (retry == 0) {
			ret = -EIO;
			PERR("Pass thru enable does not take effect");
			goto _exit;
		}
	}

	ret = count;

_exit:

	mutex_unlock(&client_data->mutex_bus_op);
	return ret;
}

static ssize_t bhy_store_enable_irq_log(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	int enable;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtoint(buf, 10, &enable);
	if (ret < 0) {
		PERR("invalid input");
		return ret;
	}
	client_data->enable_irq_log = enable;

	return count;
}

static ssize_t bhy_store_enable_fifo_log(struct device *dev
		, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	int enable;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = kstrtoint(buf, 10, &enable);
	if (ret < 0) {
		PERR("invalid input");
		return ret;
	}
	client_data->enable_fifo_log = enable;

	return count;
}

static ssize_t bhy_show_hw_reg_sel(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "slave_addr=0X%02X, reg=0X%02X, len=%d\n",
		client_data->hw_slave_addr, client_data->hw_reg_sel,
		client_data->hw_reg_len);
}

static ssize_t bhy_store_hw_reg_sel(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}
	ret = sscanf(buf, "%11X %11X %11d", &client_data->hw_slave_addr,
		&client_data->hw_reg_sel, &client_data->hw_reg_len);
	if (ret != 3) {
		PERR("Invalid argument");
		return -EINVAL;
	}

	return count;
}

static ssize_t bhy_show_hw_reg_val(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 reg_data[128], i;
	int pos;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	if (client_data->hw_reg_len <= 0) {
		PERR("Invalid register length");
		return -EINVAL;
	}

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_soft_pass_thru_read_reg_m(client_data,
		client_data->hw_slave_addr, client_data->hw_reg_sel,
		reg_data, client_data->hw_reg_len);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Reg op failed");
		return ret;
	}

	pos = 0;
	for (i = 0; i < client_data->hw_reg_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';

	return pos;
}

static ssize_t bhy_store_hw_reg_val(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	u8 reg_data[32] = { 0, };
	int i, j, status, digit;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	if (client_data->hw_reg_len <= 0) {
		PERR("Invalid register length");
		return -EINVAL;
	}

	status = 0;
	for (i = j = 0; i < count && j < client_data->hw_reg_len; ++i) {
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' ||
			buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		PDEBUG("digit is %d", digit);
		switch (status) {
		case 2:
			++j; /* Fall thru */
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->hw_reg_len)
		j = client_data->hw_reg_len;
	else if (j < client_data->hw_reg_len) {
		PERR("Invalid argument");
		return -EINVAL;
	}
	PDEBUG("Reg data read as");
	for (i = 0; i < j; ++i)
		PDEBUG("%d", reg_data[i]);

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_soft_pass_thru_write_reg_m(client_data,
		client_data->hw_slave_addr, client_data->hw_reg_sel,
		reg_data, client_data->hw_reg_len);
	mutex_unlock(&client_data->mutex_bus_op);
	if (ret < 0) {
		PERR("Reg op failed");
		return ret;
	}

	return count;
}

static ssize_t bhy_show_sw_watchdog_enabled(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&client_data->mutex_sw_watchdog);
	if (client_data->sw_watchdog_disabled == BHY_TRUE)
		ret = snprintf(buf, 64, "disabled\n");
	else
		ret = snprintf(buf, 64, "enabled\n");
	mutex_unlock(&client_data->mutex_sw_watchdog);

	return ret;
}

static ssize_t bhy_store_sw_watchdog_enabled(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	int enable;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &enable);
	if (ret < 0) {
		PERR("invalid input");
		return ret;
	}

	mutex_lock(&client_data->mutex_sw_watchdog);
	if (enable) {
		client_data->sw_watchdog_disabled = BHY_FALSE;
		client_data->inactive_count = 0;
	} else
		client_data->sw_watchdog_disabled = BHY_TRUE;
	mutex_unlock(&client_data->mutex_sw_watchdog);

	return count;
}

static ssize_t bhy_store_trigger_sw_watchdog(struct device *dev
	, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	ssize_t ret;
	int req;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &req);
	if (ret < 0) {
		PERR("invalid input");
		return ret;
	}
	if (req != 1)
		return -EINVAL;

	PDEBUG("Manual triggered software watchdog!!!");
	client_data->recover_from_disaster = BHY_TRUE;
	bhy_request_firmware(client_data);

	return count;
}

#endif /*~ BHY_DEBUG */

static DEVICE_ATTR(rom_id, S_IRUGO,
	bhy_show_rom_id, NULL);
static DEVICE_ATTR(ram_id, S_IRUGO,
	bhy_show_ram_id, NULL);
static DEVICE_ATTR(status_bank, S_IRUGO,
	bhy_show_status_bank, NULL);
static DEVICE_ATTR(sensor_sel, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_sensor_sel);
static DEVICE_ATTR(sensor_info, S_IRUGO,
	bhy_show_sensor_info, NULL);
static DEVICE_ATTR(sensor_conf, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_sensor_conf, bhy_store_sensor_conf);
static DEVICE_ATTR(sensor_flush, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_sensor_flush);
static DEVICE_ATTR(calib_profile, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_calib_profile, bhy_store_calib_profile);
static DEVICE_ATTR(sic_matrix, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_sic_matrix, bhy_store_sic_matrix);
static DEVICE_ATTR(meta_event_ctrl, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_meta_event_ctrl, bhy_store_meta_event_ctrl);
static DEVICE_ATTR(fifo_ctrl, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_fifo_ctrl, bhy_store_fifo_ctrl);
#ifdef BHY_AR_HAL_SUPPORT
static DEVICE_ATTR(activate_ar_hal, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_activate_ar_hal);
#endif /*~ BHY_AR_HAL_SUPPORT */
static DEVICE_ATTR(reset_flag, S_IRUGO,
	bhy_show_reset_flag, NULL);
static DEVICE_ATTR(working_mode, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_working_mode, bhy_store_working_mode);
static DEVICE_ATTR(op_mode, S_IRUGO,
	bhy_show_op_mode, NULL);
static DEVICE_ATTR(bsx_version, S_IRUGO,
	bhy_show_bsx_version, NULL);
static DEVICE_ATTR(driver_version, S_IRUGO,
	bhy_show_driver_version, NULL);
#ifdef BHY_AR_HAL_SUPPORT
static DEVICE_ATTR(fifo_frame_ar, S_IRUGO,
	bhy_show_fifo_frame_ar, NULL);
#endif /*~ BHY_AR_HAL_SUPPORT */
static DEVICE_ATTR(bmi160_foc_offset_acc, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bmi160_foc_offset_acc, bhy_store_bmi160_foc_offset_acc);
static DEVICE_ATTR(bmi160_foc_offset_gyro,
	S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bmi160_foc_offset_gyro, bhy_store_bmi160_foc_offset_gyro);
static DEVICE_ATTR(bmi160_foc_conf, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bmi160_foc_conf, bhy_store_bmi160_foc_conf);
static DEVICE_ATTR(bmi160_foc_exec, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bmi160_foc_exec, bhy_store_bmi160_foc_exec);
static DEVICE_ATTR(bmi160_foc_save_to_nvm,
	S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bmi160_foc_save_to_nvm, bhy_store_bmi160_foc_save_to_nvm);
static DEVICE_ATTR(bma2x2_foc_offset,
	S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bma2x2_foc_offset, bhy_store_bma2x2_foc_offset);
static DEVICE_ATTR(bma2x2_foc_conf, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bma2x2_foc_conf, bhy_store_bma2x2_foc_conf);
static DEVICE_ATTR(bma2x2_foc_exec, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_bma2x2_foc_exec, bhy_store_bma2x2_foc_exec);
static DEVICE_ATTR(self_test, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_self_test, bhy_store_self_test);
static DEVICE_ATTR(self_test_result, S_IRUGO,
	bhy_show_self_test_result, NULL);
static DEVICE_ATTR(update_device_info, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_update_device_info);
static DEVICE_ATTR(mapping_matrix_acc, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_mapping_matrix_acc, bhy_store_mapping_matrix_acc);
static DEVICE_ATTR(sensor_data_size, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_sensor_data_size);
static DEVICE_ATTR(mapping_matrix, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_mapping_matrix, bhy_store_mapping_matrix);
static DEVICE_ATTR(custom_version, S_IRUGO,
	bhy_show_custom_version, NULL);
static DEVICE_ATTR(req_fw, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_req_fw);
#ifdef BHY_DEBUG
static DEVICE_ATTR(reg_sel, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_reg_sel, bhy_store_reg_sel);
static DEVICE_ATTR(reg_val, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_reg_val, bhy_store_reg_val);
static DEVICE_ATTR(param_sel, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_param_sel, bhy_store_param_sel);
static DEVICE_ATTR(param_val, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_param_val, bhy_store_param_val);
static DEVICE_ATTR(log_raw_data, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_log_raw_data);
static DEVICE_ATTR(log_input_data_gesture, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_log_input_data_gesture);
static DEVICE_ATTR(log_input_data_tilt_ar, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_log_input_data_tilt_ar);
static DEVICE_ATTR(log_fusion_data, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_log_fusion_data);
static DEVICE_ATTR(enable_pass_thru, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_enable_pass_thru);
static DEVICE_ATTR(enable_irq_log, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_enable_irq_log);
static DEVICE_ATTR(enable_fifo_log, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_enable_fifo_log);
static DEVICE_ATTR(hw_reg_sel, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_hw_reg_sel, bhy_store_hw_reg_sel);
static DEVICE_ATTR(hw_reg_val, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_hw_reg_val, bhy_store_hw_reg_val);
static DEVICE_ATTR(sw_watchdog_enabled, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
	bhy_show_sw_watchdog_enabled, bhy_store_sw_watchdog_enabled);
static DEVICE_ATTR(trigger_sw_watchdog, S_IWUSR | S_IWGRP | S_IWOTH,
	NULL, bhy_store_trigger_sw_watchdog);
#endif /*~ BHY_DEBUG */

static struct attribute *input_attributes[] = {
	&dev_attr_rom_id.attr,
	&dev_attr_ram_id.attr,
	&dev_attr_status_bank.attr,
	&dev_attr_sensor_sel.attr,
	&dev_attr_sensor_info.attr,
	&dev_attr_sensor_conf.attr,
	&dev_attr_sensor_flush.attr,
	&dev_attr_calib_profile.attr,
	&dev_attr_sic_matrix.attr,
	&dev_attr_meta_event_ctrl.attr,
	&dev_attr_fifo_ctrl.attr,
#ifdef BHY_AR_HAL_SUPPORT
	&dev_attr_activate_ar_hal.attr,
#endif /*~ BHY_AR_HAL_SUPPORT */
	&dev_attr_reset_flag.attr,
	&dev_attr_working_mode.attr,
	&dev_attr_op_mode.attr,
	&dev_attr_bsx_version.attr,
	&dev_attr_driver_version.attr,
	&dev_attr_bmi160_foc_offset_acc.attr,
	&dev_attr_bmi160_foc_offset_gyro.attr,
	&dev_attr_bmi160_foc_conf.attr,
	&dev_attr_bmi160_foc_exec.attr,
	&dev_attr_bmi160_foc_save_to_nvm.attr,
	&dev_attr_bma2x2_foc_offset.attr,
	&dev_attr_bma2x2_foc_conf.attr,
	&dev_attr_bma2x2_foc_exec.attr,
	&dev_attr_self_test.attr,
	&dev_attr_self_test_result.attr,
	&dev_attr_update_device_info.attr,
	&dev_attr_mapping_matrix_acc.attr,
	&dev_attr_sensor_data_size.attr,
	&dev_attr_mapping_matrix.attr,
	&dev_attr_custom_version.attr,
	&dev_attr_req_fw.attr,
#ifdef BHY_DEBUG
	&dev_attr_reg_sel.attr,
	&dev_attr_reg_val.attr,
	&dev_attr_param_sel.attr,
	&dev_attr_param_val.attr,
	&dev_attr_log_raw_data.attr,
	&dev_attr_log_input_data_gesture.attr,
	&dev_attr_log_input_data_tilt_ar.attr,
	&dev_attr_log_fusion_data.attr,
	&dev_attr_enable_pass_thru.attr,
	&dev_attr_enable_irq_log.attr,
	&dev_attr_enable_fifo_log.attr,
	&dev_attr_hw_reg_sel.attr,
	&dev_attr_hw_reg_val.attr,
	&dev_attr_sw_watchdog_enabled.attr,
	&dev_attr_trigger_sw_watchdog.attr,
#endif /*~ BHY_DEBUG */
	NULL
};

#ifdef BHY_AR_HAL_SUPPORT
static struct attribute *input_ar_attributes[] = {
	&dev_attr_rom_id.attr,
	&dev_attr_status_bank.attr,
	&dev_attr_sensor_sel.attr,
	&dev_attr_sensor_conf.attr,
	&dev_attr_sensor_flush.attr,
	&dev_attr_meta_event_ctrl.attr,
	&dev_attr_reset_flag.attr,
	&dev_attr_fifo_frame_ar.attr,
	NULL
};
#endif /*~ BHY_AR_HAL_SUPPORT */

static ssize_t bhy_show_fifo_frame(struct file *file
	, struct kobject *kobj, struct bin_attribute *attr,
	char *buffer, loff_t pos, size_t size)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct input_dev *input = to_input_dev(dev);
	struct bhy_client_data *client_data = input_get_drvdata(input);
	struct frame_queue *q = &client_data->data_queue;

	if (client_data == NULL) {
		PERR("Invalid client_data pointer");
		return -ENODEV;
	}

	mutex_lock(&q->lock);
	if (q->tail == q->head) {
		mutex_unlock(&q->lock);
		return 0;
	}
	memcpy(buffer, &q->frames[q->tail], sizeof(struct fifo_frame));
	if (q->tail == BHY_FRAME_SIZE - 1)
		q->tail = 0;
	else
		++q->tail;
	mutex_unlock(&q->lock);

	return sizeof(struct fifo_frame);
}

static struct bin_attribute bin_attr_fifo_frame = {
	.attr = {
		.name = "fifo_frame",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = bhy_show_fifo_frame,
	.write = NULL,
};

static ssize_t bhy_bst_show_rom_id(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct bst_dev *bst_dev = to_bst_dev(dev);
	struct bhy_client_data *client_data = bst_get_drvdata(bst_dev);
	ssize_t ret;

	ret = snprintf(buf, 32, "%d\n", client_data->rom_id);

	return ret;
}

static ssize_t bhy_bst_show_ram_id(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct bst_dev *bst_dev = to_bst_dev(dev);
	struct bhy_client_data *client_data = bst_get_drvdata(bst_dev);
	ssize_t ret;

	ret = snprintf(buf, 32, "%d\n", client_data->ram_id);

	return ret;
}

static ssize_t bhy_bst_show_dev_type(struct device *dev
	, struct device_attribute *attr, char *buf)
{
	struct bst_dev *bst_dev = to_bst_dev(dev);
	struct bhy_client_data *client_data = bst_get_drvdata(bst_dev);
	ssize_t ret;

	ret = snprintf(buf, 32, "%s\n", client_data->dev_type);

	return ret;
}

static DEVICE_ATTR(bhy_rom_id, S_IRUGO,
	bhy_bst_show_rom_id, NULL);
static DEVICE_ATTR(bhy_ram_id, S_IRUGO,
	bhy_bst_show_ram_id, NULL);
static DEVICE_ATTR(bhy_dev_type, S_IRUGO,
	bhy_bst_show_dev_type, NULL);

static struct attribute *bst_attributes[] = {
	&dev_attr_bhy_rom_id.attr,
	&dev_attr_bhy_ram_id.attr,
	&dev_attr_bhy_dev_type.attr,
	NULL
};

static void bhy_sw_watchdog_work_func(struct work_struct *work)
{
	struct bhy_client_data *client_data = container_of(work
		, struct bhy_client_data, sw_watchdog_work);
	int in_suspend_copy;
	int i;
	struct bhy_sensor_context *ct;
	int sensor_on = BHY_FALSE;
	int continuous_sensor_on = BHY_FALSE;
	int ret;
	u8 data[16];
	__le16 le16_data;
	u16 byte_remain;
	u8 irq_status;

	in_suspend_copy = atomic_read(&client_data->in_suspend);
	mutex_lock(&client_data->mutex_sw_watchdog);
	if (client_data->sw_watchdog_disabled == BHY_TRUE) {
		mutex_unlock(&client_data->mutex_sw_watchdog);
		return;
	}
	ct = client_data->sensor_context;
	for (i = 1; i <= BHY_SENSOR_HANDLE_MAX; ++i) {
		if (in_suspend_copy && ct[i].is_wakeup == BHY_FALSE)
			continue;
		if (ct[i].sample_rate <= 0)
			continue;
		if (ct[i].type == SENSOR_TYPE_CONTINUOUS &&
			ct[i].report_latency == 0) {
			sensor_on = BHY_TRUE;
			continuous_sensor_on = BHY_TRUE;
			break;
		}
		if (sensor_on == BHY_FALSE)
			sensor_on = BHY_TRUE;
	}
	if (sensor_on == BHY_TRUE)
		++client_data->inactive_count;
	/*PDEBUG("inactive_count is %d", client_data->inactive_count);*/
	if (client_data->inactive_count < BHY_SW_WATCHDOG_EXPIRE_COUNT) {
		mutex_unlock(&client_data->mutex_sw_watchdog);
		return;
	}
	/* Reset inactive count */
	client_data->inactive_count = 0;
	mutex_unlock(&client_data->mutex_sw_watchdog);

	if (continuous_sensor_on == BHY_TRUE) {
		mutex_lock(&client_data->mutex_bus_op);
		ret = bhy_read_reg(client_data, BHY_REG_BYTES_REMAIN_0,
			(u8 *)&le16_data, 2);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Fatal error: I2C communication failed");
			return;
		}
		byte_remain = le16_to_cpu(le16_data);
		ret = bhy_read_reg(client_data, BHY_REG_INT_STATUS,
			(u8 *)&irq_status, 1);
		if (ret < 0) {
			mutex_unlock(&client_data->mutex_bus_op);
			PERR("Fatal error: I2C communication failed");
			return;
		}
		mutex_unlock(&client_data->mutex_bus_op);
		if (byte_remain > 0 && irq_status != 0) {
			/* Retrigger data reading */
			schedule_work(&client_data->irq_work);
			return;
		}
	} else {
		mutex_lock(&client_data->mutex_bus_op);
		ret = bhy_read_parameter(client_data, BHY_PAGE_SYSTEM,
			BHY_PARAM_SYSTEM_HOST_IRQ_TIMESTAMP, data,
			sizeof(data));
		mutex_unlock(&client_data->mutex_bus_op);
		if (ret != -EBUSY) {
			PDEBUG("Check point triggered but passed");
			return;
		}
	}

	PDEBUG("Live lock detected!!!");
	client_data->recover_from_disaster = BHY_TRUE;
	bhy_request_firmware(client_data);
}

static enum hrtimer_restart bhy_sw_watchdog_timer_callback(
	struct hrtimer *timer)
{
	struct bhy_client_data *client_data = container_of(timer
		, struct bhy_client_data, sw_watchdog_timer);
	ktime_t now, interval;
	now = hrtimer_cb_get_time(timer);
	interval = ktime_set(0, BHY_SW_WATCHDOG_TIMER_INTERVAL);
	hrtimer_forward(timer, now, interval);
	schedule_work(&client_data->sw_watchdog_work);
	return HRTIMER_RESTART;
}

static void bhy_clear_up(struct bhy_client_data *client_data)
{
	if (client_data != NULL) {
		mutex_destroy(&client_data->mutex_bus_op);
		mutex_destroy(&client_data->data_queue.lock);
		mutex_destroy(&client_data->flush_queue.lock);
#ifdef BHY_AR_HAL_SUPPORT
		mutex_destroy(&client_data->data_queue_ar.lock);
#endif /*~ BHY_AR_HAL_SUPPORT */
		if (client_data->input_attribute_group != NULL) {
			sysfs_remove_group(&client_data->input->dev.kobj,
				client_data->input_attribute_group);
			kfree(client_data->input_attribute_group);
			client_data->input_attribute_group = NULL;
		}
		sysfs_remove_bin_file(&client_data->input->dev.kobj,
			&bin_attr_fifo_frame);
		if (client_data->input != NULL) {
			input_unregister_device(client_data->input);
			input_free_device(client_data->input);
			client_data->input = NULL;
		}
#ifdef BHY_AR_HAL_SUPPORT
		if (client_data->input_ar_attribute_group != NULL) {
			sysfs_remove_group(&client_data->input_ar->dev.kobj,
				client_data->input_ar_attribute_group);
			kfree(client_data->input_ar_attribute_group);
			client_data->input_ar_attribute_group = NULL;
		}
		if (client_data->input_ar != NULL) {
			input_unregister_device(client_data->input_ar);
			input_free_device(client_data->input_ar);
			client_data->input_ar = NULL;
		}
#endif /*~ BHY_AR_HAL_SUPPORT */
		if (client_data->bst_attribute_group != NULL) {
			sysfs_remove_group(&client_data->bst_dev->dev.kobj,
				client_data->bst_attribute_group);
			kfree(client_data->bst_attribute_group);
			client_data->bst_attribute_group = NULL;
		}
		if (client_data->bst_dev != NULL) {
			bst_unregister_device(client_data->bst_dev);
			bst_free_device(client_data->bst_dev);
			client_data->bst_dev = NULL;
		}
		if (client_data->data_bus.irq != -1)
			free_irq(client_data->data_bus.irq, client_data);
		if (client_data->fifo_buf != NULL) {
			kfree(client_data->fifo_buf);
			client_data->fifo_buf = NULL;
		}
		if (client_data->data_queue.frames != NULL) {
			kfree(client_data->data_queue.frames);
			client_data->data_queue.frames = NULL;
		}
#ifdef BHY_AR_HAL_SUPPORT
		if (client_data->data_queue_ar.frames != NULL) {
			kfree(client_data->data_queue_ar.frames);
			client_data->data_queue_ar.frames = NULL;
		}
#endif /*~ BHY_AR_HAL_SUPPORT */
		wake_lock_destroy(&client_data->wlock);
		hrtimer_cancel(&client_data->sw_watchdog_timer);
		mutex_destroy(&client_data->mutex_sw_watchdog);
		kfree(client_data);
	}
}

int bhy_probe(struct bhy_data_bus *data_bus)
{
	struct bhy_client_data *client_data = NULL;
	int ret;
	ktime_t ktime;
	int i;
	/*u8 reg_val;*/
   printk("BHy driver version: %s", DRIVER_VERSION);
	printk("############bhy_probe function entrance \n");

	/* check chip id */
	ret = bhy_check_chip_id(data_bus);
	printk("############bhy_probe function entrance 1\n");
	if (ret < 0) {
		PERR("Bosch Sensortec Device not found, chip id mismatch");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 2\n");
	PNOTICE("Bosch Sensortec Device %s detected", SENSOR_NAME);

	/* init client_data */
	client_data = kzalloc(sizeof(struct bhy_client_data), GFP_KERNEL);
	if (client_data == NULL) {
		PERR("no memory available for struct bhy_client_data");
		ret = -ENOMEM;
		goto err_exit;
	}
	printk("############bhy_probe function entrance 3\n");
	dev_set_drvdata(data_bus->dev, client_data);
	client_data->data_bus = *data_bus;
	mutex_init(&client_data->mutex_bus_op);
	mutex_init(&client_data->data_queue.lock);
	mutex_init(&client_data->flush_queue.lock);
#ifdef BHY_AR_HAL_SUPPORT
	mutex_init(&client_data->data_queue_ar.lock);
#endif /*~ BHY_AR_HAL_SUPPORT */
	printk("############bhy_probe function entrance 4\n");

	client_data->rom_id = 0;
	client_data->ram_id = 0;
	client_data->dev_type[0] = '\0';
	bhy_clear_flush_queue(&client_data->flush_queue);
	memset(client_data->self_test_result, -1, PHYSICAL_SENSOR_COUNT);
#ifdef BHY_TS_LOGGING_SUPPORT
	client_data->irq_count = 0;
#endif /*~ BHY_TS_LOGGING_SUPPORT */
	printk("############bhy_probe function entrance 5\n");
	ret = bhy_request_irq(client_data);
	printk("############bhy_probe function entrance 6\n");
	if (ret < 0) {
		PERR("Request IRQ failed");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 7\n");

	/* init input devices */
	ret = bhy_init_input_dev(client_data);
	if (ret < 0) {
		PERR("Init input dev failed");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 8\n");
	/* sysfs input node creation */
	client_data->input_attribute_group =
		kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
	if (client_data->input_attribute_group == NULL) {
		ret = -ENOMEM;
		PERR("No mem for input_attribute_group");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 9\n");
	client_data->input_attribute_group->attrs = input_attributes;
	ret = sysfs_create_group(&client_data->input->dev.kobj,
		client_data->input_attribute_group);
	if (ret < 0) {
		kfree(client_data->input_attribute_group);
		client_data->input_attribute_group = NULL;
		goto err_exit;
	}
	printk("############bhy_probe function entrance 10\n");

	ret = sysfs_create_bin_file(&client_data->input->dev.kobj,
		&bin_attr_fifo_frame);
	if (ret < 0) {
		sysfs_remove_bin_file(&client_data->input->dev.kobj,
			&bin_attr_fifo_frame);
		goto err_exit;
	}
	printk("############bhy_probe function entrance 11\n");

#ifdef BHY_AR_HAL_SUPPORT
	/* sysfs input node for AR creation */
	client_data->input_ar_attribute_group =
		kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
	if (client_data->input_ar_attribute_group == NULL) {
		ret = -ENOMEM;
		PERR("No mem for input_ar_attribute_group");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 12\n");
	client_data->input_ar_attribute_group->attrs = input_ar_attributes;
	ret = sysfs_create_group(&client_data->input_ar->dev.kobj,
		client_data->input_ar_attribute_group);
	if (ret < 0) {
		kfree(client_data->input_ar_attribute_group);
		client_data->input_ar_attribute_group = NULL;
		goto err_exit;
	}
	printk("############bhy_probe function entrance 13\n");
#endif /*~ BHY_AR_HAL_SUPPORT */

	/* bst device creation */
	client_data->bst_dev = bst_allocate_device();
	if (!client_data->bst_dev) {
		PERR("Allocate bst device failed");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 14\n");
	client_data->bst_dev->name = SENSOR_NAME;
	bst_set_drvdata(client_data->bst_dev, client_data);
	ret = bst_register_device(client_data->bst_dev);
	if (ret < 0) {
		bst_free_device(client_data->bst_dev);
		client_data->bst_dev = NULL;
		PERR("Register bst device failed");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 15\n");
	client_data->bst_attribute_group =
		kzalloc(sizeof(struct attribute_group), GFP_KERNEL);
	if (client_data->bst_attribute_group == NULL) {
		ret = -ENOMEM;
		PERR("No mem for bst_attribute_group");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 16\n");
	client_data->bst_attribute_group->attrs = bst_attributes;
	ret = sysfs_create_group(&client_data->bst_dev->dev.kobj,
		client_data->bst_attribute_group);
	if (ret < 0) {
		PERR("Create sysfs nodes for bst device failed");
		goto err_exit;
	}
	printk("############bhy_probe function entrance 17\n");
	client_data->fifo_buf = kmalloc(BHY_FIFO_LEN_MAX, GFP_KERNEL);
	if (!client_data->fifo_buf) {
		PERR("Allocate FIFO buffer failed");
		ret = -ENOMEM;
		goto err_exit;
	}
	printk("############bhy_probe function entrance 18\n");

	client_data->data_queue.frames = kmalloc(BHY_FRAME_SIZE *
			sizeof(struct fifo_frame), GFP_KERNEL);
	if (!client_data->data_queue.frames) {
		PERR("Allocate FIFO frame buffer failed");
		ret = -ENOMEM;
		goto err_exit;
	}
	client_data->data_queue.head = 0;
	client_data->data_queue.tail = 0;
#ifdef BHY_AR_HAL_SUPPORT
	client_data->data_queue_ar.frames = kmalloc(BHY_FRAME_SIZE_AR *
			sizeof(struct fifo_frame), GFP_KERNEL);
	if (!client_data->data_queue_ar.frames) {
		PERR("Allocate ar FIFO frame buffer failed");
		ret = -ENOMEM;
		goto err_exit;
	}
	client_data->data_queue_ar.head = 0;
	client_data->data_queue_ar.tail = 0;
#endif /*~ BHY_AR_HAL_SUPPORT */
	printk("############bhy_probe function entrance 19\n");
	printk("############bhy_probe function entrance 20\n");
	bhy_init_sensor_context(client_data);
	printk("############bhy_probe function entrance 21\n");
	wake_lock_init(&client_data->wlock, WAKE_LOCK_SUSPEND, "bhy");

	ktime = ktime_set(0, BHY_SW_WATCHDOG_TIMER_INTERVAL);
	hrtimer_init(&client_data->sw_watchdog_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	client_data->sw_watchdog_timer.function =
		&bhy_sw_watchdog_timer_callback;
	INIT_WORK(&client_data->sw_watchdog_work, bhy_sw_watchdog_work_func);
	hrtimer_start(&client_data->sw_watchdog_timer, ktime, HRTIMER_MODE_REL);
	mutex_init(&client_data->mutex_sw_watchdog);
	client_data->inactive_count = 0;
	client_data->recover_from_disaster = BHY_FALSE;
	client_data->sw_watchdog_disabled = BHY_FALSE;
	for (i = 0; i < PHYSICAL_SENSOR_COUNT; ++i) {
		client_data->ps_context[i].index = i;
		client_data->ps_context[i].use_mapping_matrix = BHY_FALSE;
	}
	for (i = 1; i <= BHY_META_EVENT_MAX; ++i) {
		client_data->me_context[i].index = i;
		client_data->me_context[i].event_en = BHY_STATUS_DEFAULT;
		client_data->me_context[i].irq_en = BHY_STATUS_DEFAULT;
		client_data->mew_context[i].index = i;
		client_data->mew_context[i].event_en = BHY_STATUS_DEFAULT;
		client_data->mew_context[i].irq_en = BHY_STATUS_DEFAULT;
	}

	atomic_set(&client_data->reset_flag, RESET_FLAG_TODO);
	/*reg_val = 0;
	ret = bhy_write_reg(client_data, BHY_REG_CHIP_CTRL, &reg_val, 1);
	if (ret < 0) {
		PERR("Set chip control failed");
		return ret;
	}
	atomic_set(&client_data->reset_flag, RESET_FLAG_READY);*/
		printk("############bhy_probe function entrance 22\n");

	PNOTICE("sensor %s probed successfully", SENSOR_NAME);
	return 0;

err_exit:
	bhy_clear_up(client_data);

	return ret;
}
EXPORT_SYMBOL(bhy_probe);

int bhy_remove(struct device *dev)
{
	struct bhy_client_data *client_data = dev_get_drvdata(dev);
	bhy_clear_up(client_data);
	return 0;
}
EXPORT_SYMBOL(bhy_remove);

#ifdef CONFIG_PM
int bhy_suspend(struct device *dev)
{
	struct bhy_client_data *client_data = dev_get_drvdata(dev);
	int ret;
	u8 data;
#ifdef BHY_TS_LOGGING_SUPPORT
	struct frame_queue *q = &client_data->data_queue;
#endif /*~ BHY_TS_LOGGING_SUPPORT */

	PINFO("Enter suspend");

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read host ctrl reg failed");
		return -EIO;
	}
	data |= HOST_CTRL_MASK_AP_SUSPENDED;
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host ctrl reg failed");
		return -EIO;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	enable_irq_wake(client_data->data_bus.irq);

	atomic_set(&client_data->in_suspend, 1);

#ifdef BHY_TS_LOGGING_SUPPORT
	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_AP_SLEEP_STATUS;
	q->frames[q->head].data[0] = BHY_AP_STATUS_SUSPEND;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);
#endif /*~ BHY_TS_LOGGING_SUPPORT */

	return 0;
}
EXPORT_SYMBOL(bhy_suspend);

int bhy_resume(struct device *dev)
{
	struct bhy_client_data *client_data = dev_get_drvdata(dev);
	int ret;
	u8 data;
#ifdef BHY_TS_LOGGING_SUPPORT
	struct frame_queue *q = &client_data->data_queue;
#endif /*~ BHY_TS_LOGGING_SUPPORT */

	PINFO("Enter resume");

	//disable_irq_wake(client_data->data_bus.irq);

	/* Wait for 50ms in case we cannot receive IRQ */
	//msleep(50);

	mutex_lock(&client_data->mutex_bus_op);
	ret = bhy_read_reg(client_data, BHY_REG_HOST_CTRL, &data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Read host ctrl reg failed");
		return -EIO;
	}
	data &= ~HOST_CTRL_MASK_AP_SUSPENDED;
	ret = bhy_write_reg(client_data, BHY_REG_HOST_CTRL, &data, 1);
	if (ret < 0) {
		mutex_unlock(&client_data->mutex_bus_op);
		PERR("Write host ctrl reg failed");
		return -EIO;
	}
	mutex_unlock(&client_data->mutex_bus_op);

	atomic_set(&client_data->in_suspend, 0);

	/* Flush after resume */
	ret = bhy_enqueue_flush(client_data, BHY_FLUSH_FLUSH_ALL);
	if (ret < 0) {
		PERR("Write sensor flush failed");
		return ret;
	}

#ifdef BHY_TS_LOGGING_SUPPORT
	client_data->irq_count = 0;
	mutex_lock(&q->lock);
	q->frames[q->head].handle = BHY_SENSOR_HANDLE_AP_SLEEP_STATUS;
	q->frames[q->head].data[0] = BHY_AP_STATUS_RESUME;
	if (q->head == BHY_FRAME_SIZE - 1)
		q->head = 0;
	else
		++q->head;
	if (q->head == q->tail) {
		PDEBUG("One frame data lost!!!");
		if (q->tail == BHY_FRAME_SIZE - 1)
			q->tail = 0;
		else
			++q->tail;
	}
	mutex_unlock(&q->lock);

	input_event(client_data->input, EV_MSC, MSC_RAW, 0);
	input_sync(client_data->input);
#endif /*~ BHY_TS_LOGGING_SUPPORT */

	return 0;
}
EXPORT_SYMBOL(bhy_resume);
#endif /*~ CONFIG_PM */
