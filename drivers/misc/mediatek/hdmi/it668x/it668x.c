/*
 * MHL support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * MHL TX driver for IT668x
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <video/omapdss.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/reboot.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"
#include "mach/eint.h"
#include "mach/irqs.h"
#include <mach/mt_gpio.h>

MODULE_LICENSE("GPL");

#include "it668x.h"

#include "mhl_rcp.h"
#include "it668x_drv.h"
#include "mhl_bridge_drv.h"
/*  */
/* MHL Log */
/*  */

#define MHL_ERR_LOG_ON      0x01
#define MHL_DEF_LOG_ON       0x02
#define MHL_INT_LOG_ON        0x04
#define MHL_VERB_LOG_ON    0x08
#define MHL_INFO_LOG_ON     0x10
#define MHL_MSC_LOG_ON      0x20
#define MHL_HDCP_LOG_ON     0x40
#define MHL_HDMI_LOG_ON     0x80
#define MHL_MHL_LOG_ON      0x100
#define MHL_WARN_LOG_ON     0x200
#define MHL_MUTEX_LOG_ON    0x8000

#define MHL_LOG_ON_ALL 0x7fff

static u32 s_mhl_log_on = (MHL_ERR_LOG_ON | MHL_DEF_LOG_ON);

#define MHL_ERR_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_ERR_LOG_ON) {\
		pr_err("[it668x:ERR] %s, %d: ", __func__, __LINE__);\
		pr_err(fmt, ##arg);\
	} \
} while (0)

#define MHL_DBG_LOG(fmt, arg...)     pr_err(fmt, ##arg)

#define MHL_INT_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_INT_LOG_ON) {\
		pr_info("[it668x:INT] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_VERB_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_VERB_LOG_ON) {\
		pr_info("[it668x] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_INFO_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_INFO_LOG_ON) {\
		pr_info("[it668x] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_MSC_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_MSC_LOG_ON) {\
		pr_info("[it668x] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_HDCP_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_HDCP_LOG_ON) {\
		pr_info("[it668x:HDCP] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_HDMI_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_HDMI_LOG_ON) {\
		pr_info("[it668x:HDMI] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_MHL_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_MHL_LOG_ON) {\
		pr_info("[it668x:MHL] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_WARN_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_WARN_LOG_ON) {\
		pr_info("[it668x:Warn] %s, %d: ", __func__, __LINE__);\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_MUTEX_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_MUTEX_LOG_ON) {\
		pr_info("[it668x:MUTEX]");\
		pr_info(fmt, ##arg);\
	} \
} while (0)

#define MHL_DEF_LOG(fmt, arg...) \
do {\
	if (s_mhl_log_on & MHL_DEF_LOG_ON) {\
		pr_err(fmt, ##arg);\
	} \
} while (0)

static int it668x_auto_set_vid_fmt(struct it668x_data *it668x);
static int it668x_hdmitx_video_state(struct it668x_data *it668x, enum hdmi_video_state state);
static int mhl_wite_int_sts(struct it668x_data *it668x, int offset, int field);
static void mhl_read_devcap(struct it668x_data *it668x);
static void it668x_ddc_cmd_done(struct it668x_data *it668x);
static void it668x_cbus_cmd_done(struct it668x_data *it668x);
static int mhl_queue_devcap_read_locked(struct it668x_data *it668x, unsigned char offset);
static int mhl_read_edid_block(struct it668x_data *it668x, unsigned char block, void *buffer);
static int mhl_read_edid(struct it668x_data *it668x);
static int it668x_hdmirx_hpd_low(struct it668x_data *it668x);
static int it668x_hdmirx_hpd_high(struct it668x_data *it668x);
static int mhl_cbus_Write_state_int_lock(struct it668x_data *it668x, unsigned char sub_code,
					 unsigned char sub_data);
static bool is_key_supported(struct it668x_data *it668x, int keyindex);

static int it668x_hdcp_read(struct it668x_data *it668x, int mode, unsigned char offset, int len,
			    char *p);
static void mhl_read_msc_msg(struct it668x_data *it668x);

static int it668x_ddcfire_cmd(struct it668x_data *it668x, int cmd);
static int it668x_mhl_config_path(struct it668x_data *it668x, unsigned int ddc_bypass);
static int it668x_config_ddc_bypass(struct it668x_data *it668x, unsigned int ddc_bypass);
static int it668x_fire_afe(struct it668x_data *it668x, int on);
static void mhl_dump_mem(void *p, int len);
/* extern void it668x_debgio_en(bool enable); */

static int it668x_initial_chip(struct it668x_data *it668x);
static int it668x_cbus_disconnect(struct it668x_data *it668x);
static int it668x_read_edid(void *it668x_data, void *pedid, unsigned short max_length);
static void it668x_turn_on_vbus(struct it668x_data *it668x, bool enable);
static void it668x_switch_to_usb(struct it668x_data *it668x, bool keep_detection);
static void it668x_switch_to_mhl(struct it668x_data *it668x);


struct it668x_data *it668x_factory = NULL;
struct task_struct *it668x_timer_task = NULL;

static struct task_struct *it668x_irq_task;
wait_queue_head_t it668x_irq_wq;	/* NFI, LVDS, HDMI */
atomic_t it668x_irq_event = ATOMIC_INIT(0);

static int i2c_write_reg(struct i2c_client *client, unsigned int offset, u8 value)
{
	return i2c_smbus_write_byte_data(client, offset, value);
}


static int i2c_read_reg(struct i2c_client *client, unsigned int offset, u8 *value)
{
	int ret;

	if (!value)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(client, offset);

	if (ret < 0)
		return ret;

	*value = ret & 0x000000FF;

	return 0;
}


static int i2c_set_reg(struct i2c_client *client, unsigned int offset, u8 mask, u8 value)
{
	u8 temp;
	int ret;

	ret = i2c_read_reg(client, offset, &temp);
	if (ret < 0)
		return ret;

	value = ((temp & (~mask)) | (mask & value));

	return i2c_write_reg(client, offset, value);
}

void __it668x_enable_irq(int caller)
{
	MHL_INT_LOG("enable IRQ, by %d\n", caller);
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
}

void __it668x_disable_irq(int caller)
{
	MHL_INT_LOG("disable IRQ, by %d\n", caller);
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
}

#define IT668X_DISABLE_IRQ()   __it668x_disable_irq(__LINE__)
#define IT668X_ENABLE_IRQ()    __it668x_enable_irq(__LINE__)

void __it668x_mutex_lock(struct mutex *lock, char *name, int caller)
{
	MHL_MUTEX_LOG("%s(%p) try lock by caller=%d\n", name, lock, caller);
	mutex_lock(lock);
	MHL_MUTEX_LOG("%s(%p) try lock by caller=%d, locked\n", name, lock, caller);
}

void __it668x_mutex_unlock(struct mutex *lock, char *name, int caller)
{
	MHL_MUTEX_LOG("%s(%p) unlock by caller=%d\n", name, lock, caller);
	mutex_unlock(lock);
	MHL_MUTEX_LOG("%s(%p) unlock by caller=%d, unlocked\n", name, lock, caller);
}

#define IT668X_MUTEX_LOCK(lock)   __it668x_mutex_lock(lock, #lock, __LINE__)
#define IT668X_MUTEX_UNLOCK(lock)    __it668x_mutex_unlock(lock, #lock, __LINE__)

void queue_msc_work(struct it668x_data *it668x)
{
	queue_work(it668x->cbus_cmd_wqs, &it668x->msc_work);
}

void it668x_v2hfifo_reset(struct it668x_data *it668x)
{
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	i2c_set_reg(mhl, REG_MHL_CONTROL, 0x40, 0x40);
	delay1ms(5);
	i2c_set_reg(mhl, REG_MHL_CONTROL, 0x40, 0x00);
}

static void it668x_hw_reset(void)
{
	/*  */
	/* todo: toggle system reset pin with GPIO */
	/* 1. SYSRSTN = low */
	/* 2. delay 30 ms ( depends on your R/C circuit ) */
	/* 3. SYSRSTN = high */
	/*  */
#ifdef GPIO_HDMI_9024_RESET
	MHL_INFO_LOG("hw reset\n");
	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ZERO);
	msleep(30);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ONE);
	msleep(20);
#endif
}

void it668x_dump_register(struct it668x_data *it668x)
{
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;
	int i, j;
	int ij;
	unsigned char uc_data = 0;
	char str[128] = { 0 };
	char str0[128] = { 0 };

	MHL_DBG_LOG("############################################################\n");
	MHL_DBG_LOG("# Dump IT6681\n");
	MHL_DBG_LOG("############################################################\n");

	strcpy(str, ".      ");
	for (j = 0; j < 16; j++) {
		sprintf(str0, " %02X", j);
		strcat(str, str0);
		if ((j == 3) || (j == 7) || (j == 11))
			strcat(str, "  ");
	}

	MHL_DBG_LOG("%s\n", str);
	MHL_DBG_LOG("        HDMI TX Register\n");
	MHL_DBG_LOG("        -----------------------------------------------------\n");

	for (i = 0; i < 0x100; i += 16) {
		sprintf(str, "[%3X]  ", i);
		for (j = 0; j < 16; j++) {
			ij = (i + j) & 0xFF;
			if (ij != 0x17) {
				if (0 == i2c_read_reg(hdmitx, ij, &uc_data)) {
					sprintf(str0, " %02X", uc_data);
					strcat(str, str0);
				} else {
					strcat(str, " ??");
				}
			} else {
				strcat(str, " XX");
			}
			if ((j == 3) || (j == 7) || (j == 11))
				strcat(str, " -");
		}

		MHL_DBG_LOG("%s\n", str);
		if ((i % 0x40) == 0x30) {
			MHL_DBG_LOG
			    ("        -----------------------------------------------------\n");
		}
	}

	MHL_DBG_LOG("        MHL Register\n");
	MHL_DBG_LOG("        -----------------------------------------------------\n");

	for (i = 0; i < 0x100; i += 16) {
		sprintf(str, "[%3X]  ", i);
		for (j = 0; j < 16; j++) {
			ij = (i + j) & 0xFF;

			if (ij == 0x17 || ij == 0x59 || ij == 0x5B) {
				strcat(str, " XX");
			} else {
				if (0 ==
				    i2c_read_reg(mhl, (unsigned char)((i + j) & 0xFF), &uc_data)) {
					sprintf(str0, " %02X", uc_data);
					strcat(str, str0);
				} else {
					strcat(str, " ??");
				}
			}

			if ((j == 3) || (j == 7) || (j == 11))
				strcat(str, " -");
		}

		MHL_DBG_LOG("%s\n", str);
		if ((i % 0x40) == 0x30) {
			MHL_DBG_LOG
			    ("        -----------------------------------------------------\n");
		}
	}

	MHL_DBG_LOG("        RX Register\n");
	MHL_DBG_LOG("        -----------------------------------------------------\n");

	for (i = 0; i < 0x40; i += 16) {
		sprintf(str, "[%3X]  ", i);
		for (j = 0; j < 16; j++) {
			if (0 == i2c_read_reg(hdmirx, (unsigned char)((i + j) & 0xFF), &uc_data)) {
				sprintf(str0, " %02X", uc_data);
				strcat(str, str0);
			} else {
				strcat(str, " ??");
			}
			if ((j == 3) || (j == 7) || (j == 11))
				strcat(str, " -");
		}

		MHL_DBG_LOG("%s\n", str);
		if ((i % 0x40) == 0x30) {
			MHL_DBG_LOG
			    ("        -------------------------------------------------------\n");
		}
	}
}

static const unsigned char csc_matrix_rgb2yuv_601_limited[] = {
	0x00, 0x80, 0x10,
	0xB2, 0x04, 0x65, 0x02, 0xE9, 0x00,
	0x93, 0x3C, 0x18, 0x04, 0x55, 0x3F,
	0x49, 0x3D, 0x9F, 0x3E, 0x18, 0x04
};

static const unsigned char csc_matrix_rgb2yuv_601_full[] = {
	0x10, 0x80, 0x10,
	0x09, 0x04, 0x0E, 0x02, 0xC9, 0x00,
	0x0F, 0x3D, 0x84, 0x03, 0x6D, 0x3F,
	0xAB, 0x3D, 0xD1, 0x3E, 0x84, 0x03
};

static const unsigned char csc_matrix_rgb2yuv_709_limited[] = {
	0x00, 0x80, 0x10,
	0xB8, 0x05, 0xB4, 0x01, 0x94, 0x00,
	0x4A, 0x3C, 0x17, 0x04, 0x9F, 0x3F,
	0xD9, 0x3C, 0x10, 0x3F, 0x17, 0x04
};

static const unsigned char csc_matrix_rgb2yuv_709_full[] = {
	0x10, 0x80, 0x10,
	0xEA, 0x04, 0x77, 0x01, 0x7F, 0x00,
	0xD0, 0x3C, 0x83, 0x03, 0xAD, 0x3F,
	0x4B, 0x3D, 0x32, 0x3F, 0x83, 0x03
};

static const unsigned char csc_matrix_yuv2rgb_601_limited[] = {
	0x00, 0x00, 0x00,
	0x00, 0x08, 0x6B, 0x3A, 0x50, 0x3D,
	0x00, 0x08, 0xF5, 0x0A, 0x02, 0x00,
	0x00, 0x08, 0xFD, 0x3F, 0xDA, 0x0D
};

static const unsigned char csc_matrix_yuv2rgb_601_full[] = {
	0x04, 0x00, 0xA7,
	0x4F, 0x09, 0x81, 0x39, 0xDD, 0x3C,
	0x4F, 0x09, 0xC4, 0x0C, 0x01, 0x00,
	0x4F, 0x09, 0xFD, 0x3F, 0x1F, 0x10
};

static const unsigned char csc_matrix_yuv2rgb_709_limited[] = {
	0x00, 0x00, 0x00,
	0x00, 0x08, 0x55, 0x3C, 0x88, 0x3E,
	0x00, 0x08, 0x51, 0x0C, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x84, 0x0E
};

static const unsigned char csc_matrix_yuv2rgb_709_full[] = {
	0x04, 0x00, 0xA7,
	0x4F, 0x09, 0xBA, 0x3B, 0x4B, 0x3E,
	0x4F, 0x09, 0x57, 0x0E, 0x02, 0x00,
	0x4F, 0x09, 0xFE, 0x3F, 0xE8, 0x10
};

static unsigned char const s_it668x_internal_edid[] = {
/*
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x06, 0x8f, 0x12, 0xb0, 0x01, 0x00, 0x00, 0x00,
    0x0c, 0x14, 0x01, 0x03, 0x80, 0x1c, 0x15, 0x78, 0x0a, 0x1e, 0xac, 0x98, 0x59, 0x56, 0x85, 0x28,
    0x29, 0x52, 0x57, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e,
    0x96, 0x00, 0xfa, 0xbe, 0x00, 0x00, 0x00, 0x18, 0xd5, 0x09, 0x80, 0xa0, 0x20, 0xe0, 0x2d, 0x10,
    0x10, 0x60, 0xa2, 0x00, 0xfa, 0xbe, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x56,
    0x41, 0x2d, 0x31, 0x38, 0x33, 0x31, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
    0x00, 0x17, 0x3d, 0x0d, 0x2e, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xfa,
    0x02, 0x03, 0x30, 0xf1, 0x43, 0x84, 0x10, 0x03, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00, 0x00,
    0xe2, 0x00, 0x0f, 0xe3, 0x05, 0x03, 0x01, 0x78, 0x03, 0x0c, 0x00, 0x11, 0x00, 0x88, 0x2d, 0x20,
    0xc0, 0x0e, 0x01, 0x00, 0x00, 0x12, 0x18, 0x20, 0x28, 0x20, 0x38, 0x20, 0x58, 0x20, 0x68, 0x20,
    0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28, 0x55, 0x00, 0xa0, 0x5a, 0x00, 0x00,
    0x00, 0x1e, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00, 0xa0, 0x5a,
    0x00, 0x00, 0x00, 0x18, 0xf3, 0x39, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c, 0x45, 0x00,
    0xe0, 0x0e, 0x11, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6a,
*/

/* EDID up to 720p only */
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x06, 0x8F, 0x12, 0xB0, 0x01, 0x00, 0x00, 0x00,
	0x0C, 0x14, 0x01, 0x03, 0x80, 0x1C, 0x15, 0x78,
	0x0A, 0x1E, 0xAC, 0x98, 0x59, 0x56, 0x85, 0x28,
	0x29, 0x52, 0x57, 0x20, 0x00, 0x00,
	0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01,

	0x1A, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0x40, 0xB4, 0x10,
	    0x00, 0x00, 0x1E,

	0xD5, 0x09, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10,
	0x10, 0x60, 0xA2, 0x00, 0xFA, 0xBE, 0x00, 0x00,
	0x00, 0x18,
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x56,
	0x41, 0x2D, 0x31, 0x38, 0x33, 0x31, 0x0A, 0x20,
	0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
	0x00, 0x17, 0x3D, 0x0D, 0x2E, 0x11, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xC6,

	0x02, 0x03, 0x1D, 0xF1,
	0x42, 0x84, 0x03,
	0x23, 0x09, 0x07, 0x07,
	0x83, 0x01, 0x00, 0x00,
	0xE2, 0x00, 0x0F,
	0xE3, 0x05, 0x03, 0x01,

	0x65, 0x03, 0x0c, 0x00, 0x10, 0x00,

	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xA0, 0x5A, 0x00,
	    0x00, 0x00, 0x1E,
	0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xA0, 0x5A, 0x00,
	    0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xF0
/* EDID up to 720p only (end) */
/* EDID up to 1080p no 3D */
/*
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x06, 0x8F, 0x12, 0xB0, 0x01, 0x00, 0x00, 0x00,
    0x0C, 0x14, 0x01, 0x03, 0x80, 0x1C, 0x15, 0x78, 0x0A, 0x1E, 0xAC, 0x98, 0x59, 0x56, 0x85, 0x28,
    0x29, 0x52, 0x57, 0x20, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E,
    0x96, 0x00, 0xFA, 0xBE, 0x00, 0x00, 0x00, 0x18, 0xD5, 0x09, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10,
    0x10, 0x60, 0xA2, 0x00, 0xFA, 0xBE, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x56,
    0x41, 0x2D, 0x31, 0x38, 0x33, 0x31, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
    0x00, 0x17, 0x3D, 0x0D, 0x2E, 0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xFA,

    0x02, 0x03, 0x1F, 0xF1, 0x43, 0x84, 0x10, 0x03, 0x23, 0x09, 0x07, 0x07, 0x83,
    0x01, 0x00, 0x00, 0xE2, 0x00, 0x0F, 0xE3, 0x05, 0x03, 0x01,
    0x67, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x88, 0x2D, ,
    0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x1E,
    0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xA0, 0x5A, 0x00, 0x00, 0x00, 0x18,
    0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0xE0, 0x0E, 0x11, 0x00, 0x00, 0x1E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, , 0x56
*/
/* EDID up to 1080p no 3D (end) */
};

#define rol(x, y) (((x) << (y)) | (((unsigned long)x) >> (32-y)))
static void hdmi_sha_transform(unsigned long *h, unsigned long *w)
{
	int t;
	unsigned long tmp;

	for (t = 16; t < 80; t++) {
		tmp = w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16];
		w[t] = rol(tmp, 1);
		/* pr_hdmi("w[%2d] = %08lX\n",t,w[t]) ; */
	}

	h[0] = 0x67452301;
	h[1] = 0xefcdab89;
	h[2] = 0x98badcfe;
	h[3] = 0x10325476;
	h[4] = 0xc3d2e1f0;

	for (t = 0; t < 20; t++) {
		tmp = rol(h[0], 5) + ((h[1] & h[2]) | (h[3] & ~h[1])) + h[4] + w[t] + 0x5a827999;
		/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */

		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;

	}
	for (t = 20; t < 40; t++) {
		tmp = rol(h[0], 5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0x6ed9eba1;
		/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	}
	for (t = 40; t < 60; t++) {
		tmp =
		    rol(h[0],
			5) + ((h[1] & h[2]) | (h[1] & h[3]) | (h[2] & h[3])) + h[4] + w[t] +
		    0x8f1bbcdc;
		/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	}
	for (t = 60; t < 80; t++) {
		tmp = rol(h[0], 5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0xca62c1d6;
		/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1], 30);
		h[1] = h[0];
		h[0] = tmp;
	}
	/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */

	h[0] += 0x67452301;
	h[1] += 0xefcdab89;
	h[2] += 0x98badcfe;
	h[3] += 0x10325476;
	h[4] += 0xc3d2e1f0;
	/* pr_hdmi("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ; */
}


static void hdmi_sha_simple(unsigned char *p, int msgcnt, unsigned char *output)
{
	int i, t;
	unsigned long c;
	unsigned char *pBuff = p;
	unsigned long w[80];
	unsigned long sha[5];

	for (i = 0; i < msgcnt; i++) {
		t = i / 4;
		if (i % 4 == 0)
			w[t] = 0;
		c = pBuff[i];
		c &= 0xFF;
		c <<= (3 - (i % 4)) * 8;
		w[t] |= c;
		/* pr_hdmi("pBuff[%d] = %02x, c = %08lX, w[%d] = %08lX\n",i,pBuff[i],c,t,w[t]) ; */
	}
	t = i / 4;
	if (i % 4 == 0)
		w[t] = 0;
	c = 0x80 << ((3 - i % 4) * 8);
	w[t] |= c;
	t++;
	for (; t < 15; t++)
		w[t] = 0;
	w[15] = msgcnt * 8;

	/* for( t = 0 ; t< 16 ; t++ ){ */
	/* printk("w[%2d] = %08lX\n",t,w[t]) ; */
	/* } */

	hdmi_sha_transform(sha, w);

	for (i = 0; i < 5; i++) {
		output[i * 4] = (unsigned char)(sha[i] & 0xFF);
		output[i * 4 + 1] = (unsigned char)((sha[i] >> 8) & 0xFF);
		output[i * 4 + 2] = (unsigned char)((sha[i] >> 16) & 0xFF);
		output[i * 4 + 3] = (unsigned char)((sha[i] >> 24) & 0xFF);
	}
}

static bool it668x_hdcp_repeater_check(struct it668x_data *it668x)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* int ret =0; */

	int i;
	int ksvcnt = 0;
	int msgcnt;

	unsigned char bstatus[2];
	unsigned char av[5][4], bv[5][4], ksvlist[32];
	unsigned char m[64], m0[8];


	/*  */
	/* read back V value and M value from reg0x51 to reg 0x55 */
	/* check the register define for detial */
	/*  */

	i2c_set_reg(hdmitx, REG_HDMITX_SHA_SEL, M_SHA_SEL, SEL_Mi);

	for (i = 0; i < 4; i++)
		i2c_read_reg(hdmitx, 0x51 + i, &m0[i]);

	for (i = 0; i < 4; i++) {
		i2c_set_reg(hdmitx, REG_HDMITX_SHA_SEL, M_SHA_SEL, (unsigned char)(i & 0x07));
		i2c_read_reg(hdmitx, 0x55, &m0[i + 4]);
	}

	MHL_HDCP_LOG(" M0: 0x ");

	for (i = 0; i < 8; i++)
		MHL_HDCP_LOG("%02X ", m0[7 - i]);

	/* MHL_HDCP_LOG("\n\n"); */

	/*  */
	/* read back HDCP offset BSTATUS (0x41) back to internal register */
	/*  */
	it668x_hdcp_read(it668x, HDPCD_REGISTER_READ, 0x41, 2, NULL);

	i2c_read_reg(hdmitx, REG_HDMITX_BSTATUS_L, &bstatus[0]);
	i2c_read_reg(hdmitx, REG_HDMITX_BSTATUS_H, &bstatus[1]);


	MHL_HDCP_LOG(" Device Count = %X\n", bstatus[0] & 0x7F);
	MHL_HDCP_LOG(" Max. Device Exceeded = %02X\n", (bstatus[0] & 0x80) >> 7);


	MHL_HDCP_LOG(" Depth = %X\n", bstatus[1] & 0x07);
	MHL_HDCP_LOG(" Max. Cascade Exceeded = %02X\n", (bstatus[1] & 0x08) >> 3);
	MHL_HDCP_LOG(" HDMI_MODE = %d\n", (bstatus[1] & 0x10) >> 4);
	MHL_HDCP_LOG("\n");

	if ((bstatus[0] & 0x80) || (bstatus[1] & 0x08)) {
		MHL_ERR_LOG(" ERROR: Max. Device or Cascade Exceeded !!!\n");
		return false;
	} else {
		/* read ksv list from ddc fifo */
		ksvcnt = 5 * (bstatus[0] & 0x7F);

		/*  */
		/* read back HDCP offset KSV LIST (0x43) back to internal register */
		/*  */

		if (ksvcnt)
			it668x_hdcp_read(it668x, HDPCD_FIFO_READ, 0x43, ksvcnt, ksvlist);
		else
			MHL_HDCP_LOG(" WARNING: Device Count = 0 !!!\n");

		/* i2c_set_reg(hdmitx, 0x10, 0x01, 0x01); */
		/* for(i = 0; i<ksvcnt; i++ ){ */
		/* i2c_read_reg(hdmitx, 0x17, &ksvlist[i]); */
		/* } */

		/* i2c_set_reg(hdmitx, 0x10, 0x01, 0x01); */

		msgcnt = 0;

		for (i = 0; i < (bstatus[0] & 0x7F); i++) {
			m[msgcnt++] = (unsigned char)ksvlist[i * 5 + 0];
			m[msgcnt++] = (unsigned char)ksvlist[i * 5 + 1];
			m[msgcnt++] = (unsigned char)ksvlist[i * 5 + 2];
			m[msgcnt++] = (unsigned char)ksvlist[i * 5 + 3];
			m[msgcnt++] = (unsigned char)ksvlist[i * 5 + 4];

			MHL_HDCP_LOG(" KSV List %d = 0x %02X %02X %02X %02X %02X\n", i,
				     m[i * 5 + 4], m[i * 5 + 3], m[i * 5 + 2], m[i * 5 + 1],
				     m[i * 5 + 0]);
		}

		MHL_HDCP_LOG("\n");

		m[msgcnt++] = bstatus[0];
		m[msgcnt++] = bstatus[1];

		m[msgcnt++] = m0[0];
		m[msgcnt++] = m0[1];
		m[msgcnt++] = m0[2];
		m[msgcnt++] = m0[3];
		m[msgcnt++] = m0[4];
		m[msgcnt++] = m0[5];
		m[msgcnt++] = m0[6];
		m[msgcnt++] = m0[7];

		MHL_HDCP_LOG(" SHA Message Count = %d\n\n", msgcnt);

		m[msgcnt] = 0x80;

		for (i = msgcnt + 1; i < 62; i++)
			m[i] = 0x00;

		m[62] = ((8 * msgcnt) >> 8) & 0xFF;
		m[63] = (8 * msgcnt) & 0xFF;

		/* ShowMsg(M); */

		hdmi_sha_simple(&m[0], msgcnt, &av[0][0]);

		for (i = 0; i < 5; i++)
			MHL_HDCP_LOG(" AV.H%d = 0x %02X %02X %02X %02X\n", i, av[i][3], av[i][2],
				     av[i][1], av[i][0]);

		MHL_HDCP_LOG("\n");


		for (i = 0; i < 5; i++) {
			it668x_hdcp_read(it668x, HDPCD_REGISTER_READ, 0x20 + (i * 4), 4, NULL);

			i2c_set_reg(hdmitx, REG_HDMITX_SHA_SEL, M_SHA_SEL,
				    (unsigned char)(i & 0x07));

			i2c_read_reg(hdmitx, 0x51, &bv[i][0]);
			i2c_read_reg(hdmitx, 0x52, &bv[i][1]);
			i2c_read_reg(hdmitx, 0x53, &bv[i][2]);
			i2c_read_reg(hdmitx, 0x54, &bv[i][3]);

			MHL_HDCP_LOG("BV.H%d = 0x %02X %02X %02X %02X\n", i, bv[i][3], bv[i][2],
				     bv[i][1], bv[i][0]);

			if (av[i][0] != bv[i][0] || av[i][1] != bv[i][1] ||
			    av[i][2] != bv[i][2] || av[i][3] != bv[i][3])
				return false;
		}

	}

	return true;
}

static int it668x_hdcp_read(struct it668x_data *it668x, int mode, unsigned char offset, int len,
			    char *p)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	int ret = 0;
	int i;

	IT668X_MUTEX_LOCK(&it668x->ddc_lock);
	/* pr_err(" mutex_lock(&it668x->ddc_lock);"); */

	if (mode == HDPCD_FIFO_READ)
		i2c_set_reg(hdmitx, REG_DCC_FIFO_ACCESS_MODE, B_EN_NEW_FIFO_ACCESS,
			    B_EN_NEW_FIFO_ACCESS);

	i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, B_DDC_PC_MASTER);

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, CMD_DDC_CLEAR_FIFO);

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_HEADER, DDC_HEADER_HDCP);

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_OFFSET, offset);

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_BYTE_NUMBER, (unsigned char)len);

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_SEGMENT, 0);

	/* i2c_set_reg(mhl, REG_MHL_INT_05_MASK09 ,B_MASK_DDC_CMD_DONE ,0x00); */
	/* init_completion(&it668x->ddc_complete); */
	/* i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, CMD_DDC_BURST_READ); */
	/* mutex_unlock(&it668x->lock); */
	/* ret = wait_for_completion_timeout(&it668x->ddc_complete,msecs_to_jiffies(2500)); */
	/* mutex_lock(&it668x->lock); */
	/* i2c_set_reg(mhl, REG_MHL_INT_05_MASK09 ,B_MASK_DDC_CMD_DONE ,B_MASK_DDC_CMD_DONE); */

	ret = it668x_ddcfire_cmd(it668x, CMD_DDC_BURST_READ);

	if (ret < 0) {
		MHL_ERR_LOG(" it668x_hdcp_read  DDC return fail ...\n");
		goto dpcdrd_error;
	}

	i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, B_DDC_HDCP_MASTER);

	if (mode == HDPCD_FIFO_READ) {

		for (i = 0; i < len; i++) {

			i2c_read_reg(hdmitx, REG_HDMITX_DDC_FIFO, p);
			p++;
		}
	}
dpcdrd_error:
	i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, B_DDC_HDCP_MASTER);

	if (mode == HDPCD_FIFO_READ)
		i2c_set_reg(hdmitx, REG_DCC_FIFO_ACCESS_MODE, B_EN_NEW_FIFO_ACCESS, 0x00);

	IT668X_MUTEX_UNLOCK(&it668x->ddc_lock);

	return ret;

}

static int it668x_count_bit(unsigned char d)
{

	int i, cnt;
	for (i = 0, cnt = 0; i < 8; i++) {

		if (d & 0x01)
			cnt++;
		d = d >> 1;
	}

	return cnt;
}

static int hdcp_ksv_bit_cnt(unsigned char *ksv)
{
	int cnt;

	cnt = it668x_count_bit(ksv[0]);
	cnt += it668x_count_bit(ksv[1]);
	cnt += it668x_count_bit(ksv[2]);
	cnt += it668x_count_bit(ksv[3]);
	cnt += it668x_count_bit(ksv[4]);

	MHL_HDCP_LOG("HDCP ksv bit cnt = %d\n", cnt);
	return cnt;
}

static int it668x_hdmitx_enhdcp_start(struct it668x_data *it668x)
{
	int sink_hdmi;
	int hdcp_wait;
	unsigned char bksv[5];
	unsigned char temp;
	unsigned char bstatus[2];

	int ret = 0;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_HDCP, B_REST_HDCP);
	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_EN_CPDESIRE, B_EN_CPDESIRE);
	delay1ms(1);

	i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_HDCP, 0x00);

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_ENA_SIPROM, B_ENA_SIPROM);
	i2c_set_reg(hdmitx, REG_HDMITX_SIPMISC, B_SIP_POWRDM, 0x00);
	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_ENA_SIPROM, 0x00);
	i2c_set_reg(hdmitx, REG_HDMITX_DIS_ENCRYTION, B_ENCRYPTION_DISABLE, 0x00);
	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_TEST, M_HDCP_4B_OPTION,
		    EN_RI_COMB_READ | EN_R0_COMB_READ);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, LOCK_PWD);


	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_EN_HDCP11_FEATURE | B_EN_HDCP12_SYNCDET,
		    0x00);

	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_OPTION, M_HDCP_4F_OPTION, B_EN_HDCP_AUTO_MUTE);

	i2c_write_reg(hdmitx, REG_HDMITX_INT_08, INT_HDCP_AUTH_DONE | INT_HDCP_AUTH_FAIL);

	i2c_set_reg(hdmitx, REG_HDMITX_INT_08_MASK,
		    B_MASK_HDCP_AUTH_FAIL | B_MASK_HDCP_AUTH_DONE | B_MASK_HDCP_KSVLIST_CHK, 0x00);

	i2c_set_reg(hdmitx, REG_HDMITX_INT_08_MASK, INT_HDCP_PJ_CHK | INT_HDCP_RI_CHK,
		    INT_HDCP_PJ_CHK | INT_HDCP_RI_CHK);

	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_AN_GEN, B_EN_AN_GEN, B_EN_AN_GEN);	/* Enable An Generator */
	delay1ms(1);
	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_AN_GEN, B_EN_AN_GEN, 0x00);	/* Stop An Generator */

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN00, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN00, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN01, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN01, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN02, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN02, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN03, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN03, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN04, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN04, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN05, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN05, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN06, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN06, temp);

	i2c_read_reg(hdmitx, REG_HDMITX_GENAN07, &temp);
	i2c_write_reg(hdmitx, REG_HDMITX_AN07, temp);

	/* fixAN */
/*
    i2c_write_reg(hdmitx, REG_HDMITX_AN00, 0x7E);
    i2c_write_reg(hdmitx, REG_HDMITX_AN01, 0x26);
    i2c_write_reg(hdmitx, REG_HDMITX_AN02, 0xDA);
    i2c_write_reg(hdmitx, REG_HDMITX_AN03, 0x32);
    i2c_write_reg(hdmitx, REG_HDMITX_AN04, 0xAA);
    i2c_write_reg(hdmitx, REG_HDMITX_AN05, 0x3B);
    i2c_write_reg(hdmitx, REG_HDMITX_AN06, 0x5E);
    i2c_write_reg(hdmitx, REG_HDMITX_AN07, 0x92);

*/
	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_EN_CPDESIRE, B_EN_CPDESIRE);

	for (hdcp_wait = 0; hdcp_wait < 15; hdcp_wait++) {	/* wait sink HDMI ready 30X50ms = 1.5s */
		delay1ms(100);

		if (it668x->tx_ver == 0) {

			it668x_hdcp_read(it668x, HDPCD_FIFO_READ, 0x41, 2, bstatus);
			MHL_HDCP_LOG(" tc_ver =0, bstatus=0x%2.2x%2.2x\n", (int)bstatus[1],
				     (int)bstatus[0]);
			sink_hdmi = (bstatus[1] & 0x10) >> 4;
		} else {

			it668x_hdcp_read(it668x, HDPCD_REGISTER_READ, 0x41, 2, bstatus);
			i2c_read_reg(hdmitx, REG_HDMITX_BSTATUS_L, &bstatus[0]);
			i2c_read_reg(hdmitx, REG_HDMITX_BSTATUS_H, &bstatus[1]);
			sink_hdmi = (bstatus[1] & 0x10) >> 4;

			MHL_HDCP_LOG(" read bstatus=0x%2.2x%2.2x\n", (int)bstatus[1],
				     (int)bstatus[0]);

		}

		MHL_HDCP_LOG(" sink_hdmi = %x\n", sink_hdmi);
		if (sink_hdmi == true)
			/* if(it668x->recive_hdmi_mode == sink_hdmi) */
			goto hdcp_start;

		if (it668x->video_state != HDMI_VIDEO_ON)
			break;
	}

	goto hdcp_ena_err_return;

hdcp_start:


	ret = it668x_hdcp_read(it668x, HDPCD_FIFO_READ, 0x00, 5, bksv);
	if (ret < 0)
		goto hdcp_ena_err_return;


	MHL_DEF_LOG("[it668x] BKSV = 0x%X%X%X%X%X%X%X%X%X%X\n", (int)(bksv[4] >> 4),
		    (int)(bksv[4] & 0x0F), (int)(bksv[3] >> 4), (int)(bksv[3] & 0x0F),
		    (int)(bksv[2] >> 4), (int)(bksv[2] & 0x0F), (int)(bksv[1] >> 4),
		    (int)(bksv[1] & 0x0F), (int)(bksv[0] >> 4), (int)(bksv[0] & 0x0F));

	if (hdcp_ksv_bit_cnt(bksv) > 20)
		goto hdcp_ena_err_return;


	if (bksv[0] == 0x23 && bksv[1] == 0xDE && bksv[2] == 0x5C && bksv[3] == 0x43
	    && bksv[4] == 0x93) {
		MHL_ERR_LOG(" The BKSV is in revocation list for ATC test !!!\n");
		MHL_ERR_LOG(" Abort HDCP Authentication !!!\n");
		goto hdcp_ena_err_return;
	}

	if (bksv[0] == 0x27 && bksv[1] == 0xF8 && bksv[2] == 0xA0 && bksv[3] == 0x6E
	    && bksv[4] == 0x2D) {
		MHL_ERR_LOG(" The BKSV is in revocation list for ATC test !!!\n");
		MHL_ERR_LOG(" Abort HDCP Authentication !!!\n");
		goto hdcp_ena_err_return;
	}
	/* if( bksv[0]==0x00 && bksv[1]==0x00 && bksv[2]==0x00 && bksv[3]==0x00 && bksv[4]==0x00 ) { */
	/* MHL_ERR_LOG(" The BKSV is in revocation list for ATC test !!!\n"); */
	/* MHL_ERR_LOG(" Abort HDCP Authentication !!!\n"); */
	/* goto hdcp_ena_err_return; */
	/* } */

	/* if( bksv[0]==0xFF && bksv[1]==0xFF && bksv[2]==0xFF && bksv[3]==0xFF && bksv[4]==0xFF ) { */
	/* MHL_ERR_LOG(" The BKSV is in revocation list for ATC test !!!\n"); */
	/* MHL_ERR_LOG(" Abort HDCP Authentication !!!\n"); */
	/* goto hdcp_ena_err_return; */
	/* } */



	i2c_write_reg(hdmitx, REG_HDMITX_INT_08, 0xFF);	/* clr all hdcp irq */

	i2c_set_reg(hdmitx, REG_HDMITX_HDCP_START, B_HDCP_AUTH_START, B_HDCP_AUTH_START);

	return 0;

hdcp_ena_err_return:
	return -1;

}

static int it668x_hdcp_state(struct it668x_data *it668x, enum hdcp_state state)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* increase counter */
	if (it668x->hdcp_enable == false)
		state = HDCP_OFF;

	MHL_DEF_LOG(" it668x_hdcp_state %2.2X ==> %2.2X\n", (int)it668x->hdcp_state, (int)state);

	switch (state) {
	case HDCP_OFF:
		break;
	case HDCP_CP_START:
		it668x->hdcp_fail_time_out++;
		break;
	case HDCP_CP_GOING:
		break;
	case HDCP_CP_DONE:
		break;
	case HDCP_CP_FAIL:
		break;
	default:
		break;
	}

	if (it668x->hdcp_state != state) {

		it668x->hdcp_state = state;
		switch (state) {

		case HDCP_OFF:
			MHL_DEF_LOG(" it668x->Hdcp_state -->HDCP_Off\n");
			i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_EN_CPDESIRE, 0x00);	/* Disable CP_Desired */
			i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_HDCP, B_REST_HDCP);

			i2c_set_reg(hdmitx, REG_HDMITX_INT_08_MASK, 0x3F, 0x3F);	/* disable hdcp Interrupt */

			i2c_write_reg(hdmitx, REG_HDMITX_INT_08, 0x3F);	/* clear previous hdcp irq */


			it668x->hdcp_fail_time_out = 0;
			break;

		case HDCP_CP_START:
			MHL_DEF_LOG(" it668x->Hdcp_state -->HDCP_CPStart\n");
			break;
		case HDCP_CP_GOING:
			MHL_DEF_LOG(" it668x->Hdcp_state -->HDCP_CPGoing\n");
			break;
		case HDCP_CP_DONE:
			/* clear AVMute */
			i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, 0x00);
			i2c_set_reg(hdmitx, REG_HDMITX_VIDEO_BLACK, B_EN_VIDEO_BLACK, 0);
			MHL_DEF_LOG(" it668x->Hdcp_state -->HDCP_CPDone\n");
			it668x->hdcp_fail_time_out = 0;
			break;
		case HDCP_CP_FAIL:
			MHL_DEF_LOG(" it668x->Hdcp_state -->HDCP_CPFail\n");
			/* Disable CPDesired */
			i2c_set_reg(hdmitx, REG_HDMITX_HDCP_CONFIG, B_EN_CPDESIRE, 0x00);
			/* Black Screen */
			i2c_set_reg(hdmitx, REG_HDMITX_VIDEO_BLACK, B_EN_VIDEO_BLACK, B_EN_VIDEO_BLACK);
			break;
		default:
			break;
		}
	}

	return ret;
}

static void it668x_hdmirx_cdrest(struct it668x_data *it668x)
{
	it668x->avi_cmd = AVI_CDREST;
	it668x->avi_work = true;
	queue_work(it668x->avi_cmd_wqs, &it668x->avi_control_work);
}

static void it668x_mhl_hpd_high_event(struct it668x_data *it668x)
{
	MHL_DEF_LOG(" it668x_mhl_hpd_high_event\n");
	/* it668x->hpd_status = true; */
	it668x->avi_cmd = HPD_HIGH_EVENT;
	it668x->avi_work = true;
	queue_work(it668x->avi_cmd_wqs, &it668x->avi_control_work);
}

static void it668x_mhl_avi_new_event(struct it668x_data *it668x)
{
	/* it668x->hpd_status = true; */
	it668x->avi_cmd = CEA_NEW_AVI;
	it668x->avi_work = true;
	queue_work(it668x->avi_cmd_wqs, &it668x->avi_control_work);
}

static void it668x_hdmitx_avi_hdcp_restart(struct it668x_data *it668x)
{
	it668x->avi_cmd = AVI_HDCP_RESTART;
	it668x->avi_work = true;
	queue_work(it668x->avi_cmd_wqs, &it668x->avi_control_work);
}

static void it668x_chip_reset(struct it668x_data *it668x)
{
	it668x->avi_cmd = CHIP_RESET;
	it668x->avi_work = true;
	queue_work(it668x->avi_cmd_wqs, &it668x->avi_control_work);
}

static void it668x_hdmitx_hdcp_start(struct it668x_data *it668x)
{

	it668x->hdcp_cmd = HDCP_START;
	it668x->hdcp_work = true;
	it668x->hdcp_abort = false;
	/* queue_work(it668x->hdcp_cmd_wqs,&it668x->hdcp_control_work); */
	queue_work(it668x->avi_cmd_wqs, &it668x->hdcp_control_work);

	it668x_hdcp_state(it668x, HDCP_CP_START);
}

static void it668x_hdmitx_hdcp_abort(struct it668x_data *it668x)
{

	it668x->hdcp_abort = true;

	it668x_hdcp_state(it668x, HDCP_OFF);
}

static void it668x_hdmitx_hdcp_ksv_chk(struct it668x_data *it668x)
{

	it668x->hdcp_cmd = HDCP_KSV_CHK;
	it668x->hdcp_work = true;
	it668x->hdcp_abort = false;
	/* queue_work(it668x->hdcp_cmd_wqs,&it668x->hdcp_control_work); */
	queue_work(it668x->avi_cmd_wqs, &it668x->hdcp_control_work);

}

static void hdmi_hdcp_control_thread(struct work_struct *work)
{

	struct it668x_data *it668x = container_of(work, struct it668x_data, hdcp_control_work);

	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* struct i2c_client *hdmirx = it668x->pdata->hdmirx_client; */

	int ret = 0;
	unsigned char bcaps;
	int i;
	int ksvready;
	int hdcp_fire_count;
	/* struct timing_table timing_config; */


	MHL_DEF_LOG(" hdmi_hdcp_control_thread(%d) ...\n", it668x->hdcp_cmd);
	IT668X_MUTEX_LOCK(&it668x->lock);


	if (it668x->hdcp_work == true) {
		it668x->hdcp_work = false;

		if (it668x->hdcp_abort == true) {
			MHL_ERR_LOG(" hdcp abort....\n");
			goto hdcp_err_exit;
		}
		switch (it668x->hdcp_cmd) {

		case HDCP_START:
			if (it668x->hdcp_enable == true) {

				/*
				 *  AP Do hdcp part I auth use ddc bypass
				 *
				 */
				/*
				   if(it668x->hdcp_work_around == false){
				   it668x->hdcp_work_around = true;
				   it668x_config_ddc_bypass(it668x, true);

				   mutex_unlock(&it668x->lock);

				   //add part I auth code here

				   mutex_lock(&it668x->lock);
				   it668x_config_ddc_bypass(it668x, false);
				   }
				 */

				it668x_hdcp_state(it668x, HDCP_CP_GOING);
				hdcp_fire_count = 0;
				do {
					hdcp_fire_count++;
					ret = it668x_hdmitx_enhdcp_start(it668x);

					IT668X_MUTEX_UNLOCK(&it668x->lock);
					wait_event_interruptible_timeout(it668x->hdcp_wq, NULL,
									 msecs_to_jiffies(150));
					IT668X_MUTEX_LOCK(&it668x->lock);

					if (it668x->hdcp_abort == true) {
						MHL_ERR_LOG("(HDCP_START) hdcp abort....\n");
						goto hdcp_err_exit;
					}

					if (hdcp_fire_count > 4) {
						MHL_ERR_LOG(" hdcp fire count > 10 ..\n");
						it668x_chip_reset(it668x);
						ret = -2;
						break;
					}
				} while (ret < 0);

				if (ret < 0) {
					if (hdcp_fire_count <= 10) {
						MHL_VERB_LOG
						    (" it668x_hdmitx_enhdcp_start fail ..\n");
						it668x_hdcp_state(it668x, HDCP_CP_FAIL);
					}
				}
			}
			break;

		case HDCP_KSV_CHK:

			ksvready = false;

			for (i = 0; i < 25; i++) {

				IT668X_MUTEX_UNLOCK(&it668x->lock);
				wait_event_interruptible_timeout(it668x->hdcp_wq, NULL,
								 msecs_to_jiffies(200));
				IT668X_MUTEX_LOCK(&it668x->lock);

				if (it668x->hdcp_abort == true) {
					MHL_ERR_LOG("(HDCP_KSV_CHK)  hdcp abort....\n");
					goto hdcp_err_exit;
				}
				it668x_hdcp_read(it668x, HDPCD_REGISTER_READ, 0x40, 1, NULL);

				i2c_read_reg(hdmitx, REG_HDMITX_BCAPS, &bcaps);

				if ((bcaps & 0x20) == 0x00) {
					MHL_ERR_LOG("  HDCP KSV list not ready ...\n");
					/* break; */
				} else {
					ksvready = true;
					MHL_DEF_LOG("  HDCP KSV list ready ...\n");
					break;
				}

			}

			if (ksvready == false) {
				MHL_ERR_LOG("  ksv list not read, hdcp fail...\n");

				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, M_KSVLIST_CHECK,
					    0x03);
				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, M_KSVLIST_CHECK,
					    0x00);
			}

			if (it668x_hdcp_repeater_check(it668x) == true) {


				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, B_KSVLIST_DONE,
					    B_KSVLIST_DONE);
				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, M_KSVLIST_CHECK,
					    0x00);
				MHL_DEF_LOG(" SHA Check Result = PASS\n");


			} else {
				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, M_KSVLIST_CHECK,
					    0x03);
				i2c_set_reg(hdmitx, REG_HDMITX_HDCP_KSVLIST_CHECK, M_KSVLIST_CHECK,
					    0x00);
				MHL_ERR_LOG("SHA Check Result = FAIL\n");

			}

			break;

		default:
			MHL_INFO_LOG(" default cmd\n");
			break;
		}
	}

hdcp_err_exit:
	/* it668x->hdcp_cmd = HDCP_CMD_NONE; */
	IT668X_MUTEX_UNLOCK(&it668x->lock);
	MHL_DEF_LOG(" hdmi_hdcp_control_thread END...\n");
	return;
}

static void hdmi_avi_control_thread(struct work_struct *work)
{
	struct it668x_data *it668x = container_of(work, struct it668x_data, avi_control_work);

	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	int ret = 0;
	unsigned char temp;
	/* struct timing_table timing_config; */

#ifdef MHL_2X_3D
	struct cbus_data *cbus_cmd;
	bool feature_3d = false;
#endif

	MHL_DEF_LOG(" hdmi_avi_control_thread(%d) ...\n", it668x->avi_cmd);

	IT668X_MUTEX_LOCK(&it668x->lock);

	if (it668x->avi_work == true) {
		it668x->avi_work = false;

		switch (it668x->avi_cmd) {
		case AVI_CDREST:
			i2c_set_reg(hdmirx, REG_HDMIRX_RESET, 1, 1);
			msleep(20);
			i2c_set_reg(hdmirx, REG_HDMIRX_RESET, 1, 0);
			msleep(20);
			i2c_set_reg(hdmirx, REG_HDMIRX_RESET, EN_CD_RESET, EN_CD_RESET);
			wait_event_interruptible_timeout(it668x->avi_wq, NULL,
							 msecs_to_jiffies(200));
			i2c_set_reg(hdmirx, REG_HDMIRX_RESET, EN_CD_RESET, 0x00);

			i2c_read_reg(hdmirx, REG_HDMIRX_INPUT_STS, &temp);

			if ((temp & B_RXLCLK_READY) == B_RXLCLK_READY)
				it668x_hdmitx_video_state(it668x, HDMI_VIDEO_WAIT);

			break;

		case HPD_HIGH_EVENT:

#ifdef CTS_FORCE_OUTPUT
			/* for cts testing */
			it668x->sink_support_packet_pixel_mode = 1;
			it668x->sink_support_hdmi = true;
			it668x_hdmirx_hpd_high(it668x);
			break;
#endif

			it668x->sink_support_packet_pixel_mode = 0;
			it668x->sink_support_hdmi = false;

			i2c_read_reg(hdmitx, REG_HDMITX_VIDEO_STATUS, &temp);
			if ((temp & B_HPD_DETECT) != B_HPD_DETECT) {
				MHL_VERB_LOG(" HPD low at HPD_HIGH_EVENT ...\n");
				break;
			}

			memset(it668x->mhl_devcap, 0x00, sizeof(it668x->mhl_devcap));

			/*We will read minimum devcap information */
			mhl_queue_devcap_read_locked(it668x, MHL_DEVCAP_MHL_VERSION);
			mhl_queue_devcap_read_locked(it668x, MHL_DEVCAP_VID_LINK_MODE);

			if (it668x->
			    mhl_devcap[MHL_DEVCAP_VID_LINK_MODE] & MHL_DEV_VID_LINK_SUPP_PPIXEL) {
				it668x->sink_support_packet_pixel_mode = 1;
			}

			MHL_DEF_LOG(" HPD high - MHL ver=0x%x, linkmode = 0x%x\n",
				    it668x->mhl_devcap[MHL_DEVCAP_MHL_VERSION],
				    it668x->mhl_devcap[MHL_DEVCAP_VID_LINK_MODE]);

			/* read edid process */
			if (it668x->ddc_bypass == false) {
				ret = mhl_read_edid(it668x);
				if (ret < 0) {
					MHL_ERR_LOG(" edid read failed\n");
					goto err_exit;
				}
				/* mhl_dump_mem(it668x->edidbuf,512); */
			}

			if ((it668x->mhl_devcap[MHL_DEVCAP_MHL_VERSION] == 0x20)
			    && (it668x->mhl_devcap[MHL_DEVCAP_VID_LINK_MODE] &
				(MHL_DEV_VID_LINK_SUPP_PPIXEL | MHL_DEV_VID_LINK_SUPPYCBCR422))
			    /*&&it668x->sink_support_hdmi == true */
			    ) {
				MHL_DEF_LOG(" CEA_NEW_AVI MHL RX Ver.2.x\n");

#ifdef MHL_2X_3D
				feature_3d = true;
				/*Request 3D interrupt to sink device.
				 * To do msc command*/
				cbus_cmd = kzalloc(sizeof(struct cbus_data), GFP_KERNEL);
				if (!cbus_cmd) {
					MHL_VERB_LOG(" failed to allocate cbus data\n");
					goto err_exit;
				}
				cbus_cmd->cmd = SET_INT;
				cbus_cmd->offset = CBUS_MHL_INTR_REG_0;
				cbus_cmd->data = MHL_INT_3D_REQ;
				list_add_tail(&cbus_cmd->list, &it668x->cbus_data_list);

				queue_msc_work(it668x);
#endif

			} else {
				/* TODO: This case MHL 1.0 */
				MHL_DEF_LOG(" CEA_NEW_AVI MHL RX Ver.1.x, Pixel 60\n");
			}
			/* Tranmin remove to it668x_auto_set_vid_fmt */
			it668x_mhl_config_path(it668x, it668x->ddc_bypass);

			it668x_hdmirx_hpd_low(it668x);	/* pull down hpd */
			wait_event_interruptible_timeout(it668x->avi_wq, NULL,
							 msecs_to_jiffies(200));

			/*make HPD be high */
			i2c_read_reg(hdmitx, REG_HDMITX_VIDEO_STATUS, &temp);

			if (temp & B_HPD_DETECT)
				it668x_hdmirx_hpd_high(it668x);

			break;

		case CEA_NEW_AVI:

			it668x_fire_afe(it668x, false);	/* turn off afe */

			it668x_auto_set_vid_fmt(it668x);

			it668x_hdmitx_video_state(it668x, HDMI_VIDEO_ON);
			it668x_v2hfifo_reset(it668x);

			it668x_fire_afe(it668x, true);	/* turn on afe */

			if (it668x->hdcp_enable == true) {
				it668x_mhl_config_path(it668x, false);
				IT668X_MUTEX_UNLOCK(&it668x->lock);
				wait_event_interruptible_timeout(it668x->avi_wq, NULL,
								 msecs_to_jiffies(500));
				IT668X_MUTEX_LOCK(&it668x->lock);
				it668x_hdmitx_hdcp_start(it668x);
			} else {
				it668x_mhl_config_path(it668x, it668x->ddc_bypass);
			}

			break;
		case AVI_HDCP_RESTART:
			if (it668x->hdcp_enable == true) {
				while (it668x->hdcp_work == true) {
					MHL_VERB_LOG("wait hdcp thread end ....");
					IT668X_MUTEX_UNLOCK(&it668x->lock);
					wait_event_interruptible_timeout(it668x->avi_wq, NULL,
									 msecs_to_jiffies(200));
					IT668X_MUTEX_LOCK(&it668x->lock);
				}
				it668x_mhl_config_path(it668x, false);
				it668x_hdmitx_hdcp_start(it668x);
			} else {
				it668x_mhl_config_path(it668x, it668x->ddc_bypass);
			}

			break;
		case CHIP_RESET:
			/* i2c_read_reg(hdmitx , REG_HDMITX_VIDEO_STATUS , &temp); */
			IT668X_DISABLE_IRQ();
			it668x_cbus_disconnect(it668x);
			it668x_initial_chip(it668x);
			IT668X_ENABLE_IRQ();

			break;
		default:
			MHL_INFO_LOG(" default cmd\n");
			break;
		}
	}

err_exit:
	/* it668x->avi_cmd = AVI_CMD_NONE; */
	IT668X_MUTEX_UNLOCK(&it668x->lock);
	MHL_DEF_LOG("hdmi_avi_control_thread END ...\n");
	return;
}

static int it668x_hdmitx_load_csc_table(struct it668x_data *it668x, enum it668x_color_scale csc)
{
	int ret = 0;
	int i;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	unsigned char const *p_tcsc = csc_matrix_rgb2yuv_709_limited;

	switch (csc) {

	case CSC_RGB2YUV_ITU601_0_255:
		p_tcsc = csc_matrix_rgb2yuv_601_full;
		break;

	case CSC_RGB2YUV_ITU709_0_255:
		p_tcsc = csc_matrix_rgb2yuv_709_full;
		break;

	case CSC_RGB2YUV_ITU601_16_235:
		p_tcsc = csc_matrix_rgb2yuv_601_limited;
		break;
	case CSC_RGB2YUV_ITU709_16_235:
		p_tcsc = csc_matrix_rgb2yuv_709_limited;
		break;
	case CSC_YUV2RGB_ITU601_0_255:
		p_tcsc = csc_matrix_yuv2rgb_601_full;
		break;
	case CSC_YUV2RGB_ITU709_0_255:
		p_tcsc = csc_matrix_yuv2rgb_709_full;
		break;
	case CSC_YUV2RGB_ITU601_16_235:
		p_tcsc = csc_matrix_yuv2rgb_601_limited;
		break;
	case CSC_YUV2RGB_ITU709_16_235:
		p_tcsc = csc_matrix_yuv2rgb_709_limited;
		break;
	default:
		return 0;

		break;
	}

	for (i = 0; i < CSC_MATRIX_LEN; i++) {
		ret = i2c_write_reg(hdmitx, REG_HDMITX_CSCTABLE_START + i, p_tcsc[i]);
		if (ret < 0)
			return ret;

		/* pr_hdmi(" reg%02X <- %02X\n",(int)(i+0x73),(int)p_tcsc[i]); */
	}

	return ret;

}


static int it668x_hdmitx_reg_init(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_GATE_CLK, B_GATE_RCLK, 0x00);
	if (ret < 0)
		return ret;
	/* add for HDCP issue */
	ret = i2c_set_reg(hdmitx, REG_HDMITX_SWREST, M_RESET_ALL, M_RESET_ALL);
	if (ret < 0)
		return ret;

	/* PLL Reset */
	ret = i2c_set_reg(hdmitx, REG_HDMITX_AFE_XP_CONFIG, B_XP_RESET, 0x00);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_ASYNC_RESET | B_REST_RCLK | M_RESET_ALL,
			B_ASYNC_RESET | B_REST_RCLK | M_RESET_ALL);
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_RCLK, 0x00);
	if (ret < 0)
		return ret;

	/* MHL Slave Address */
	ret =
	    i2c_set_reg(hdmitx, REG_HDMITX_MHL_I2C_EN, MASK_ALL_BIT,
			IT668x_MHL_ADDR | B_EN_MHL_I2C);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_RINGOSC_CONFIG, M_RINGOSC_SPEED, RINGOSC_NORMAL);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_AFE_XP_CONFIG, B_XP_GAIN | B_XP_ER0, B_XP_ER0);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_CBUS_SMT, B_ENA_CBUS_SMT, B_ENA_CBUS_SMT);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, MASK_ALL_BIT, UNLOCK_PWD_0);
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, MASK_ALL_BIT, UNLOCK_PWD_1);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(hdmitx, REG_HDMITX_TEST_CTRL0, B_CBUS_DRVING | B_FORCE_VIDEO_STABLE,
			SET_CBUS_DRINING);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_TEST_CTRL1, M_DDC_SPEED, SET_DDC_SPEED);
	if (ret < 0)
		return ret;

#ifdef CTS_FORCE_OUTPUT

	/* ret =i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF, UNLOCK_PWD_0); */
	/* if (ret < 0) */
	/* return ret; */
	/* ret =i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF, UNLOCK_PWD_1); */
	/* if (ret < 0) */
	/* return ret; */
	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_IO, B_FORCE_HPD, B_FORCE_HPD);
	if (ret < 0)
		return ret;

	/* ret =i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF, LOCK_PWD); */
	/* if (ret < 0) */
	/* return ret; */
#endif

	ret = i2c_set_reg(hdmitx, REG_HDMITX_REG_LOCK, MASK_ALL_BIT, LOCK_PWD);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_VID_IO_CONFIG, B_PLLBUF_AUTO_RST, B_PLLBUF_AUTO_RST);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(hdmitx, REG_HDMITX_PULLUP_SEL, M_5KUP_SEL | M_10KUP_SEL,
			SET_R_PULLUP10K | SET_R_PULLUP5K);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(hdmitx, REG_HDMITX_CSC_SIGNEDBIT,
			B_CSC_SIGNED | B_Y_SIGNED | B_CB_SIGNED | B_CR_SIGNED, 0x00);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_AVI_COLOR, 0x08, 0x08);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_IO, B_ENA_MHLTX_INT, B_ENA_MHLTX_INT);
	if (ret < 0)
		return ret;

	/* Clear all Interrupt */

	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_06, MASK_ALL_BIT, 0xFF);
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_07, MASK_ALL_BIT, 0xFF);
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_08, MASK_ALL_BIT, 0xFF);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(hdmitx, REG_HDMITX_INT_07_MASK, B_MASK_RXSEN | B_MASK_HPD, 0x00);
	if (ret < 0)
		return ret;

	return ret;
}

static void it668x_init_mhl_devcaps(struct it668x_data *it668x)
{
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_00, DEV_STATE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_01, DEV_MHL_VERSION);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_02, DEV_CAT_SOURCE_NO_PWR);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_03, DEV_ADOPTER_ID_H);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_04, DEV_ADOPTER_ID_L);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_05, DEV_VID_LINK_MODE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_06, DEV_AUDIO_LINK_MODE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_07, DEV_VIDEO_TYPE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_08, DEV_LOGICAL_DEV);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_09, DEV_BANDWIDTH);

	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_10, DEV_FEATURE_FLAG);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_11, DEV_DEVICE_ID_H);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_12, DEV_DEVICE_ID_L);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_13, DEV_SCRATCHPAD_SIZE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_14, DEV_INT_STATUS_SIZE);
	i2c_write_reg(mhl, REG_MHL_DEVICE_CAP_15, DEV_RESERVED);

	return;
}

static int it668x_mhl_reg_init(struct it668x_data *it668x)
{
	int ret = 0;

	struct i2c_client *mhl = it668x->pdata->mhl_client;

	/* u8 temp; */

	/* MHLTX Reset */
	ret = i2c_set_reg(mhl, REG_MHL_CONTROL, B_DISABLE_MHL, B_DISABLE_MHL);	/* Disable CBUS */
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(mhl, REG_MHL_INT_04, MASK_ALL_BIT, 0xFF);	/*  */
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(mhl, REG_MHL_INT_05, MASK_ALL_BIT, 0xFF);	/*  */
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(mhl, REG_MHL_INT_06, MASK_ALL_BIT, 0xFF);	/*  */
	if (ret < 0)
		return ret;
	/* Enable Disconnect and Discovery Interrupt */
	ret = i2c_set_reg(mhl, REG_MHL_INT_06_MASK0A, MASK_ALL_BIT, 0x80);
	if (ret < 0)
		return ret;
	/* Enable CBusNotDet Interrupt */
	ret = i2c_set_reg(mhl, REG_MHL_INT_04_MASK08, MASK_ALL_BIT, 0xFF);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(mhl, REG_MHL_OPTION, B_CBUS_LOW_WAKE_UP, 0x00);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(mhl, REG_MHL_OSCD_CTRL, B_CBUS_DEGLITCH | M_100MS_ADJ,
			B_CBUS_DEGLITCH | SET_100MS_ADJ);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(mhl, REG_MHL_PATHEN_CTROL, B_HW_AUTO_PATH_EN | B_PACKET_PIXEL_HDCP_OPT,
			B_PACKET_PIXEL_HDCP_OPT);
	if (ret < 0)
		return ret;

	ret =
	    i2c_set_reg(mhl, REG_CBUS_LINK_LAYER_CTEL_36, M_T_ACK_HIGH | M_T_ACK_LOW,
			SET_T_ACK_HIGH | SET_T_ACK_LOW);
	if (ret < 0)
		return ret;

	/* i2c_read_reg(mhl, REG_CBUS_DDC_CONTROL, &temp); */
	/* printk(" I2C READ MHL reg 0x38 = %2.2X\n",temp); */
	ret = i2c_set_reg(mhl, REG_CBUS_DDC_CONTROL, B_EN_DDC_WAIT_ABORT, 0x00);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(mhl, REG_MSC_FIFO_MODE, B_PKT_FIFO_BURST_MODE, B_PKT_FIFO_BURST_MODE);
	if (ret < 0)
		return ret;
	ret = i2c_set_reg(mhl, REG_CBUS_DISCOVER_CONTROL1, M_CBUS_DISC_OPT_1, SET_CBUS_DISC_OPT_1);
	if (ret < 0)
		return ret;

	/* Enable MHL */
	ret =
	    i2c_set_reg(mhl, REG_MHL_CONTROL, B_PACKET_PIXEL_GB_SWAP | B_EN_VBUS_OUT,
			B_PACKET_PIXEL_GB_SWAP | B_EN_VBUS_OUT);
	if (ret < 0)
		return ret;

	ret = i2c_set_reg(mhl, 0x2B, 0xF0, 0xF0);
	if (ret < 0)
		return ret;

	it668x_init_mhl_devcaps(it668x);

	return ret;
}


#define  DEFAULT_OCLK   50310	/* Default OCLK value           // V1.4_3: 20140114 (B2) */
#define  MAX_OCLK_DIFF  15	/* Maximum OCLK deviation (%)   // V1.4_3: 20140114 (B2) */
#define  SET_OCLK_COMP  0	/* OCLK compensation            // V1.4_3: 20140114 (B2) */

static unsigned long it668x_read_default_calibration(struct it668x_data *it668x)
{

	int no_sip_data = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* struct i2c_client *hdmirx = it668x->pdata->hdmirx_client; */

	unsigned long oscsclk, diff;
	unsigned char rddata[4];

	MHL_VERB_LOG("it668x_read_default_calibration() %p %p\n", it668x, hdmitx);
	/* hdmitxwr(0xF8, 0xC3); */
	/* hdmitxwr(0xF8, 0xA5); */

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	i2c_set_reg(hdmitx, REG_HDMITX_SIPROM_CONFIG, B_ENA_SIPROM | B_SIPROM_SEL,
		    B_ENA_SIPROM | B_SIPROM_SEL_EXT);

	i2c_set_reg(hdmitx, REG_HDMITX_SIPMISC, B_SIP_RESET, B_SIP_RESET);
	i2c_set_reg(hdmitx, REG_HDMITX_SIPMISC, B_SIP_RESET, 0);

	i2c_set_reg(hdmitx, REG_HDMITX_SIP_OPTION_SEL, B_SIP_MACRO_SEL, B_SIP_MACRO_SEL);

	/* hdmitxset(0x20, 0x90, 0x90);   // enable SiPROM */
	/* hdmitxset(0x37, 0x10, 0x10);   // reset SiPROM */
	/* hdmitxset(0x37, 0x10, 0x00); */

	/* hdmitxset(0x36, 0x01, 0x01); */

	i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR0, 0x00);
	i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR1, 0x00);
	i2c_write_reg(hdmitx, REG_HDMITX_SIP_COMMAND, SIP_COMMAND_READ);

	/* hdmitxwr(0x30, 0x00);  // start address */
	/* hdmitxwr(0x31, 0x00); */
	/* hdmitxwr(0x33, 0x04);  // read fire */
	/* hdmitxbrd(0x24, 4, &rddata[0]); */

	i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK, rddata);
	i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 1, rddata + 1);
	i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 2, rddata + 2);
	i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 3, rddata + 3);

	/* rddata[0] = hdmitxrd(0x24); */
	/* rddata[1] = hdmitxrd(0x25); */
	/* rddata[2] = hdmitxrd(0x26); */
	/* rddata[3] = hdmitxrd(0x27); */

	MHL_VERB_LOG(" SiPROM[0]=0x%02X, SiPROM[1]=0x%02X, SiPROM[2]=0x%02X, SiPROM[3]=0x%02X\n",
		     (int)rddata[0], (int)rddata[1], (int)rddata[2], (int)rddata[3]);

	no_sip_data = false;

	if (rddata[0] == 0x01 && rddata[1] == 0x01 && rddata[2] == 0x01 && rddata[3] == 0x01) {
		/* hdmitxwr(0x30, 0x01);  // start address */
		/* hdmitxwr(0x31, 0x60); */
		i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR0, 0x01);
		i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR1, 0x60);
		MHL_VERB_LOG(" CP 100ms calibration using Block 0 ...\n");
	} else if (rddata[0] == 0xff && rddata[1] == 0x00 && rddata[2] == 0xff && rddata[3] == 0x00) {
		/* hdmitxwr(0x30, 0x03);  // start address */
		/* hdmitxwr(0x31, 0x60); */
		i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR0, 0x03);
		i2c_write_reg(hdmitx, REG_HDMITX_SIP_ADDR1, 0x60);
		MHL_VERB_LOG(" CP 100ms calibration using Block 1 ...\n");
	} else
		no_sip_data = true;

	if (no_sip_data == false) {

		i2c_write_reg(hdmitx, REG_HDMITX_SIP_COMMAND, SIP_COMMAND_READ);

		i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK, rddata);
		i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 1, rddata + 1);
		i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 2, rddata + 2);
		i2c_read_reg(hdmitx, REG_HDMITX_SIP_READBACK + 3, rddata + 3);

		oscsclk = ((rddata[2] << 16) + (rddata[1] << 8) + rddata[0]) / 100;
		oscsclk += SET_OCLK_COMP;

		if (oscsclk > DEFAULT_OCLK)
			diff = oscsclk - DEFAULT_OCLK;
		else
			diff = oscsclk - DEFAULT_OCLK;

		if (diff > (DEFAULT_OCLK * MAX_OCLK_DIFF / 100)) {
			MHL_ERR_LOG(" ERROR: CP 100ms calibration value > MaxDiff !!!\n");
			no_sip_data = true;
		}

	}

	if (no_sip_data == true) {
		/* oscsclk = DEFAULT_OCLK; */
		oscsclk = 0;
		MHL_ERR_LOG(" no siprom calibration value, oscsclk = 0 !!\n");

	}

	return oscsclk;
}

/* ////////////////////////////////////////////////////////////////// */
/* void cal_oclk( void ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_cal_oclk(struct it668x_data *it668x)
{
	int i;

	unsigned char temp;

	unsigned long oscclk;
	unsigned int oscdiv;

	int ret = 0;
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	it668x->rclk = 0;
	it668x->oscdiv = 5;

	MHL_VERB_LOG(" it668x_cal_oclk() %p %p %p\n", hdmirx, hdmitx, mhl);

	ret = i2c_write_reg(hdmirx, 0x2B, IT668x_HDMITX_ADDR | 0x01);
	if (ret < 0) {
		MHL_ERR_LOG(" it668x_cal_oclk FAIL config HDMITX address..\n");
		return ret;
	}

	/* CBUS Slave address enable */
	ret = i2c_set_reg(hdmitx, REG_HDMITX_MHL_I2C_EN, 0xFF, IT668x_MHL_ADDR | B_EN_MHL_I2C);
	if (ret < 0) {
		MHL_ERR_LOG(" it668x_cal_oclk FAIL config MHL address..\n");
		return ret;
	}

	/* Disable CBUS */
	ret = i2c_set_reg(mhl, REG_MHL_CONTROL, B_DISABLE_MHL, B_DISABLE_MHL);
	if (ret < 0) {
		MHL_ERR_LOG(" it668x_cal_oclk FAIL REG_MHL_CONTROL ..\n");
		return ret;
	}

	oscclk = 0;		/* it668x_read_default_calibration(it668x); */

	if (oscclk == 0) {

		for (i = 0; i < 4; i++) {

			ret = i2c_write_reg(mhl, B_EN_CALIBRATION, B_EN_CALIBRATION);
			if (ret < 0)
				return ret;

			hold1ms(MHL_OCLK_CAL_TIME);

			ret = i2c_write_reg(mhl, B_EN_CALIBRATION, 0);
			if (ret < 0)
				return ret;

			ret = i2c_read_reg(mhl, REG_R100MS_CNT0, &temp);
			if (ret < 0)
				return ret;

			oscclk += (unsigned long)temp;

			ret = i2c_read_reg(mhl, REG_R100MS_CNT1, &temp);
			if (ret < 0)
				return ret;

			oscclk += (unsigned long)temp << 8;

			ret = i2c_read_reg(mhl, REG_R100MS_CNT2, &temp);
			if (ret < 0)
				return ret;

			oscclk += (unsigned long)temp << 16;

			MHL_VERB_LOG(" loop(%d ms)=%d, sum =%lu\n", MHL_OCLK_CAL_TIME, i, oscclk);
		}

		oscclk = (oscclk / 4) / MHL_OCLK_CAL_TIME;
	}
	/* if we have a bad calibration value, try using siprom value */
	if (oscclk < 39000UL || oscclk > 59000UL) {
		oscclk = it668x_read_default_calibration(it668x);
		MHL_DEF_LOG("OSCCLK.sip=%luKHz\n", oscclk);
	}
	/* if we have a bad calibration value here, use the average vaule */
	if (oscclk < 39000UL || oscclk > 59000UL) {
		oscclk = 49854;
		MHL_DEF_LOG("OSCCLK.def=%luKHz\n", oscclk);
	}

	MHL_DEF_LOG(" OSCCLK=%luKHz\n", oscclk);

	oscdiv = oscclk / 10000;

	if ((oscclk % 10000) > 5000)
		oscdiv++;

	MHL_VERB_LOG(" oscdiv=%d\n", oscdiv);

	MHL_VERB_LOG(" OCLK=%lukHz\n", oscclk / oscdiv);

	/* ret = i2c_set_reg(mhl, 0x01, 0x70, oscdiv<<4); */
	/* if (ret < 0) */
	/* return ret; */

	i2c_set_reg(hdmirx, 0x0E, 0x03, it668x->rclk_div);	/* RCLK div sel[00] = 2, [01] =4 */

	if (it668x->rclk_div == 0)
		oscclk /= 2;
	else if (it668x->rclk_div == 1)
		oscclk /= 4;

	it668x->oscdiv = oscdiv;
	/* it668x->rclk = (unsigned long) (oscclk - (oscclk>>4));// 75% */

	it668x->rclk = (unsigned long)oscclk;

	return ret;
}

static int it668x_mhl_config_rclk(struct it668x_data *it668x)
{
	int ret = 0;
	unsigned int t10usint, t10usflt;

	struct i2c_client *mhl = it668x->pdata->mhl_client;

	if (it668x->rclk == 0)
		it668x_cal_oclk(it668x);

	t10usint = (unsigned int)it668x->rclk / 100;
	t10usflt = (unsigned int)it668x->rclk % 100;

	MHL_DEF_LOG(" rclk=%lukHz\n", it668x->rclk);
	MHL_DEF_LOG(" oscdiv = %2.2X\n", it668x->oscdiv);
	MHL_DEF_LOG(" T10usInt=0x%X, T10usFlt=0x%X\n", t10usint, t10usflt);

	ret = i2c_set_reg(mhl, 0x01, 0x70, (unsigned char)(it668x->oscdiv << 4));
	if (ret < 0)
		return ret;
	ret = i2c_write_reg(mhl, REG_MHL_10US_TIME_INTE, (unsigned char)(t10usint & 0xFF));
	if (ret < 0)
		return ret;
	ret =
	    i2c_write_reg(mhl, REG_MHL_10US_TIME_FLOAT,
			  (unsigned char)(((t10usint & 0x100) >> 1) + t10usflt));
	if (ret < 0)
		return ret;
	/* MHL_VERB_LOG(" MHL reg 0x02 = %X , reg 0x03 = %X\n", (int )mhltxrd(0x02), (int)mhltxrd(0x03)); */
	/* it668x_debgio_en(1); */

	return ret;
}

static int it668x_set_hdmitx_afe(struct it668x_data *it668x, unsigned long hclk,
				 unsigned long tmdsclk)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	MHL_DEF_LOG(" setup_mhltxafe(%lu MHz)\n", (unsigned long)hclk / 1000);

	if (tmdsclk > 80000)
		i2c_set_reg(hdmitx, 0x62, 0x90, 0x80);
	else
		i2c_set_reg(hdmitx, 0x62, 0x90, 0x10);

	i2c_set_reg(hdmitx, 0x63, 0x3F, 0x2F);	/* 6682 setting */

	if (hclk > 200000) {
		i2c_set_reg(hdmitx, 0x61, 0x02, 0x02);
		i2c_set_reg(hdmitx, 0x64, 0xFF, 0x00);
		i2c_set_reg(hdmirx, 0x0E, 0x10, 0x00);	/* HCLK not invert at 720P/1080P */
	} else {
		i2c_set_reg(hdmitx, 0x61, 0x02, 0x00);
		i2c_set_reg(hdmitx, 0x64, 0xFF, 0x01);

		i2c_set_reg(hdmirx, 0x0E, 0x10, 0x10);	/* HCLK invert at other video modes */
	}

	return ret;

}

static int it668x_fire_afe(struct it668x_data *it668x, int on)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	if (on) {
		ret =
		    i2c_set_reg(hdmitx, REG_HDMITX_AFE_DRV_CONFIG, B_DRV_PWRDN | B_DEV_REST, 0x00);
		if (ret < 0)
			return ret;
	} else {
		ret =
		    i2c_set_reg(hdmitx, REG_HDMITX_AFE_DRV_CONFIG, B_DRV_PWRDN | B_DEV_REST,
				B_DRV_PWRDN | B_DEV_REST);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int it668x_is_usb_mode(struct it668x_data *it668x)
{
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	unsigned char tmp = 0;
	int ret;

	ret = i2c_read_reg(mhltx, REG_MHL_CONTROL, &tmp);
	if (ret < 0)
		MHL_ERR_LOG(" %s() error at line %d", __func__, __LINE__);

	if (0 == (tmp & B_USB_SWITCH_ON))
		return 0;	/* MHL mode */

	return 1;		/* USB mode */
}

static int it668x_cbus_drive_low(struct it668x_data *it668x)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	int ret = 0;

	if (0 == it668x_is_usb_mode(it668x)) {	/* if mhl mode, pull down CBUS */

		ret = i2c_set_reg(mhltx, REG_MHL_CONTROL, B_DISABLE_MHL, B_DISABLE_MHL);
		if (ret < 0)
			MHL_VERB_LOG(" %s() error at line %d", __func__, __LINE__);

		ret = i2c_set_reg(hdmitx, 0x64, 0x80, 0x80);
		if (ret < 0)
			MHL_VERB_LOG(" %s() error at line %d", __func__, __LINE__);
	}

	return ret;
}

static int it668x_cbus_release(struct it668x_data *it668x)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	int ret = 0;

	if (0 == it668x_is_usb_mode(it668x)) {	/* if mhl mode, pull down CBUS */

		ret = i2c_set_reg(hdmitx, 0x64, 0x80, 0x00);
		if (ret < 0)
			MHL_VERB_LOG(" %s() error at line %d", __func__, __LINE__);

		ret = i2c_set_reg(mhltx, REG_MHL_CONTROL, B_DISABLE_MHL, 0x00);
		if (ret < 0)
			MHL_VERB_LOG(" %s() error at line %d", __func__, __LINE__);
	}

	return ret;
}

static int it668x_cbus_disconnect(struct it668x_data *it668x)
{
	int ret;

	ret = it668x_cbus_drive_low(it668x);
	if (ret == 0) {
		delay1ms(200);
		ret = it668x_cbus_release(it668x);
	}

	return ret;
}

static int it668x_mhl_switch_state(struct it668x_data *it668x, enum mhl_state state)
{

	int ret = 0;
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	/* struct i2c_client *hdmitx = it668x->pdata->hdmitx_client; */

	unsigned char temp;

	/* #ifdef CBUS_CTS_OPT */
	/* if(it668x->mhl_state == MHL_CBUS_DISCOVER){ */
	/* it668x_mhl_config_rclk(it668x);  //restore rclk */
	/* } */
	/* #endif */

/* if(it668x->mhl_state == state){ */
/* //check MHL Cbus link time out counter */
/* switch(state){ */
/* case MHL_USB_PWRDN: */
/* break; */
/* case MHL_CBUS_START: */
/* break; */
/* case MHL_LINK_DISCONNECT: */
/* break; */
/* case MHL_1K_DETECT: */
/* //it668x->cbus_1k_det_timeout++; */
/* break; */
/* case MHL_CBUS_DISCOVER: */
/* it668x->cbus_discover_timeout++; */
/* break; */
/* case MHL_CBUS_CONNECTED: */
/* break; */
/* default : */
/* break; */
/* } */
/*  */
/* return ret; */
/*  */
/* } */

	switch (state) {
	case MHL_USB_PWRDN:
		MHL_DEF_LOG("[it668x] it668x->Mhl_state => MHL_USB_PWRDN\n");
		break;
	case MHL_LINK_DISCONNECT:

		it668x_switch_to_usb(it668x, true);

		if (it668x->mhl_pow_support)
			it668x_turn_on_vbus(it668x, false);

		ret = i2c_write_reg(mhl, REG_MHL_INT_04_MASK08, 0xFF);
		if (ret < 0)
			return ret;
		ret = i2c_write_reg(mhl, REG_MHL_INT_05_MASK09, 0xFF);
		if (ret < 0)
			return ret;

		/* ret = i2c_set_reg(mhl, REG_MHL_INT_06_MASK0A, 0xFF, B_MASK_1K_FAIL_IRQ);    //disable  1k fail IRQ */
		/* ret  = i2c_write_reg(mhl ,REG_MHL_INT_06_MASK0A, B_MASK_1K_FAIL_IRQ ); */
		ret =
		    i2c_set_reg(mhl, REG_MHL_INT_06_MASK0A, B_MASK_1K_FAIL_IRQ, B_MASK_1K_FAIL_IRQ);
		if (ret < 0)
			return ret;

		/* it668x->mhl_active_link_mode = 0; */

		it668x->cbus_detect_timeout = 0;
		it668x->cbus_1k_det_timeout = 0;
		it668x->cbus_discover_timeout = 0;
		it668x->mhl_active_link_mode = 0;

		MHL_DEF_LOG("[it668x] it668x->Mhl_state => MHL_LINK_DISCONNECT\n");
		break;

	case MHL_CBUS_START:
		/* reset Cbus fsm */
		ret = i2c_set_reg(mhl, REG_MHL_CONTROL, B_USB_SWITCH_ON | B_DISABLE_MHL, 0x00);
		if (ret < 0)
			return ret;
		/* enable  1k fail IRQ */
		ret = i2c_set_reg(mhl, REG_MHL_INT_06_MASK0A, B_MASK_1K_FAIL_IRQ, 0x00);
		if (ret < 0)
			return ret;

		it668x->cbus_detect_timeout = 0;
		it668x->cbus_1k_det_timeout = 0;
		it668x->cbus_discover_timeout = 0;
		it668x->mhl_active_link_mode = 0;

		MHL_DEF_LOG("[it668x] it668x->Mhl_state => MHL_CBUS_START\n");
		break;
/* case MHL_1K_DETECT: */
/* ret = i2c_set_reg(mhl, REG_MHL_INT_06_MASK0A, B_MASK_1K_FAIL_IRQ, 0x00);    //enable  1k fail IRQ */
/* if (ret < 0) */
/* return ret; */
/*  */
/* MHL_MHL_LOG(" it668x->Mhl_state => MHL_1K_DETECT\n"); */
/* break; */
/*  */
/* case MHL_CBUS_DISCOVER: */
/* // Enable MHL CBUS Interrupt */
/* it668x_switch_to_mhl(it668x); */
/* MHL_MHL_LOG(" it668x->Mhl_state => MHL_CBUS_DISCOVER\n"); */
/* break; */

	case MHL_CBUS_CONNECTED:
		it668x->cbus_detect_timeout = 0;
		it668x->cbus_1k_det_timeout = 0;
		it668x->cbus_discover_timeout = 0;
		it668x->cbus_packet_fail_counter = 0;
		it668x->cbus_msc_fail_counter = 0;
		it668x->cbus_ddc_fail_counter = 0;

		it668x->mhl_active_link_mode = 0;
		it668x->hdcp_work_around = false;

		ret = i2c_write_reg(mhl, REG_MHL_INT_04_MASK08, B_MASK_CBUS_RX_PKT_FAIL |
				    B_MASK_CBUS_RX_PKT_DONE | B_MASK_CBUS_TX_PKT_FAIL |
				    B_MASK_CBUS_TX_PKT_DONE);
		if (ret < 0)
			return ret;

		ret = i2c_write_reg(mhl, REG_MHL_INT_05_MASK09, 0xFF);
		if (ret < 0)
			return ret;

		ret = i2c_write_reg(mhl, REG_MHL_INT_06_MASK0A, 0x80);
		if (ret < 0)
			return ret;

		i2c_read_reg(mhl, 0x0F, &temp);

		MHL_DEF_LOG("[it668x] it668x->Mhl_state => MHL_CBUS_CONNECTED %2.2X\n", (int)temp);
		break;
	default:
		break;
	}

	it668x->mhl_state = state;
	return ret;
}

static int it668x_hdmitx_set_color_scale(struct it668x_data *it668x)
{

	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	enum it668x_color_scale csc = CSC_NONE;

	unsigned char cscmode = B_HDMITX_CSC_BYPASS;
	unsigned char colorflag;
	unsigned char en_clip;

	MHL_HDMI_LOG(" it668x_hdmitx_set_color_scale()\n");

	colorflag = 0;

	/* DynCEA(16-235), DynVESA(0-255) */
	if (it668x->color_dyn_range == COLOR_RANGE_CEA)
		colorflag |= F_VIDMODE_16_235;

	if (it668x->color_ycbcr_coef == YCBCR_COEF_ITU709)
		colorflag |= F_VIDMODE_ITU709;


	if (it668x->input_color_mode != it668x->output_color_mode)
		if (it668x->input_color_mode == F_MODE_RGB444)
			cscmode = B_HDMITX_CSC_RGB2YUV;
		else
			cscmode = B_HDMITX_CSC_YUV2RGB;

	switch (cscmode) {
	case B_HDMITX_CSC_RGB2YUV:
		MHL_HDMI_LOG(" CSC = RGB2YUV %x ", cscmode);
		switch (colorflag & (F_VIDMODE_ITU709 | F_VIDMODE_16_235)) {
		case F_VIDMODE_ITU709 | F_VIDMODE_16_235:

			csc = CSC_RGB2YUV_ITU709_16_235;

			MHL_HDMI_LOG(" ITU709 16-235 ");
			break;
		case F_VIDMODE_ITU709 | F_VIDMODE_0_255:

			csc = CSC_RGB2YUV_ITU709_0_255;

			MHL_HDMI_LOG(" ITU709 0-255 ");
			break;
		case F_VIDMODE_ITU601 | F_VIDMODE_16_235:

			csc = CSC_RGB2YUV_ITU601_16_235;

			MHL_HDMI_LOG(" ITU601 16-235 ");
			break;
			/* case F_VIDMODE_ITU601|F_VIDMODE_0_255: */
		default:
			csc = CSC_RGB2YUV_ITU601_0_255;
			MHL_HDMI_LOG(" ITU601 0-255 ");
			break;
		}
		break;

	case B_HDMITX_CSC_YUV2RGB:
		MHL_HDMI_LOG(" CSC = YUV2RGB %x ", cscmode);
		switch (colorflag & (F_VIDMODE_ITU709 | F_VIDMODE_16_235)) {
		case F_VIDMODE_ITU709 | F_VIDMODE_16_235:

			csc = CSC_YUV2RGB_ITU709_16_235;

			MHL_HDMI_LOG(" ITU709 16-235 ");
			break;
		case F_VIDMODE_ITU709 | F_VIDMODE_0_255:

			csc = CSC_YUV2RGB_ITU709_0_255;

			MHL_HDMI_LOG(" ITU709 0-255 ");
			break;
		case F_VIDMODE_ITU601 | F_VIDMODE_16_235:

			csc = CSC_YUV2RGB_ITU601_16_235;

			MHL_HDMI_LOG(" ITU601 16-235 ");
			break;
			/* case F_VIDMODE_ITU601|F_VIDMODE_0_255: */
		default:
			csc = CSC_YUV2RGB_ITU601_0_255;
			MHL_HDMI_LOG(" ITU601 0-255 ");
			break;
		}

		break;
	default:
		/* csc = CSC_NONE; */
		break;
	}

	it668x_hdmitx_load_csc_table(it668x, csc);

	en_clip = 0;
	if (it668x->color_ycbcr_coef == YCBCR_COEF_ITU601)
		en_clip = B_COLOR_CLIP;

	if (cscmode == 0x00) {

		i2c_set_reg(hdmitx, REG_HDMITX_GATE_CLK, B_GATE_TXCLK, B_GATE_TXCLK);

		if (en_clip)
			i2c_write_reg(hdmitx, REG_HDMITX_RGB_BANK_LEVEL, 0x10);
	} else {
		i2c_set_reg(hdmitx, REG_HDMITX_GATE_CLK, B_GATE_TXCLK, 0x00);
	}

	i2c_set_reg(hdmitx, REG_HDMITX_CSC_CONFIG, M_CSC_SEL | B_COLOR_CLIP, en_clip | cscmode);

	return ret;
}

static unsigned long it668x_hdmitx_cal_pclk(struct it668x_data *it668x)
{
	unsigned char predivsel, reg_val;
	unsigned int rddata, i, pwdiv;
	unsigned long sumdiv;
	unsigned char temp;
	unsigned long sum;
	unsigned long pclk = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	i2c_set_reg(hdmitx, REG_HDMITX_PCLK_CNT, B_ENA_CNT | M_DIV_SEL, B_ENA_CNT);
	delay1ms(1);
	i2c_set_reg(hdmitx, REG_HDMITX_PCLK_CNT, B_ENA_CNT, 0x00);

	i2c_read_reg(hdmitx, REG_HDMITX_PCLK_CNT, &temp);
	rddata = ((unsigned int)temp & 0x0F) << 8;
	i2c_read_reg(hdmitx, REG_HDMITX_CLKCNT_L, &temp);
	rddata += (unsigned int)temp;

	MHL_HDMI_LOG(" pclk Count Pre-Test value=%u\n", rddata);

	if (rddata < 16) {
		predivsel = 7;
		pwdiv = 128;
	} else if (rddata < 32) {
		predivsel = 6;
		pwdiv = 64;
	} else if (rddata < 64) {
		predivsel = 5;
		pwdiv = 32;
	} else if (rddata < 128) {
		predivsel = 4;
		pwdiv = 16;
	} else if (rddata < 256) {
		predivsel = 3;
		pwdiv = 8;
	} else if (rddata < 512) {
		predivsel = 2;
		pwdiv = 4;
	} else if (rddata < 1024) {
		predivsel = 1;
		pwdiv = 2;
	} else {
		predivsel = 0;
		pwdiv = 1;
	}
	MHL_HDMI_LOG(" predivsel=%X\n", (int)predivsel);

	sum = 0;

	i2c_set_reg(hdmitx, REG_HDMITX_PCLK_CNT, M_DIV_SEL, (predivsel << 4));
	i2c_read_reg(hdmitx, REG_HDMITX_PCLK_CNT, &reg_val);
	reg_val &= ~B_ENA_CNT;

	for (i = 0; i < 4; i++) {

		i2c_write_reg(hdmitx, REG_HDMITX_PCLK_CNT, reg_val | B_ENA_CNT);
		delay1ms(1);
		i2c_write_reg(hdmitx, REG_HDMITX_PCLK_CNT, reg_val);

		i2c_read_reg(hdmitx, REG_HDMITX_CLKCNT_L, &temp);
		rddata = (unsigned int)temp;
		i2c_read_reg(hdmitx, REG_HDMITX_PCLK_CNT, &temp);
		rddata += ((((unsigned int)temp) & 0x0F) << 8);

		if (it668x->rclk_div == 0)
			rddata *= 2;

		sum += (unsigned long)rddata;

		MHL_VERB_LOG(" [%d] clock cnt = %u sum= %lu\n", i, rddata, sum);
	}

	sumdiv = (unsigned long)(i * pwdiv);

	MHL_HDMI_LOG("    sumdiv= %lu\n", sumdiv);

	if (sumdiv)
		sum = sum / sumdiv;

	if (sum)
		pclk = (it668x->rclk * 2048) / sum;

	MHL_DEF_LOG(" Count TxCLK=%lu kHz\n", (unsigned long)pclk);

	return pclk;

}

static int it668x_get_3dformat(struct it668x_data *it668x)
{

	int ret = 0;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	unsigned char temp;

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_VSI_READBACK, B_EN_VSI_READBACK);

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);

	i2c_read_reg(hdmitx, REG_HDMITX_3DINFO_PB04, &temp);

	it668x->sel_3dformat = (temp & 0xF0) >> 4;

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_VSI_READBACK, 0x00);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);

	return ret;

}

static int it668x_config_3d_output(struct it668x_data *it668x)
{

	int ret = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	unsigned int isfb_start, isfb_end;

	unsigned char temp;

	MHL_VERB_LOG(" enable 3d mode %2.2X\n", (int)it668x->enable_3d);

	i2c_write_reg(hdmitx, 0xB3, 0xFF);
	i2c_write_reg(hdmitx, 0xB4, 0xFF);
	i2c_write_reg(hdmitx, 0xB5, 0xFF);

	if (it668x->enable_3d == true) {

		it668x_get_3dformat(it668x);

		MHL_VERB_LOG(" recive 3d mode %2.2X\n", (int)it668x->sel_3dformat);

		if (it668x->sel_3dformat == FRMEPKT) {

			/* isfb_start = 0xfff; */
			/* isfb_end = 0x2d; */

			isfb_start = 720;
			isfb_end = 720 + 30;

			temp = (unsigned char)(isfb_start & 0xFF);

			i2c_write_reg(hdmitx, 0xB3, temp);

			temp = (unsigned char)(isfb_end & 0xFF);

			i2c_write_reg(hdmitx, 0xB4, temp);

			temp =
			    (unsigned char)(((isfb_end & 0xF00) >> 4) +
					    ((isfb_start & 0xF00) >> 8));

			i2c_write_reg(hdmitx, 0xB5, temp);

		}
	}

	return ret;
}


static int it668x_auto_set_vid_fmt(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* struct i2c_client *hdmirx = it668x->pdata->hdmirx_client; */

	int packet_mode;
	unsigned long tmdsclk, hclk;
	/* unsigned long cal_hclk,cal_vclk; */
	unsigned char input_hight_speed = false;
	unsigned char temp = 0;
	/*  */
	/* NOTE: Packet pixel mode only YUV422 */
	/*  */

	/* i2c_read_reg(hdmirx, 0x06, &temp ); */
	/* input_hight_speed = (temp&0x10)>>4; */

	it668x->recive_hdmi_mode = (temp & 0x04) >> 2;

	i2c_read_reg(hdmitx, REG_HDMITX_AVI_COLOR, &temp);

	it668x->pixel_repeat = (temp & 0x30) >> 4;

	it668x->input_color_mode = (temp & 0xC0) >> 6;

	tmdsclk = it668x_hdmitx_cal_pclk(it668x);

	if (tmdsclk > 100000)
		input_hight_speed = true;
	/* clk = clk*(it668x->pixel_repeat +1); */

#ifdef MHL_2X_3D
	it668x_config_3d_output(it668x);
#endif

	packet_mode = false;

	if (it668x->force_color_mode == false)
		it668x->output_color_mode = it668x->input_color_mode;

	if (it668x->sink_support_packet_pixel_mode == 1) {

		if (it668x->auto_packet_pixel_mode == FORCE_PACKET_PIXEL_MODE) {
			it668x->output_color_mode = COLOR_YCbCr422;
			packet_mode = true;
		} else {
			if (it668x->auto_packet_pixel_mode == AUTO_PACKET_PIXEL_MODE) {
				if (input_hight_speed) {

					it668x->output_color_mode = COLOR_YCbCr422;
					packet_mode = true;

				}
			}
		}
	}

	temp = (it668x->output_color_mode & 0x03) << 6;

	i2c_set_reg(hdmitx, REG_HDMITX_OUT_COLOR, M_OUT_COLOR, temp);

	it668x->color_ycbcr_coef = YCBCR_COEF_ITU709;

	/* check 480P and 576P */
	if (tmdsclk > 26000)
		it668x->color_ycbcr_coef = YCBCR_COEF_ITU601;

	it668x_hdmitx_set_color_scale(it668x);

	MHL_VERB_LOG("[it668x]input_color_mode = %2.2X\n", (int)it668x->input_color_mode);
	MHL_VERB_LOG("[it668x]output_color_mode = %2.2X\n", (int)it668x->output_color_mode);
	MHL_VERB_LOG("[it668x]pixel_repeat = %2.2X\n", (int)it668x->pixel_repeat);
	MHL_VERB_LOG("[it668x]input_hight_speed = %2.2X\n", (int)input_hight_speed);

	it668x->mhl_active_link_mode &= 0xFC;

	if (packet_mode == true) {
		hclk = tmdsclk * 2;

		it668x->mhl_active_link_mode |= MHL_STATUS_CLK_PACKED_PIXEL;

		i2c_set_reg(mhl, REG_MHL_CONTROL, B_PACKET_PIXEL_MODE, B_PACKET_PIXEL_MODE);

		MHL_VERB_LOG("[it668x]PACKET PIXEL MODE\n");

	} else {
		hclk = tmdsclk * 3;
		it668x->mhl_active_link_mode |= MHL_STATUS_CLK_NORMAL;

		i2c_set_reg(mhl, REG_MHL_CONTROL, B_PACKET_PIXEL_MODE, 0x00);

		MHL_VERB_LOG("[it668x]NORMAL PIXEL MODE\n");

	}

	it668x_set_hdmitx_afe(it668x, hclk, tmdsclk);
	/* tirgger hw set path_en and clock mode */
	i2c_set_reg(mhl, REG_MHL_PATHEN_CTROL, B_SET_PATHEN | B_CLR_PATHEN, B_SET_PATHEN);
	/* mhl_cbus_Write_state_int_lock(it668x, CBUS_MHL_STATUS_OFFSET_1, it668x->mhl_active_link_mode); */
	/* mhl_wite_int_sts(it668x,CBUS_MHL_STATUS_OFFSET_1,it668x->mhl_active_link_mode); */

	return ret;
}

static int it668x_hdmitx_video_state(struct it668x_data *it668x, enum hdmi_video_state state)
{

	int ret = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	if (it668x->video_state != state) {
		if (state != HDMI_VIDEO_ON)
			it668x_hdmitx_hdcp_abort(it668x);

		switch (state) {
		case HDMI_VIDEO_REST:
			MHL_DEF_LOG(" Hdmi_Video_state ==> HDMI_Video_REST\n");

			i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_VIDEO, B_REST_VIDEO);


			i2c_write_reg(hdmitx, REG_HDMITX_INT_06_MASK, 0xFF);

			i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, B_EN_AVMUTE);

			break;
		case HDMI_VIDEO_WAIT:

			MHL_DEF_LOG(" Hdmi_Video_state ==> HDMI_Video_WAIT\n");

			i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_VIDEO, B_REST_VIDEO);

			i2c_set_reg(hdmitx, REG_HDMITX_INT_06_MASK,
				    B_MASK_VIDEO_STABLE | B_MASK_VIDEO_UNSTABLE |
				    B_MASK_INT_3D_PKT_PRESENT | B_MASK_INT_3D_PKT_LOSS, 0x00);

			i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, B_EN_AVMUTE);
			i2c_set_reg(hdmitx, REG_HDMITX_SWREST, B_REST_VIDEO, 0x00);

			break;
		case HDMI_VIDEO_ON:
			MHL_DEF_LOG(" Hdmi_Video_state ==> HDMI_Video_ON\n");


			if (it668x->hdcp_enable == true) {
				i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE,
					    B_EN_AVMUTE);
			} else {
				i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, 0x00);
			}

			/* it668x_fire_afe(it668x,true); */
			break;
		default:
			break;
		}
		it668x->video_state = state;
	}

	return ret;

}

static int it668x_hdmirx_Terminator_Off(struct it668x_data *it668x)
{
	unsigned char temp;
	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	i2c_read_reg(hdmirx, REG_HDMIRX_TMDS_POW, &temp);
	temp |= SET_RXTMDS_OFF;

	ret = i2c_write_reg(hdmirx, REG_HDMIRX_TMDS_POW, temp);

	MHL_HDMI_LOG(" hdmirx_Terminator_Off(),reg0A=%02X\n", (int)temp);

	return ret;
}

static int it668x_hdmirx_Terminator_On(struct it668x_data *it668x)
{

	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;
	unsigned char temp;
	i2c_read_reg(hdmirx, REG_HDMIRX_TMDS_POW, &temp);

	temp &= ~SET_RXTMDS_OFF;

	ret = i2c_write_reg(hdmirx, REG_HDMIRX_TMDS_POW, 0x00);

	MHL_HDMI_LOG(" hdmirx_Terminator_On(),reg0A=%02X\n", (int)temp);

	return ret;

}

static int it668x_hdmirx_hpd_low(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	i2c_set_reg(hdmirx, REG_HDMIRX_HPD_CTRL, M_SET_HPD, SET_HPD_LOW);

	MHL_DEF_LOG("[it668x] hdmirx_hpd_low()\n");

	it668x_hdmirx_Terminator_Off(it668x);

	return ret;
}

static int it668x_hdmirx_hpd_high(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	i2c_set_reg(hdmirx, REG_HDMIRX_HPD_CTRL, M_SET_HPD, SET_HPD_HIGH);
	MHL_DEF_LOG("[it668x] hdmirx_hpd_high()\n");

	it668x_hdmirx_Terminator_On(it668x);

	return ret;
}


static int it668x_config_ddc_bypass(struct it668x_data *it668x, unsigned int ddc_bypass)
{

	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	unsigned char temp;

	MHL_VERB_LOG(" it668x_config_ddc_bypass(%d)\n", ddc_bypass);

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0x00);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	if (ddc_bypass == true) {
		i2c_set_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, M_DDC_PATH_CTRL, 0x03);
		/* i2c_set_reg(hdmitx, 0xE0, 0x38, 0x30); */
		i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_RXDDC_TO_CBUS, B_EN_RXDDC_TO_CBUS);

	} else {
		i2c_set_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, M_DDC_PATH_CTRL, 0x02);
		/* i2c_set_reg(hdmitx, 0xE0, 0x38, 0x38); */
		i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_RXDDC_TO_CBUS, 0x00);

	}


	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);


	i2c_read_reg(hdmitx, REG_HDMITX_PATH_CTRL, &temp);
	MHL_VERB_LOG(" mhl set ddc  path   0xE0 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, &temp);
	MHL_VERB_LOG(" mhl set ddc  path   0xE5 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_RB_MHL_DATA_PATH, &temp);
	MHL_VERB_LOG(" mhl set data path   0xE6 = %2.2X\n", (int)(temp));



	return ret;

}

static int it668x_config_data_path(struct it668x_data *it668x, unsigned int packet_regen)
{

	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	unsigned char temp;

	MHL_VERB_LOG(" it668x_config_data_path(%d)\n", packet_regen);

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0x00);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);

	if (packet_regen == 0) {
		/* bypass control pkt */
		i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL,
			    B_EN_REPEAT_CTROL_PKT | B_EN_TX_GEN_CTROL_PKT, 0x00);

		i2c_set_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, B_EN_AUTO_TXSRC | M_MHL_DATA_PATH,
			    MHL_BYPASS_HDCP);

	} else {
		/* gen control packet */
		i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL,
			    B_EN_REPEAT_CTROL_PKT | B_EN_TX_GEN_CTROL_PKT,
			    B_EN_REPEAT_CTROL_PKT | B_EN_TX_GEN_CTROL_PKT);

		i2c_set_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, B_EN_AUTO_TXSRC | M_MHL_DATA_PATH,
			    MHL_TX_CTRL_HDCP);
	}

	i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);

	i2c_read_reg(hdmitx, REG_HDMITX_PATH_CTRL, &temp);
	MHL_VERB_LOG(" mhl set ddc  path   0xE0 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, &temp);
	MHL_VERB_LOG(" mhl set ddc  path   0xE5 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_RB_MHL_DATA_PATH, &temp);
	MHL_VERB_LOG(" mhl set data path   0xE6 = %2.2X\n", (int)(temp));

	return ret;
}

static int it668x_mhl_config_path(struct it668x_data *it668x, unsigned int ddc_bypass)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	unsigned char temp;
	unsigned int regen = 0;

	it668x_config_ddc_bypass(it668x, ddc_bypass);

	if ((it668x->hdcp_enable) ||
	    ((it668x->
	      mhl_active_link_mode & (MHL_STATUS_CLK_PACKED_PIXEL | MHL_STATUS_CLK_NORMAL)) ==
	     MHL_STATUS_CLK_PACKED_PIXEL) || (it668x->color_dyn_range == COLOR_RANGE_CEA)
	    || (it668x->input_color_mode != it668x->output_color_mode)) {

		MHL_VERB_LOG("hdcp_enable = %d\n", it668x->hdcp_enable);
		MHL_VERB_LOG("mhl_active_link_mode = 0x%02x\n", it668x->mhl_active_link_mode);
		MHL_VERB_LOG("color_dyn_range = %d\n", it668x->color_dyn_range);
		MHL_VERB_LOG("input_color_mode = %d\n", it668x->input_color_mode);
		MHL_VERB_LOG("output_color_mode = %d\n", it668x->output_color_mode);

		regen = 1;
	}

	it668x_config_data_path(it668x, regen);

	/* i2c_set_reg(hdmitx, 0xE4, 0x08, 0x08); //move to init m */

	i2c_read_reg(hdmitx, REG_HDMITX_PATH_CTRL, &temp);
	MHL_DEF_LOG(" mhl set ddc  path   0xE0 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_MHL_DATA_PATH, &temp);
	MHL_DEF_LOG(" mhl set ddc  path   0xE5 = %2.2X\n", (int)(temp));

	i2c_read_reg(hdmitx, REG_HDMITX_RB_MHL_DATA_PATH, &temp);
	MHL_DEF_LOG(" mhl set data path   0xE6 = %2.2X\n", (int)(temp));

	return ret;
}

static void it668x_init_var(struct it668x_data *it668x)
{

	it668x->pixel_repeat = 0;
	it668x->color_dyn_range = COLOR_RANGE_CEA;
	it668x->color_ycbcr_coef = YCBCR_COEF_ITU709;
	it668x->input_color_mode = COLOR_RGB444;
	it668x->output_color_mode = COLOR_RGB444;

	it668x->enable_3d = false;

	it668x->rclk_div = 0;

	it668x->auto_packet_pixel_mode = AUTO_PACKET_PIXEL_MODE;

#if DEV_POW_SUPPLY
	it668x->mhl_pow_support = true;
#else
	it668x->mhl_pow_support = false;
#endif

	it668x->mhl_pow_en = false;

	it668x->force_color_mode = false;

#ifdef MHL_HDCP_EN
	it668x->hdcp_enable = true;
#else
	it668x->hdcp_enable = false;
#endif

#ifdef EN_DDC_BYPASS
	it668x->ddc_bypass = true;
#else
	it668x->ddc_bypass = false;
#endif

	it668x->use_internal_edid_before_edid_ready = true;

}

static int it668x_hdmirx_ini(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	/* i2c_write_reg(hdmirx, REG_HDMIRX_TX_ADDRESS, IT668x_HDMITX_ADDR); */
	i2c_write_reg(hdmirx, REG_HDMIRX_TX_ADDRESS, IT668x_HDMITX_ADDR | 0x01);

	i2c_set_reg(hdmirx, REG_HDMIRX_CLK_CTRL, B_EN_GVCLK, 0x00);

	i2c_set_reg(hdmirx, REG_HDMIRX_RESET, 0xC1, 0xFD);
	/*  */
	i2c_set_reg(hdmirx, 0x2D, 0x0C, 0x0C);
	i2c_write_reg(hdmirx, 0x2F, 0x8F);

	/* IT668X RXHPD IO select */
	i2c_set_reg(hdmirx, REG_HDMIRX_HPD_CTRL, 0xF0, SET_HPD_LOW);	/* HPD SW control and set to low */
	i2c_set_reg(hdmirx, REG_HDMIRX_IRQ0_MASK, 0x14, 0x04);	/* disable ECC err irq and clock change irq */
	i2c_set_reg(hdmirx, REG_HDMIRX_RESET, 0x01, 0x00);
	/* interrupt pin type and polarity */
	/* push_pull ,high active */
	i2c_set_reg(hdmirx, REG_HDMIRX_IO_CONFIG,
	B_INT_IO_TYPE | B_INT_ACT_POPARITY, IO_INT_PUSH_PULL | IO_INT_ACT_LOW);
	/* i2c_set_reg(hdmirx, 0x0E, 0x60, 0x60); */

	it668x_hdmirx_hpd_low(it668x);

	return ret;
}

static int it668x_rest(struct it668x_data *it668x)
{
	int ret = 0;

	ret = it668x_hdmirx_ini(it668x);

	if (ret < 0)
		return ret;

	ret = it668x_hdmitx_reg_init(it668x);
	if (ret < 0)
		return ret;

	ret = it668x_mhl_config_rclk(it668x);
	if (ret < 0)
		return ret;

	ret = it668x_mhl_reg_init(it668x);

	if (ret < 0)
		return ret;

	ret = it668x_mhl_config_path(it668x, it668x->ddc_bypass);

	if (ret < 0)
		return ret;

	if (1 == it668x_is_usb_mode(it668x))
		ret = it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT);
	else
		ret = it668x_mhl_switch_state(it668x, MHL_CBUS_START);

	if (ret < 0)
		return ret;

	ret = it668x_hdmitx_video_state(it668x, HDMI_VIDEO_REST);
	if (ret < 0)
		return ret;


	return ret;
}

static int it668x_initial_chip(struct it668x_data *it668x)
{
	int ret = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	unsigned char vender_id[2] = { 0xFF, 0xFF }, device_id[2] = {
	0xFF, 0xFF};
	unsigned char temp;

	MHL_DEF_LOG("it668x_initial_chip\n");

	it668x_hw_reset();

	i2c_write_reg(hdmirx, REG_HDMIRX_TX_ADDRESS, IT668x_HDMITX_ADDR);
	i2c_write_reg(hdmirx, REG_HDMIRX_TX_ADDRESS, IT668x_HDMITX_ADDR | 0x01);

	i2c_read_reg(hdmirx, REG_VENDER_ID_L, &vender_id[0]);
	i2c_read_reg(hdmirx, REG_VENDER_ID_H, &vender_id[1]);

	i2c_read_reg(hdmirx, REG_DEVICE_ID_L, &device_id[0]);
	i2c_read_reg(hdmirx, REG_VEVICE_ID_H, &device_id[1]);

	i2c_read_reg(hdmirx, REG_HDMIRX_VERSION, &temp);
	it668x->ver = (unsigned int)temp;

	MHL_VERB_LOG(" Current DevID=%X%X\n", (int)vender_id[1], (int)vender_id[0]);
	MHL_VERB_LOG(" Current VenID=%X%X\n", (int)device_id[1], (int)device_id[0]);

	if ((vender_id[0] == 0x54) && (vender_id[1] == 0x49) && (device_id[0] == 0x81)
	    && (device_id[1] == 0x66)) {

		MHL_VERB_LOG(" ###############################################\n");
		MHL_VERB_LOG(" #            MHL Initialization               #\n");
		MHL_VERB_LOG(" ###############################################\n");
		it668x_init_var(it668x);
		i2c_read_reg(hdmitx, REG_VEVICE_ID_H, &temp);
		it668x->tx_ver = (temp & 0xF0) >> 4;

		MHL_DEF_LOG("  Ver = %2.2X , tx_ver = %2.2X\n", (int)it668x->ver, it668x->tx_ver);
		ret = it668x_rest(it668x);
	} else {
		MHL_VERB_LOG("  ERROR: Can not find IT668xA0 MHLTX Device !!!\n");
		ret = -1;
	}

	return ret;
}

static int it668x_hdmirx_irq(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;
	unsigned char rx_clk_stable = 0;
	unsigned char rx_clk_vaild = 0;
	unsigned char reg05;
	unsigned char reg06;
	int force_cdr_reset = false;

	i2c_read_reg(hdmirx, REG_HDMIRX_IRQ0, &reg05);
	i2c_write_reg(hdmirx, REG_HDMIRX_IRQ0, reg05);

	i2c_read_reg(hdmirx, REG_HDMIRX_INPUT_STS, &reg06);

	MHL_HDMI_LOG(" rx_irq %02x\n", (int)reg05);

	/* emily says: */
	/* When PCLK> 80 Mz,  ignore RxCLKChg_Det interrupt. */
	/* if(RX reg06, D[4]==1) , ignore (RX reg05, D[4]) */
	if (reg06 & B_RX_CLK_SPEED)
		reg05 &= ~INT_CLK_CHG_DETECT;

	if (reg05 & INT_PWR5V_CHG) {
		MHL_HDMI_LOG(" PWR5V change\n");
		if (reg06 & B_RX_PWR5V_ON)
			MHL_HDMI_LOG(" PWR5V is ON\n");
		else
			MHL_HDMI_LOG(" PWR5V is OFF\n");
	}

	if (reg05 & INT_HDMI_MODE_CHG) {

		if (reg06 & B_RX_HDMI_MODE) {
			it668x->recive_hdmi_mode = true;
			MHL_DEF_LOG(" HDMI/DVI mode change to : HDMI mode\n");
		} else {
			it668x->recive_hdmi_mode = false;
			MHL_DEF_LOG(" HDMI/DVI mode change to : DVI mode\n");
		}
	}

	if (reg05 & 0x04)
		MHL_HDMI_LOG(" HDMI RX ECC Error !!\n");
	/* if( reg05 & 0x20 ){ */
	/* MHL_HDMI_LOG(" HDMI RX TimerInt\n"); */
	/* } */

	/* if( reg05 & 0x40 ){ */
	/* MHL_HDMI_LOG(" HDMI RX AutoEQ update\n"); */
	/* } */

	if (reg05 & (INT_CLK_STABLE_CHG | INT_CLK_CHG_DETECT)) {

		if (reg05 & INT_CLK_STABLE_CHG)
			MHL_HDMI_LOG(" HDMI RX RX clock stable change\n");
		if (reg05 & INT_CLK_CHG_DETECT)
			MHL_HDMI_LOG(" HDMI RX clock change detected\n");

		rx_clk_stable = (reg06 & B_RX_CLK_STABLE) >> 6;
		rx_clk_vaild = (reg06 & B_RX_CLK_VAILD) >> 3;

		if (rx_clk_vaild)
			MHL_HDMI_LOG(" Clock is valid, ");
		else
			MHL_HDMI_LOG(" Clock is NOT valid,");

		if (rx_clk_stable)
			MHL_HDMI_LOG(" Clock is stable\n");
		else
			MHL_HDMI_LOG(" Clock is NOT stable\n");

		if ((rx_clk_stable == 0) || (rx_clk_vaild == 0))
			force_cdr_reset = true;
	}

	if (((reg06 & B_RX_HDMI_MODE) == 0) && (rx_clk_stable == 1) && (rx_clk_vaild == 1)) {
		MHL_DEF_LOG(" RxCLK correct, but DVI mode detected !\n");
		force_cdr_reset = true;

	}

	if (force_cdr_reset == true) {

		MHL_DEF_LOG(" force_cdr_reset\n");

		it668x->enable_3d = false;
		it668x_hdmitx_video_state(it668x, HDMI_VIDEO_REST);
		it668x_hdmirx_cdrest(it668x);

	} else {

		MHL_VERB_LOG(" rx_clk_stable change =%d, it668x->hdmi_hpd_rxsen=%d ", rx_clk_stable,
			     it668x->hdmi_hpd_rxsen);
		if ((rx_clk_stable == true) && (it668x->hdmi_hpd_rxsen != 0))
			it668x_hdmitx_video_state(it668x, HDMI_VIDEO_WAIT);
	}

	return ret;
}

static int it668x_hdmitx_video_audio_irq(struct it668x_data *it668x)
{
	int ret = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	unsigned char hdmitx_reg06, hdmitx_reg07, hdmitx_reg08;
	unsigned char rd_reg;
	unsigned char temp;

	i2c_read_reg(hdmitx, REG_HDMITX_INT_06, &hdmitx_reg06);
	i2c_read_reg(hdmitx, REG_HDMITX_INT_07, &hdmitx_reg07);
	i2c_read_reg(hdmitx, REG_HDMITX_INT_08, &hdmitx_reg08);

	i2c_write_reg(hdmitx, REG_HDMITX_INT_06, hdmitx_reg06);
	i2c_write_reg(hdmitx, REG_HDMITX_INT_07, hdmitx_reg07);
	i2c_write_reg(hdmitx, REG_HDMITX_INT_08, hdmitx_reg08);

	MHL_VERB_LOG(" hdmitx irq: reg06=%2.2X,reg07=%2.2X,reg08=%2.2X\n", hdmitx_reg06,
		     hdmitx_reg07, hdmitx_reg08);

	if (hdmitx_reg06 & (INT_3D_PKT_PRESENT | INT_3D_PKT_LOSS)) {
		if (hdmitx_reg06 & INT_3D_PKT_PRESENT) {

			MHL_INT_LOG(" INT_3D_PKT_PRESENT\n");

			it668x->enable_3d = true;
		}

		if (hdmitx_reg06 & INT_3D_PKT_LOSS) {

			MHL_INT_LOG(" INT_3D_PKT_LOSS\n");
			it668x->enable_3d = false;
		}

		if (it668x->video_state == HDMI_VIDEO_ON)
			it668x_mhl_avi_new_event(it668x);
	}

	if ((hdmitx_reg06 & INT_VIDEO_UNSTABLE) || (hdmitx_reg06 & INT_VIDEO_STABLE)) {

		MHL_INT_LOG(" hdmitx_reg07=0x%02X, hdmitx_reg08=0x%02X\n", (int)hdmitx_reg07,
			    (int)hdmitx_reg08);

		i2c_read_reg(hdmitx, REG_HDMITX_VIDEO_STATUS, &temp);

		if ((temp & B_VIDEO_STABLE) == B_VIDEO_STABLE) {

			MHL_DEF_LOG(" Video Stable On Interrupt ...\n");

			it668x_mhl_avi_new_event(it668x);
		} else {

			it668x_hdmitx_video_state(it668x, HDMI_VIDEO_WAIT);
			it668x_hdmitx_hdcp_abort(it668x);
			MHL_DEF_LOG(" Video Stable Off Interrupt ...\n");
		}
	}

	if (hdmitx_reg07 & (INT_RXSEN | INT_HPD)) {

		MHL_INT_LOG(" HPD/RXsen Change Interrupt\n");

		i2c_read_reg(hdmitx, REG_HDMITX_VIDEO_STATUS, &rd_reg);
		rd_reg = (rd_reg & (B_HPD_DETECT | B_RXSEN_DETECT)) >> 5;

		MHL_DEF_LOG(" HPD Change Interrupt HPD/RXsen(it668x->RxSen) ===> %d/%d(%d)!! ",
			    (int)((rd_reg & 0x02) >> 1), (int)(rd_reg & 0x01),
			    it668x->hdmi_hpd_rxsen);

		if ((it668x->hdmi_hpd_rxsen != rd_reg)) {

			if ((rd_reg & 0x03) == 0x03) {
				it668x_hdmitx_hdcp_abort(it668x);
				it668x_mhl_hpd_high_event(it668x);
			} else {

				it668x_hdmirx_hpd_low(it668x);
				it668x_hdmitx_hdcp_abort(it668x);
			}
		}

		it668x->hdmi_hpd_rxsen = (int)rd_reg;
	}

	if (hdmitx_reg07 & INT_DDC_FIFO_ERROR) {

		MHL_INT_LOG(" DDC FIFO Error Interrupt ...\n");

		i2c_read_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, &temp);
		i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, B_DDC_PC_MASTER);
		i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, CMD_DDC_CLEAR_FIFO);
		i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, temp);
	}

	if (hdmitx_reg07 & INT_DDC_NOACK)
		MHL_INT_LOG(" DDC NACK Interrupt ...\n");

	if (hdmitx_reg07 & INT_V2H_FIFO_ERR)
		MHL_INT_LOG(" V2H FIFO Error Interrupt ...\n");

	if (hdmitx_reg07 & INT_VIDEO_PARA_CHG) {

		MHL_INT_LOG(" Video Parameter Change Interrupt ...\n");
/*
	i2c_read_reg(hdmitx , REG_HDMITX_VIDEO_STATUS , &temp);
	//MHL_INT_LOG(" Take Care of the following Readback. Video Stable = %X ...\n", (int)((temp&0x10)>>4));
	i2c_read_reg(hdmitx , REG_HDMITX_VSTABLE_DEBUG , &temp);
	//MHL_INT_LOG(" Video Parameter Change Status hdmitx_reg1d[7:4]= 0x%01X => ", (int)((temp&0xF0)>>4));

	//switch( ((temp&0xF0)>>4) ) {
	//case 0x0: MHL_INT_LOG(" No Video Parameter Change !!!\n"); break;
	//case 0x1: MHL_INT_LOG(" VSync2nd Width Change !!!\n"); break;
	//case 0x2: MHL_INT_LOG(" VSync Width Change !!!\n"); break;
	//case 0x3: MHL_INT_LOG(" VDE2nd End Change !!!\n"); break;
	//case 0x4: MHL_INT_LOG(" VDE2nd Start Change !!!\n"); break;
	//case 0x5: MHL_INT_LOG(" VSync2nd Start Change !!!\n"); break;
	//case 0x6: MHL_INT_LOG(" VDE End Change !!!\n"); break;
	//case 0x7: MHL_INT_LOG(" VDE Start Change !!!\n"); break;
	//case 0x8: MHL_INT_LOG(" Field2nd VRRise Change !!!\n"); break;
	//case 0x9: MHL_INT_LOG(" HSync Width Change !!!\n"); break;
	//case 0xA: MHL_INT_LOG(" HDE End Change !!!\n"); break;
	//case 0xB: MHL_INT_LOG(" HDE Start Change !!!\n"); break;
	//case 0xC: MHL_INT_LOG(" Line Cnt Change !!!\n"); break;
	//case 0xD: MHL_INT_LOG(" Pixel Cnt Change !!!\n"); break;
	//}

	*/
	}

	if (hdmitx_reg08 & INT_HDCP_AUTH_FAIL) {

		MHL_ERR_LOG(" HDCP Authentication Fail Interrupt ...\n\n");

		if (it668x->video_state == HDMI_VIDEO_ON) {
			MHL_VERB_LOG("R0 = 0x");
			i2c_read_reg(hdmitx, 0x41, &temp);
			MHL_VERB_LOG("%2.2X ", (int)temp);
			i2c_read_reg(hdmitx, 0x40, &temp);
			MHL_VERB_LOG("%2.2X ", (int)temp);

			if (it668x->hdcp_fail_time_out > HDCP_FAIL_TIMEOUT) {

				it668x_hdcp_state(it668x, HDCP_CP_FAIL);
				MHL_ERR_LOG
				    (" ERROR: Set Black Screen because of hdcp_fail_time_out>%d !!!\n",
				     it668x->hdcp_fail_time_out);
			} else {
				it668x_hdmitx_avi_hdcp_restart(it668x);
			}
		}
	}

	if (hdmitx_reg08 & INT_HDCP_AUTH_DONE) {
		MHL_DEF_LOG(" HDCP Authentication Done Interrupt ...\n\n");

		if (it668x->video_state == HDMI_VIDEO_ON)
			it668x_hdcp_state(it668x, HDCP_CP_DONE);
	}

	if (hdmitx_reg08 & INT_HDCP_KSVLIST_CHK) {

		MHL_DEF_LOG(" HDCP KSV List Check Interrupt ...\n");

		it668x_hdmitx_hdcp_ksv_chk(it668x);
		/* i2c_set_reg(hdmitx, 0x22, 0x01, 0x01); */
		/* i2c_set_reg(hdmitx, 0x22, 0x03, 0x00); */
	}

	if (hdmitx_reg08 & INT_HDCP_RI_CHK)
		MHL_INT_LOG(" HDCP Ri Check Done Interrupt ...\n");

	if (hdmitx_reg08 & INT_HDCP_PJ_CHK)
		MHL_INT_LOG(" HDCP Pj Check Done Interrupt ...\n");

	if (hdmitx_reg08 & INT_HDCP_SYNC_DETFAIL)
		MHL_INT_LOG(" HDCP 1.2 Sync Detect Fail Interrupt ...\n");

	return ret;
}

/* ////////////////////////////////////////////////////////////////// */
/* void cbus_irq( struct it668x_data *it668x ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_cbus_irq(struct it668x_data *it668x)
{
	unsigned char mhl_reg04, mhl_reg05;
	unsigned char rd_reg;

	int ret = 0;
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	/* struct i2c_client *hdmitx = it668x->pdata->hdmitx_client; */

	ret = i2c_read_reg(mhl, REG_MHL_INT_04, &mhl_reg04);
	if (ret < 0) {
		MHL_ERR_LOG(" i2c read mhl reg04 error ....\n");
		return ret;
	}
	ret = i2c_read_reg(mhl, REG_MHL_INT_05, &mhl_reg05);
	if (ret < 0) {
		MHL_ERR_LOG(" i2c read mhl reg05 error ....\n");
		return ret;
	}

	i2c_write_reg(mhl, REG_MHL_INT_04, mhl_reg04);
	i2c_write_reg(mhl, REG_MHL_INT_05, mhl_reg05);

	MHL_VERB_LOG(": it668x_cbus_irq reg04=%2.2X,reg05=%2.2X\n", mhl_reg04, mhl_reg05);

	if (mhl_reg04 & INT_CBUS_TX_PKT_DONE)
		MHL_INT_LOG(" CBUS Link Layer TX Packet Done Interrupt ...\n");

	if (mhl_reg04 & INT_CBUS_TX_PKT_FAIL) {
		MHL_INT_LOG(" ERROR: CBUS Link Layer TX Packet Fail Interrupt ...\n");

		i2c_read_reg(mhl, REG_CBUS_PKT_FAIL_STATUS, &rd_reg);
		i2c_write_reg(mhl, REG_CBUS_PKT_FAIL_STATUS, rd_reg & 0xF0);

		MHL_INT_LOG("  TX Packet error Status reg15=%X\n", (int)rd_reg);
		/* if( rd_reg&0x10 ) */
		/* MHL_INT_LOG(" TX Packet Fail when Retry > 32 times !!!\n"); */
		/* if( rd_reg&0x20 ) */
		/* MHL_INT_LOG(" TX Packet TimeOut !!!\n"); */
		/* if( rd_reg&0x40 ) */
		/* MHL_INT_LOG(" TX Initiator Arbitration Error !!!\n"); */
		/* if( rd_reg&0x80 ) */
		/* MHL_INT_LOG(" TX CBUS Hang !!!\n"); */

#ifdef MHL_CBUS_FAIL_AUTO_RESTART

		if (it668x->cbus_packet_fail_counter++ > CBUS_COMM_FAIL_TIMEOUT) {
			MHL_INT_LOG
			    (" ERROR: CBUS Link Layer Error ==> Retry CBUS Discovery Process !!!\n");
			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
		}
#endif

	}

	if (mhl_reg04 & INT_CBUS_RX_PKT_DONE)
		MHL_INT_LOG(" CBUS Link Layer RX Packet Done Interrupt ...\n");

	if (mhl_reg04 & INT_CBUS_RX_PKT_FAIL) {

		MHL_INT_LOG(" ERROR: CBUS Link Layer RX Packet Fail Interrupt ...\n");

		i2c_read_reg(mhl, REG_CBUS_PKT_FAIL_STATUS, &rd_reg);
		i2c_write_reg(mhl, REG_CBUS_PKT_FAIL_STATUS, rd_reg & 0x0F);

		MHL_INT_LOG("  TX Packet error Status reg15=%X\n", (int)rd_reg);
		/* if( rd_reg&0x01 ) */
		/* MHL_INT_LOG(" RX Packet Fail !!!\n"); */
		/* if( rd_reg&0x02 ) */
		/* MHL_INT_LOG(" RX Packet TimeOut !!!\n"); */
		/* if( rd_reg&0x04 ) */
		/* MHL_INT_LOG(" RX Parity Check Error !!!\n"); */
		/* if( rd_reg&0x08 ) */
		/* MHL_INT_LOG(" Bi-Phase Error !!!\n"); */

#ifdef MHL_CBUS_FAIL_AUTO_RESTART
		if (it668x->cbus_packet_fail_counter++ > CBUS_COMM_FAIL_TIMEOUT) {
			MHL_INT_LOG
			    (" ERROR: CBUS Link Layer Error ==> Retry CBUS Discovery Process !!!\n");

			/* mhl_cbus_rediscover(); */
			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
		}
#endif
	}

	if (mhl_reg04 & INT_CBUS_MSC_RX_MSG) {
		MHL_INT_LOG(" MSC RX MSC_MSG Interrupt ...\n");

		mhl_read_msc_msg(it668x);

	}

	if (mhl_reg04 & INT_CBUS_WRITE_START)
		MHL_INT_LOG(" MSC RX WRITE_STAT Interrupt ...\n");
	if (mhl_reg04 & INT_CBUS_WRITE_BURST)
		MHL_INT_LOG(" MSC RX WRITE_BURST Interrupt  ...\n");

	if (mhl_reg04 & INT_CBUS_NOT_DETECT) {
		MHL_DEF_LOG(" CBUS Low and VBus5V not Detect Interrupt ... ==> %dth Fail\n",
			    (int)it668x->cbus_detect_timeout);

		if (it668x->mhl_pow_support)
			it668x_turn_on_vbus(it668x, false);

		it668x->cbus_detect_timeout++;
		if (it668x->cbus_detect_timeout > CBUS_NO_5V_1k_TIMEOUT) {

			MHL_DEF_LOG
			    (" ERROR: CBUS Low and VBus5V Detect Error ==> Switch to USB Mode !!!\n");
			it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT);

		}
		/* else{ */
		/* it668x_mhl_switch_state(it668x,MHL_CBUS_START); */
		/* } */
	}

	if (mhl_reg05 & INT_CBUS_MSC_REQ_DONE) {
		MHL_INT_LOG(" MSC Req Done Interrupt ...\n");

		it668x_cbus_cmd_done(it668x);
	}

	if (mhl_reg05 & INT_CBUS_MSC_REQ_FAIL) {
		MHL_INT_LOG(" MSC Req Fail Interrupt (Unexpected) ...\n");

		i2c_read_reg(mhl, REG_MSC_REQ_FAIL_STATUS_L, &rd_reg);
		i2c_write_reg(mhl, REG_MSC_REQ_FAIL_STATUS_L, rd_reg);
		/* rd_reg = mhltxrd(0x18); */
		/* mhltxwr(0x18, rd_reg); */
		MHL_INT_LOG(" MSC Req Fail reg18= %02X\n", (int)rd_reg);

		/* if( rd_reg&0x01 ) */
		/* MHL_ERR_LOG(" ERROR: Incomplete Packet !!!\n"); */
		/* if( rd_reg&0x02 ) */
		/* MHL_ERR_LOG(" ERROR: 100ms TimeOut !!!\n"); */
		/* if( rd_reg&0x04 ) */
		/* MHL_ERR_LOG(" ERROR: Protocol Error !!!\n"); */
		/* if( rd_reg&0x08 ) */
		/* MHL_ERR_LOG(" ERROR: Retry > 32 times !!!\n"); */
		/* if( rd_reg&0x10 ){ */
		/* MHL_ERR_LOG(" ERROR: Receive ABORT Packet !!!\n"); */
		/* it668x->msc_err_abort = true; */
		/* } */


		i2c_read_reg(mhl, REG_MSC_REQ_FAIL_STATUS_H, &rd_reg);
		i2c_write_reg(mhl, REG_MSC_REQ_FAIL_STATUS_H, rd_reg);

		/* if( rd_reg&0x01 ) */
		/* MHL_ERR_LOG(" ERROR: TX FW Fail in the middle of the command sequence !!!\n"); */
		/* if( rd_reg&0x02 ) */
		/* MHL_ERR_LOG(" ERROR: TX Fail because FW mode RxPktFIFO not empty !!!\n"); */

#ifdef MHL_CBUS_FAIL_AUTO_RESTART
		if (it668x->cbus_msc_fail_counter++ > CBUS_COMM_FAIL_TIMEOUT) {
			MHL_ERR_LOG
			    (" ERROR: CBUS MSC Channel Error ==> Retry CBUS Discovery Process !!!\n");

			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
		}
#endif
	}

	if (mhl_reg05 & INT_CBUS_MSC_RPD_DONE)
		MHL_INT_LOG(" MSC Rpd Done Interrupt ...\n");

	if (mhl_reg05 & INT_CBUS_MSC_RPD_FAIL) {

		MHL_ERR_LOG(" MSC Rpd Fail Interrupt ...\n");

		i2c_read_reg(mhl, REG_MSC_RPD_FAIL_STATUS_L, &rd_reg);
		i2c_write_reg(mhl, REG_MSC_RPD_FAIL_STATUS_L, rd_reg);

		MHL_ERR_LOG(" MSC Rpd Fail status reg1A=%X\n", (int)rd_reg);
		/* if( rd_reg&0x01 ) */
		/* MHL_ERR_LOG(" ERROR: Initial Bad Offset !!!\n"); */
		/* if( rd_reg&0x02 ) */
		/* MHL_ERR_LOG(" ERROR: Incremental Bad Offset !!!\n"); */
		/* if( rd_reg&0x04 ) */
		/* MHL_ERR_LOG(" ERROR: Invalid Command !!!\n"); */
		/* if( rd_reg&0x08 ) */
		/* MHL_ERR_LOG(" ERROR: Receive dPacket in Responder Idle State !!!\n"); */
		/* if( rd_reg&0x10 ) */
		/* MHL_ERR_LOG(" ERROR: Incomplete Packet !!!\n"); */
		/* if( rd_reg&0x20 ) */
		/* MHL_ERR_LOG(" ERROR: 100ms TimeOut !!!\n"); */
		/* if( rd_reg&0x40 ) { */
		/* MHL_ERR_LOG(" MSC_MSG Responder Busy ==> Return NACK Packet !!!\n"); */
		/* } */
		/* if( rd_reg&0x80 ) */
		/* MHL_ERR_LOG(" ERROR: Protocol Error !!!\n"); */

		i2c_read_reg(mhl, REG_MSC_RPD_FAIL_STATUS_H, &rd_reg);
		i2c_write_reg(mhl, REG_MSC_RPD_FAIL_STATUS_H, rd_reg);

		MHL_INT_LOG(" MSC Rpd Fail status reg1B=%X\n", (int)rd_reg);
		/* if( rd_reg&0x01 ) */
		/* MHL_ERR_LOG(" ERROR: Retry > 32 times !!!\n"); */
		/* if( rd_reg&0x02 ){ */
		/* MHL_ERR_LOG(" ERROR: Receive ABORT Packet !!!\n"); */
		/* //get_msc_errcode(); */
		/* } */

#ifdef MHL_CBUS_FAIL_AUTO_RESTART
		if (it668x->cbus_msc_fail_counter++ > CBUS_COMM_FAIL_TIMEOUT) {
			MHL_ERR_LOG
			    (" ERROR: CBUS MSC Channel Error ==> Retry CBUS Discovery Process !!!\n");
			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
		}
#endif
	}

	if (mhl_reg05 & INT_CBUS_DDC_REQ_DONE) {
		i2c_read_reg(mhl, 0x1C, &rd_reg);
		MHL_INT_LOG(" DDC Req Done Interrupt (CBUS satatus = %X)...\n", (int)rd_reg);
		if ((rd_reg & 0x01) == 0)
			it668x_ddc_cmd_done(it668x);
	}

	if (mhl_reg05 & INT_CBUS_DDC_REQ_FAIL) {
		MHL_ERR_LOG(" DDC Req Fail Interrupt (Hardware) ...\n");

		i2c_read_reg(mhl, REG_CBUS_DDC_FAIL_STATUS, &rd_reg);
		i2c_write_reg(mhl, REG_CBUS_DDC_FAIL_STATUS, rd_reg);

		MHL_ERR_LOG(" DDC Req Fail reg16=%X\n", (int)rd_reg);

		/* if( rd_reg&0x01 ) */
		/* MHL_ERR_LOG(" ERROR: Retry > 32 times !!!\n"); */
		/* if( rd_reg&0x02 ) */
		/* MHL_ERR_LOG(" ERROR: DDC TimeOut !!!\n"); */
		/* if( rd_reg&0x04 ) */
		/* MHL_ERR_LOG(" ERROR: Receive Wrong Type Packet !!!\n"); */
		/* if( rd_reg&0x08 ) */
		/* MHL_ERR_LOG(" ERROR: Receive Unsupported Packet !!!\n"); */
		/* if( rd_reg&0x10 ) */
		/* MHL_ERR_LOG(" ERROR: Receive Incomplete Packet !!!\n"); */
		/* if( rd_reg&0x20 ) */
		/* MHL_ERR_LOG(" ERROR: Receive ABORT in Idle State !!!\n"); */
		/* if( rd_reg&0x40 ) */
		/* MHL_ERR_LOG(" ERROR: Receive Unexpected Packet!!!\n"); */
		/* if( rd_reg&0x80 ) */
		/* MHL_ERR_LOG(" ERROR: Receive ABORT in non-Idle State !!!\n"); */

#ifdef MHL_CBUS_FAIL_AUTO_RESTART
		if (it668x->cbus_ddc_fail_counter++ > CBUS_COMM_FAIL_TIMEOUT) {
			MHL_ERR_LOG
			    (" ERROR: CBUS DDC Channel Error ==> Retry CBUS Discovery Process !!!\n");
			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
		}
#endif

	}

	return ret;
}

/* ////////////////////////////////////////////////////////////////// */
/* void mhl_device_irq( struct it668x_dev_data *it668x ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_mhl_device_irq(struct it668x_data *it668x)
{
	unsigned char mhl_regA0, mhl_regA1;
	/* unsigned char mhl_regA2, mhl_regA3; */
	unsigned char rd_reg;

	int ret = 0;

	struct i2c_client *mhl = it668x->pdata->mhl_client;
	/* struct i2c_client *hdmitx = it668x->pdata->hdmitx_client; */

	i2c_read_reg(mhl, REG_MHL_INTR_0, &mhl_regA0);
	i2c_read_reg(mhl, REG_MHL_INTR_1, &mhl_regA1);


	i2c_write_reg(mhl, REG_MHL_INTR_0, mhl_regA0);
	i2c_write_reg(mhl, REG_MHL_INTR_1, mhl_regA1);

	if (mhl_regA0 & MHL_INT_DCAP_CHG) {

		i2c_read_reg(mhl, REG_MHL_STATUS_00, &rd_reg);

		MHL_INT_LOG(" MHL Device Capability Change Interrupt , DCAP_READY = %2.2X...\n",
			    (int)rd_reg);

		if (rd_reg & MHL_STATUS_DCAP_READY) {
			mhl_read_devcap(it668x);
			/* it668x_hdmitx_hdcp_abort(it668x); */
			/* it668x_mhl_hpd_high_event(it668x); //re-read edid and process hpd */
		}
	}

	if (mhl_regA0 & MHL_INT_DSCR_CHG)
		MHL_INT_LOG(" MHL DSCR_CHG Interrupt ......\n");

	if (mhl_regA0 & MHL_INT_REQ_WRT) {

		MHL_INT_LOG(" MHL REQ_WRT Interrupt  ...\n");
		/* MHL_INT_LOG(" Set GRT_WRT to 1  ...\n"); */
		mhl_wite_int_sts(it668x, CBUS_MHL_INTR_REG_0, MHL_INT_GRT_WRT);

	}

	if (mhl_regA0 & MHL_INT_GRT_WRT)
		MHL_INT_LOG(" [**]MHL GNT_WRT Interrupt  ...\n");

	if (mhl_regA1 & MHL_INT_EDID_CHG) {
		it668x_hdmitx_hdcp_abort(it668x);
		it668x_mhl_hpd_high_event(it668x);	/* re-read edid and process hpd */
		MHL_INT_LOG(" MHL EDID Change Interrupt ...\n");
	}

	return ret;

}

/* ////////////////////////////////////////////////////////////////// */
/* void cbus_link_irq( struct it668x_dev_data *it668x ) */
/*  */
/* 1.1kdetect->cbus_discovery -> discoverDone (link connected) */
/* 2.connection detect fail -> switch to USB */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_cbus_link_irq(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	/* struct i2c_client *hdmitx = it668x->pdata->hdmitx_client; */

	unsigned char mhl_reg06;
	unsigned char rd_reg;
	unsigned char mhl_status;

	i2c_read_reg(mhl, REG_MHL_INT_06, &mhl_reg06);
	i2c_write_reg(mhl, REG_MHL_INT_06, mhl_reg06);

	if (mhl_reg06 & INT_VBUS_CHANGE) {
		MHL_INT_LOG(" VBUS Status Change Interrupt ...\n");
		i2c_read_reg(mhl, REG_MHL_STATUS, &rd_reg);
		mhl_status = (rd_reg & B_STATUS_VBUS5V) >> 3;

		MHL_DEF_LOG(" Current VBUS Status = %X\n", (int)mhl_status);

		if (mhl_status == 0)
			it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT);

		if ((mhl_status) && (it668x->mhl_state == MHL_LINK_DISCONNECT))
			it668x_mhl_switch_state(it668x, MHL_CBUS_START);
	}

	if (mhl_reg06 & INT_CBUS_1K_DETECT_DONE) {
		MHL_DEF_LOG(" CBUS 1K Detect Done Interrupt ...\n");

		/* it668x_switch_to_mhl(it668x); */

		if (it668x->mhl_pow_support) {

			i2c_read_reg(mhl, REG_MHL_STATUS, &rd_reg);

			if ((rd_reg & B_STATUS_VBUS5V) != B_STATUS_VBUS5V)
				it668x_turn_on_vbus(it668x, true);
		}
		/* it668x_mhl_switch_state(it668x,MHL_CBUS_DISCOVER); */
/* #ifdef CBUS_CTS_OPT */
/* queue_work(it668x->cbus_cmd_wqs, &it668x->cbus_work); */
/* #endif */
	}

	if (mhl_reg06 & (INT_CBUS_1K_DETECT_FAIL | INT_CBUS_DISCOVER_FAIL)) {

		i2c_read_reg(mhl, REG_MHL_STATUS, &rd_reg);

		if ((rd_reg & B_STATUS_CBUS_CONNECT) != B_STATUS_CBUS_CONNECT) {

			if (it668x->mhl_pow_support)
				it668x_turn_on_vbus(it668x, false);

			if (mhl_reg06 & INT_CBUS_1K_DETECT_FAIL) {

				it668x->cbus_1k_det_timeout++;
				MHL_INT_LOG(" CBUS 1K Detect Fail Interrupt ... ==> %dth Fail\n",
					    (int)it668x->cbus_1k_det_timeout);

				/* if(it668x->mhl_pow_support){ */
				/* it668x_turn_on_vbus(it668x,false); */
				/* } */
			}

			if (mhl_reg06 & INT_CBUS_DISCOVER_FAIL) {

				it668x->cbus_discover_timeout++;
				MHL_INT_LOG(" CBUS Discovery Fail Interrupt ... ==> %dth Fail\n",
					    (int)it668x->cbus_discover_timeout);

				/* if(it668x->mhl_pow_support){ */
				/* it668x_turn_on_vbus(it668x,false); */
				/* } */

				it668x_mhl_switch_state(it668x, MHL_CBUS_START);
			}

			if ((it668x->cbus_1k_det_timeout > CBUS_DISCOVER_TIMEOUT) ||
			    (it668x->cbus_discover_timeout > CBUS_DISCOVER_TIMEOUT)) {

				MHL_DEF_LOG(" ERROR: CBUS Detect Error(%2.2X,%2.2X)\n",
					    (int)it668x->cbus_1k_det_timeout,
					    (int)it668x->cbus_discover_timeout);
				MHL_DEF_LOG(" Switch to MHL_LINK_DISCONNECT !!!\n");

				it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT);
			}
			/* else{ */
			/* it668x_mhl_switch_state(it668x, MHL_1K_DETECT); */
			/* } */
		}
	}
/* if( mhl_reg06 & INT_CBUS_1K_DETECT_FAIL ) { */
/*  */
/* it668x->cbus_1k_det_timeout++; */
/* MHL_INT_LOG(" CBUS 1K Detect Fail Interrupt ... ==> %dth Fail\n", (int)it668x->cbus_1k_det_timeout); */
/*  */
/* if(it668x->mhl_pow_support){ */
/* it668x_turn_on_vbus(it668x,false); */
/* } */
/*  */
/*  */
/* if( it668x->cbus_1k_det_timeout>CBUS_DISCOVER_TIMEOUT ) { */
/*  */
/* MHL_ERR_LOG(" ERROR: CBUS 1K Detect Error ==> Switch to USB Mode !!!\n"); */
/*  */
/* it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT); */
/*  */
/* } */
/* else{ */
/* if ( it668x->cbus_1k_det_timeout > 5 ) { */
/* it668x_mhl_switch_state( it668x, MHL_1K_DETECT ); */
/* } */
/* } */
/* } */

	if (mhl_reg06 & INT_CBUS_DISCOVER_DONE) {

		MHL_DEF_LOG(" CBUS Discovery Done Interrupt ...\n");

		it668x_switch_to_mhl(it668x);
		it668x_mhl_switch_state(it668x, MHL_CBUS_CONNECTED);
		mhl_wite_int_sts(it668x, CBUS_MHL_STATUS_OFFSET_0, MHL_STATUS_DCAP_READY);
	}
/* if( mhl_reg06 & INT_CBUS_DISCOVER_FAIL ) { */
/*  */
/* MHL_ERR_LOG(" CBUS Discovery Fail Interrupt ... ==> %dth Fail\n",(int) it668x->cbus_discover_timeout); */
/*  */
/* if(it668x->mhl_pow_support){ */
/* it668x_turn_on_vbus(it668x,false); */
/* } */
/*  */
/* it668x_mhl_switch_state(it668x, MHL_CBUS_START); */
/* } */

	if (mhl_reg06 & INT_MHL_PATHEN_CANGE) {

		MHL_INT_LOG(" CBUS PATH_EN Change Interrupt ...\n");

		i2c_read_reg(mhl, REG_MHL_STATUS_01, &rd_reg);
		/* rd_reg = mhltxrd(0xB1); */
		MHL_INT_LOG(" Current RX PATH_EN status = %d\n", (int)(rd_reg & 0x08));

		if (rd_reg & MHL_STATUS_PATH_ENABLED) {
			/* SET PATH EN */
			/* 20121213 re-work , change write mask from 0x02 to 0x06 */
			i2c_set_reg(mhl, REG_MHL_PATHEN_CTROL, B_SET_PATHEN | B_CLR_PATHEN,
				    B_SET_PATHEN);
			it668x->mhl_active_link_mode |= MHL_STATUS_PATH_ENABLED;

		} else {
			/* CLEAR PATH EN */
			/* 20121213 re-work , change write mask from 0x04 to 0x06 */
			i2c_set_reg(mhl, REG_MHL_PATHEN_CTROL, B_SET_PATHEN | B_CLR_PATHEN,
				    B_CLR_PATHEN);
			it668x->mhl_active_link_mode &= (~MHL_STATUS_PATH_ENABLED);

		}
		/* mhl_wite_int_sts(it668x, CBUS_MHL_STATUS_OFFSET_1, it668x->mhl_active_link_mode); */

	}

	if (mhl_reg06 & INT_MHL_MUTE_CHANGE) {

		MHL_INT_LOG(" CBUS MUTE Change Interrupt ...\n");
		/* MHL_INT_LOG(" Current CBUS MUTE Status = %X\n", (int)(mhltxrd(0xB1)&0x10)>>4); */
	}

	if (mhl_reg06 & INT_DEV_CAP_CHANGE) {
		MHL_INT_LOG(" CBUS DCapRdy Change Interrupt ...\n");

		/* process in mhl device irq */
		/* i2c_read_reg(mhl, REG_MHL_STATUS_00, &rd_reg); */
		/* if(rd_reg & MHL_STATUS_DCAP_READY){ */
		/* mhl_read_devcap(it668x); */
		/* } */
		/* else{ */
		/* MHL_INT_LOG(" DCapRdy Change from '1' to '0'\n"); */
		/* } */
	}
/* if( mhl_reg06 & INT_VBUS_CHANGE ) { */
/*  */
/* MHL_INT_LOG(" VBUS Status Change Interrupt ...\n"); */
/*  */
/* i2c_read_reg(mhl, REG_MHL_STATUS, &rd_reg); */
/*  */
/* mhl_status = (rd_reg & B_STATUS_VBUS5V)>>3; */
/*  */
/* MHL_INT_LOG(" Current VBUS Status = %X\n",(int)mhl_status); */
/*  */
/*  */
/* if(mhl_status == 0) */
/* it668x_mhl_switch_state(it668x, MHL_LINK_DISCONNECT); */
/*  */
/*  */
/* if((mhl_status)&&(it668x->mhl_state == MHL_LINK_DISCONNECT)) */
/* it668x_mhl_switch_state(it668x, MHL_CBUS_START); */
/* } */

	return ret;

}

/* ////////////////////////////////////////////////////////////////// */
/* void hdmitx_irq( struct it668x_dev_data *it668x ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_irq(struct it668x_data *it668x)
{
	unsigned char hdmitx_regF0;
	/* unsigned char rd_reg; */
	/* unsigned int temp; */
	int ret = 0;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	if (it668x->device_powerdown == 1)
		return ret;

	it668x_hdmirx_irq(it668x);

	i2c_read_reg(hdmitx, REG_INT_STATUS, &hdmitx_regF0);

	MHL_VERB_LOG(" it668x_irq .. regF0 = 0x%2.2X\n", hdmitx_regF0);

	if (hdmitx_regF0 & INT_U3_WAKEUP) {

		MHL_INT_LOG(" Detect U3 Wakeup Interrupt ...\n");
		i2c_set_reg(hdmitx, REG_HDMITX_IDDQ_MODE, B_EN_IO_IDDQ | B_IDDQ_OSC_POWDN, 0x00);
		it668x_mhl_switch_state(it668x, MHL_CBUS_START);
	}

	if (hdmitx_regF0 & INT_CBUS) {
		it668x_cbus_link_irq(it668x);
		it668x_cbus_irq(it668x);
	}

	if (hdmitx_regF0 & INT_HDMI)
		it668x_hdmitx_video_audio_irq(it668x);

	if (hdmitx_regF0 & INT_CBUS_SETINT)
		it668x_mhl_device_irq(it668x);

	return ret;
}

static void mhl_dump_mem(void *p, int len)
{
	int i;
	unsigned char *pr = (unsigned char *)p;
	pr_err(": mhl_dump_mem.....\n");

	for (i = 0; i < len; i++, pr++) {
		if ((i % 16) == 0)
			pr_err("\n");

		pr_err("%2.2X  ", *pr);
	}
	pr_err("\n");
}

/* ////////////////////////////////////////////////////////////////// */
/* void ddcwait( void ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_ddcwait_complete(struct it668x_data *it668x)
{
	int cbuswaitcnt;
	/* unsigned char hdmitx_reg07; */
	unsigned char hdmitx_reg0e = 0;
	/* unsigned char mhl_reg04; */
	/* unsigned char mhl_reg05; */
	unsigned char mhl_reg1c = 0;
	int ret = 0;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	cbuswaitcnt = 0;
	do {
		i2c_read_reg(hdmitx, REG_HDMITX_VIDEO_STATUS, &hdmitx_reg0e);
		if ((hdmitx_reg0e & B_HPD_DETECT) != B_HPD_DETECT) {
			MHL_ERR_LOG(" HPD OFF while DDC act...\n");
			ret = -1;
			break;
		}
		cbuswaitcnt++;
		delay1ms(CBUSWAITTIME);
		i2c_read_reg(mhl, REG_MHL_CBUS_STATUS, &mhl_reg1c);

	} while (((mhl_reg1c & 0x01) == 0x01) && (cbuswaitcnt < CBUSWAITNUM));

	/* i2c_read_reg(hdmitx,0x07 , &hdmitx_reg07 ); */
	/* i2c_read_reg(mhl, 0x05 , &mhl_reg05 ); */
	if (cbuswaitcnt == CBUSWAITNUM) {
		MHL_ERR_LOG(" ERROR: DDC Bus Wait TimeOut !!!\n");

		ret = -1;
	}

	return ret;
}

/* ////////////////////////////////////////////////////////////////// */
/* void ddcfire( void ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
static int it668x_ddcfire_cmd(struct it668x_data *it668x, int cmd)
{
	int ret = 0;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */

	MHL_VERB_LOG(" it668x_ddcfire_cmd(%2.2X)\n", (int)cmd);
	i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, cmd);

	ret = it668x_ddcwait_complete(it668x);

	return ret;
}

static void it668x_ddc_cmd_done(struct it668x_data *it668x)
{
#ifndef DDC_READ_USE_BUSY_WAIT
	complete(&it668x->ddc_complete);
#endif

	return;
}

static int mhl_read_edid_block(struct it668x_data *it668x, unsigned char block, void *buffer)
{
	unsigned char offset, readBytes, segment, readLength, i;
	unsigned char *pBuffer = (unsigned char *)buffer;
	int ret = 0;

	unsigned char ddc_config;
	unsigned char mhl_reg16;

	int ddc_cmd;

	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;

	struct i2c_client *mhl = it668x->pdata->mhl_client;

	readBytes = 32;
	segment = block / 2;
	offset = 128 * (block & 0x1);
	readLength = 0;

	MHL_DEF_LOG(" mhl_read_edid_block  [%d]\n", block);

	IT668X_MUTEX_LOCK(&it668x->ddc_lock);
	MHL_VERB_LOG(" mutex_lock(&it668x->ddc_lock);");

	if (it668x->ddc_bypass == true) {
		MHL_VERB_LOG(" ddc path write 0xE0 , bit 3 = (0) ");
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);
		ret = i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_RXDDC_TO_CBUS, 0x00);
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);
		/* if(ret <0) */
		/* goto edid_exit; */
	}

	i2c_read_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, &ddc_config);

	i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, B_DDC_PC_MASTER);
	i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, CMD_DDC_CLEAR_FIFO);
	i2c_write_reg(hdmitx, REG_HDMITX_DDC_HEADER, DDC_HEADER_EDID);
	i2c_write_reg(hdmitx, REG_HDMITX_DDC_BYTE_NUMBER, readBytes);
	i2c_write_reg(hdmitx, REG_HDMITX_DDC_SEGMENT, segment);

	if (segment != 0) {
		ddc_cmd = CMD_DDC_EDID_READ;
		i2c_set_reg(hdmitx, REG_DCC_FIFO_ACCESS_MODE, B_EN_NEW_FIFO_ACCESS, 0x00);
	} else {
		ddc_cmd = CMD_DDC_BURST_READ;
		i2c_set_reg(hdmitx, REG_DCC_FIFO_ACCESS_MODE, B_EN_NEW_FIFO_ACCESS,
			    B_EN_NEW_FIFO_ACCESS);
	}

	MHL_VERB_LOG(" ddc_cmd = %2.2X, segment = %2.2X", (int)ddc_cmd, (int)segment);

	while (readLength < 128) {

		i2c_write_reg(hdmitx, REG_HDMITX_DDC_OFFSET, offset);

#ifdef DDC_READ_USE_BUSY_WAIT
		ret = it668x_ddcfire_cmd(it668x, ddc_cmd);
		if (ret < 0) {
			MHL_VERB_LOG(" it668x_ddcfire_cmd() fail(%X) .....\n", ret);
			break;
		}
#else
		i2c_set_reg(mhl, REG_MHL_INT_05_MASK09, B_MASK_DDC_CMD_DONE, 0x00);
		/* i2c_set_reg(mhl, REG_MHL_INT_05_MASK09 ,B_MASK_DDC_CMD_FAIL ,0x00); */
		/* i2c_set_reg(mhl, REG_MHL_INT_04_MASK08 ,0x0F ,0x00); */
		init_completion(&it668x->ddc_complete);
		i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, ddc_cmd);
		IT668X_MUTEX_UNLOCK(&it668x->lock);
		ret = wait_for_completion_timeout(&it668x->ddc_complete, msecs_to_jiffies(2500));
		IT668X_MUTEX_LOCK(&it668x->lock);
		i2c_set_reg(mhl, REG_MHL_INT_05_MASK09, B_MASK_DDC_CMD_DONE, B_MASK_DDC_CMD_DONE);
		/* i2c_set_reg(mhl, REG_MHL_INT_05_MASK09 ,B_MASK_DDC_CMD_FAIL ,B_MASK_DDC_CMD_FAIL); */
		/* i2c_set_reg(mhl, REG_MHL_INT_04_MASK08 ,0x0F ,0x0F); */
		if (ret < 0) {
			MHL_VERB_LOG(" wait ddc_complete fail(%X) .....\n", ret);
			break;
		}
#endif

		ret = 0;

		i2c_read_reg(mhl, REG_CBUS_DDC_FAIL_STATUS, &mhl_reg16);
		i2c_write_reg(mhl, REG_CBUS_DDC_FAIL_STATUS, mhl_reg16);
		MHL_VERB_LOG(" wait DDC mhlreg16 = %2.2X ", (int)mhl_reg16);

		for (i = 0; i < readBytes; i++) {

			i2c_read_reg(hdmitx, REG_HDMITX_DDC_FIFO, pBuffer);
			/* MHL_VERB_LOG("  0x %2.2x  ", *pBuffer); */

			offset++;
			readLength++;
			pBuffer++;

		}
		/* MHL_VERB_LOG("\n"); */
	}

	i2c_write_reg(hdmitx, REG_HDMITX_DDC_REQUEST, CMD_DDC_CLEAR_FIFO);
	i2c_set_reg(hdmitx, REG_HDMITX_DDC_MASTER_SEL, M_DDC_MASTER, ddc_config);

	if (it668x->ddc_bypass == true) {
		MHL_VERB_LOG(" ddc path write 0xE0 , bit 3 = (1) ");
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_0);
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, UNLOCK_PWD_1);
		i2c_set_reg(hdmitx, REG_HDMITX_PATH_CTRL, B_EN_RXDDC_TO_CBUS, B_EN_RXDDC_TO_CBUS);
		i2c_write_reg(hdmitx, REG_HDMITX_REG_LOCK, 0xFF);
	}

	i2c_set_reg(hdmitx, REG_DCC_FIFO_ACCESS_MODE, B_EN_NEW_FIFO_ACCESS, B_EN_NEW_FIFO_ACCESS);
	IT668X_MUTEX_UNLOCK(&it668x->ddc_lock);
	MHL_VERB_LOG(" mutex_unlock(&it668x->ddc_lock) ret = %d;", ret);

	return ret;
}

static void it668x_edid_check_hdmi_mode(struct it668x_data *it668x, int block)
{
	unsigned char vsdb_block;
	unsigned char *edid;
	int i, ieee_reg;

	/* 3 = VSD */
	vsdb_block = 3;
	i = 0x4;

	edid = it668x->edidbuf + (block * 128);

	for (; i < edid[2]; i += ((edid[i] & 0x1f) + 1)) {
		/* Find vendor specific block */
		if ((edid[i] >> 5) == vsdb_block) {
			ieee_reg = edid[i + 1] | (edid[i + 2] << 8) | edid[i + 3] << 16;
			if (ieee_reg == 0x000c03) {
				MHL_VERB_LOG(" CEA extension find 0x000C03\n");
				it668x->sink_support_hdmi = true;
			}
			break;
		}
	}
}

static int mhl_read_edid(struct it668x_data *it668x)
{
	unsigned char block, block_count;
	unsigned short chksum;
	int ret = 0;

	/* struct i2c_client *hdmitx = it668x->pdata->hdmitx_client; */

	if (0 == mhl_read_edid_block(it668x, 0, it668x->edidbuf)) {
		block_count = it668x->edidbuf[126];
		if (block_count > 0) {
			if (block_count > (IT668X_EDID_MAX_BLOCK - 1)) {
				block_count = (IT668X_EDID_MAX_BLOCK - 1);
				chksum = it668x->edidbuf[127];
				chksum -= it668x->edidbuf[126];
				chksum += block_count;
				it668x->edidbuf[126] = block_count;
				it668x->edidbuf[127] = chksum;
			}

			for (block = 1; block < block_count + 1; block++) {

				if (-1 ==
				    mhl_read_edid_block(it668x, block,
							&it668x->edidbuf[128 * block])) {
					ret = -1;
					break;
				}

				if (it668x->edidbuf[128 * block] == 0x02) {
					MHL_HDMI_LOG("  edid find CEA extension %d\n", (int)block);
					it668x_edid_check_hdmi_mode(it668x, block);
				}
			}
		}
	} else {
		ret = -1;
	}

	it668x->is_edid_ready = (ret == 0) ? true : false;

	return ret;
}

static int mhl_cbus_req_lock(struct it668x_data *it668x, enum mhl_cbus_cmd_reg req_type,
			     unsigned char first_data, unsigned char second_data)
{

	int ret;
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	unsigned char cmd_reg[2];

	cmd_reg[0] = (unsigned char)req_type & 0x00FF;
	cmd_reg[1] = (unsigned char)((req_type & 0xFF00) >> 8);

	IT668X_MUTEX_UNLOCK(&it668x->lock);
	IT668X_MUTEX_LOCK(&it668x->msc_lock);

	/* MHL_VERB_LOG(" mutex_lock(&it668x->msc_lock);"); */
	/* MHL_VERB_LOG(" mhl_cbus_req_lock(%2.2X,%2.2X,%2.2X)\n",req_type,first_data,second_data); */
	/* MHL_VERB_LOG(" cmd_reg[0] = %2.2X, cmd_reg[1] = %2.2X\n",cmd_reg[0],cmd_reg[1]); */
	init_completion(&it668x->cbus_complete);
	i2c_set_reg(mhl, REG_MHL_INT_05, B_MASK_MSC_REQ_DONE, B_MASK_MSC_REQ_DONE);
	i2c_set_reg(mhl, REG_MHL_INT_05_MASK09, B_MASK_MSC_REQ_DONE, 0x00);

	i2c_write_reg(mhl, REG_MHL_MSC_TX_DATA0, first_data);
	i2c_write_reg(mhl, REG_MHL_MSC_TX_DATA1, second_data);

	i2c_write_reg(mhl, REG_MSC_TX_CMD_L, cmd_reg[0]);
	i2c_write_reg(mhl, REG_MSC_TX_CMD_H, cmd_reg[1]);

	/* it668x->msc_err_abort = false; */
	/* IT668X_MUTEX_UNLOCK(&it668x->lock); */

	/* ret = wait_for_completion_timeout(&it668x->cbus_complete,msecs_to_jiffies(2500)); */
	ret = wait_for_completion_timeout(&it668x->cbus_complete, msecs_to_jiffies(2000));

	/* IT668X_MUTEX_LOCK(&it668x->lock); */

	IT668X_MUTEX_UNLOCK(&it668x->msc_lock);
	IT668X_MUTEX_LOCK(&it668x->lock);
	/* MHL_VERB_LOG(" mutex_unlock(&it668x->msc_lock);"); */

	i2c_set_reg(mhl, REG_MHL_INT_05_MASK09, B_MASK_MSC_REQ_DONE, B_MASK_MSC_REQ_DONE);

	return ret ? 0 : -EIO;

}

static int mhl_cbus_msg_send_lock(struct it668x_data *it668x, unsigned char sub_code,
				  unsigned char sub_data)
{
	return mhl_cbus_req_lock(it668x, FIRE_MSC_MSG, sub_code, sub_data);
}

static int mhl_cbus_Write_state_int_lock(struct it668x_data *it668x, unsigned char sub_code,
					 unsigned char sub_data)
{
	return mhl_cbus_req_lock(it668x, FIRE_WRITE_STAT_INT, sub_code, sub_data);
}

static int mhl_cbus_misc_cmd_lock(struct it668x_data *it668x, enum mhl_cbus_cmd_reg req_type,
				  unsigned char sub_code, unsigned char sub_data)
{
	return mhl_cbus_req_lock(it668x, req_type, sub_code, sub_data);
}

static int mhl_devcap_read_locked(struct it668x_data *it668x, unsigned char offset)
{
	int ret;

	unsigned char value;

	struct i2c_client *mhl = it668x->pdata->mhl_client;

	MHL_VERB_LOG("mhl_devcap_read_locked(%2.2X)\n", offset);

	ret = mhl_cbus_req_lock(it668x, FIRE_READ_DEVCAP, offset, 0);
	if (ret < 0)
		MHL_VERB_LOG("IT668x mhl read devcap %2.2X error\n", offset);

	ret = i2c_read_reg(mhl, REG_MHL_MSC_RXDATA, &value);

	if (ret < 0) {
		MHL_VERB_LOG(" msc rcvd data reg read failing\n");
		return ret;
	}

	return value;
}

static int mhl_queue_cbus_cmd_locked(struct it668x_data *it668x, enum cbus_command command,
				     unsigned char offset, unsigned char data)
{
	struct cbus_data *cbus_cmd;

	MHL_VERB_LOG(" mhl_queue_cbus_cmd_locked(%2.2X ,%2.2X,%2.2X )\n", command, offset, data);

	cbus_cmd = kzalloc(sizeof(struct cbus_data), GFP_KERNEL);
	if (!cbus_cmd) {
		MHL_ERR_LOG(" failed to allocate msc data\n");
		return -ENOMEM;
	}
	cbus_cmd->cmd = command;
	cbus_cmd->offset = offset;
	cbus_cmd->data = data;
	cbus_cmd->use_completion = false;

	list_add_tail(&cbus_cmd->list, &it668x->cbus_data_list);

	queue_msc_work(it668x);

	return 0;
}

static int mhl_queue_devcap_read_locked(struct it668x_data *it668x, unsigned char offset)
{
	struct cbus_data *cbus_cmd;
	int ret = 0;

	cbus_cmd = kzalloc(sizeof(struct cbus_data), GFP_KERNEL);
	if (!cbus_cmd) {
		MHL_ERR_LOG(" failed to allocate msc data\n");
		return -ENOMEM;
	}

	MHL_VERB_LOG(" mhl_queue_devcap_read_locked(%2.2X )\n", offset);


	cbus_cmd->cmd = READ_DEVCAP;
	cbus_cmd->offset = offset;
	cbus_cmd->use_completion = true;
	init_completion(&cbus_cmd->complete);
	list_add_tail(&cbus_cmd->list, &it668x->cbus_data_list);
	queue_msc_work(it668x);

	IT668X_MUTEX_UNLOCK(&it668x->lock);
	ret = wait_for_completion_timeout(&cbus_cmd->complete, msecs_to_jiffies(3000));
	IT668X_MUTEX_LOCK(&it668x->lock);

	if (ret == 0)
		MHL_WARN_LOG(" read devcap:0x%X time out !!\n", offset);
	else
		kfree(cbus_cmd);

	return ret;
}

static void it668x_cbus_cmd_done(struct it668x_data *it668x)
{
	complete(&it668x->cbus_complete);
}

static void mhl_read_devcap(struct it668x_data *it668x)
{
	MHL_VERB_LOG(" mhl_read_devcap()\n");

	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_MHL_VERSION, 0) < 0)
		MHL_ERR_LOG(" MHL_VERSION read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_RESERVED, 0) < 0)
		MHL_ERR_LOG(" MHL_RESERVED read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_ADOPTER_ID_H, 0) < 0)
		MHL_ERR_LOG(" ADOPTER_ID_H read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_ADOPTER_ID_L, 0) < 0)
		MHL_ERR_LOG(" ADOPTER_ID_L read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_DEV_CAT, 0) < 0)
		MHL_ERR_LOG(" DEVCAP_DEV_CAT read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_FEATURE_FLAG, 0) < 0)
		MHL_ERR_LOG(" FEATURE_FLAG read fail\n");
	if (mhl_queue_cbus_cmd_locked(it668x, READ_DEVCAP, MHL_DEVCAP_VID_LINK_MODE, 0) < 0)
		MHL_ERR_LOG(" VID_LINK_MODE read fail\n");

}

static int mhl_wite_int_sts(struct it668x_data *it668x, int offset, int field)
{
	int ret = 0;

	ret = mhl_queue_cbus_cmd_locked(it668x, SET_INT, offset, field);
	if (ret < 0)
		MHL_ERR_LOG(" SET_INT_STATE  fail\n");

	return ret;
}

static void mhl_read_msc_msg(struct it668x_data *it668x)
{

	struct cbus_data *cbus_cmd;
	struct i2c_client *mhl = it668x->pdata->mhl_client;

	unsigned char sub_code;
	unsigned char sub_data;

	i2c_read_reg(mhl, REG_MHL_MSGRX_BYTE0, &sub_code);
	i2c_read_reg(mhl, REG_MHL_MSGRX_BYTE1, &sub_data);

	MHL_VERB_LOG(" mhl_read_msc_msg(%2.2X ,%2.2X)\n", sub_code, sub_data);

	cbus_cmd = kzalloc(sizeof(struct cbus_data), GFP_KERNEL);
	if (!cbus_cmd) {
		MHL_ERR_LOG(" failed to allocate msc data\n");
		return;
	}
	cbus_cmd->cmd = MSC_MSG;
	cbus_cmd->offset = sub_code;
	cbus_cmd->data = sub_data;
	cbus_cmd->use_completion = false;




	list_add_tail(&cbus_cmd->list, &it668x->cbus_data_list);

	queue_msc_work(it668x);

	return;
}

static void mhl_parse_rcpkey(struct it668x_data *it668x, unsigned char rcpcode)
{
	unsigned char sub_code;
	unsigned char sub_data;

	if ((rcpcode < MHL_RCP_NUM_KEYS) && is_key_supported(it668x, (int)rcpcode)) {

		/* * */
		/* TODO: write RCPK */
		sub_code = MSG_RCPK;
		sub_data = rcpcode;

		/* * */
		MHL_MSC_LOG(" Send a RCPK with action code = 0x%02X\n", rcpcode);


		IT668X_MUTEX_LOCK(&it668x->input_lock);
		input_report_key(it668x->rcp_input, it668x->keycode[rcpcode], 1);
		input_report_key(it668x->rcp_input, it668x->keycode[rcpcode], 0);
		input_sync(it668x->rcp_input);
		IT668X_MUTEX_UNLOCK(&it668x->input_lock);

	} else {

		/* * */
		/* TODO: write RCPE */
		sub_code = MSG_RCPE;
		sub_data = rcpcode;
		/* * */
		MHL_MSC_LOG(" Send a RCPE with status code = 0x%02X\n", rcpcode);
	}

	mhl_cbus_msg_send_lock(it668x, sub_code, sub_data);
}

static void mhl_parse_rapkey(struct it668x_data *it668x, unsigned char rapcode)
{
	unsigned char sub_code;
	unsigned char sub_data;

	sub_code = MSG_RAPK;
	sub_data = 0x00;

	switch (rapcode) {
	case RAP_POLL:
		MHL_MSC_LOG(" RAP Poll\n");
		break;
	case RAP_CONTENT_ON:
		MHL_MSC_LOG(" RAP Change to CONTENT_ON state\n");

		it668x_fire_afe(it668x, true);
		break;
	case RAP_CONTENT_OFF:
		MHL_MSC_LOG(" RAP Change to CONTENT_OFF state\n");
		it668x_fire_afe(it668x, false);
		break;
	default:

		sub_data = 0x01;
		MHL_MSC_LOG(" ERROR: Unknown RAP action code 0x%02X !!!\n", sub_data);
		MHL_MSC_LOG(" Send a RAPK with status code = 0x%02X\n", sub_data);
	}

	mhl_cbus_msg_send_lock(it668x, sub_code, sub_data);
}

/* ////////////////////////////////////////////////////////////////// */
/* void read_mscmsg( void ) */
/*  */
/*  */
/*  */
/* ////////////////////////////////////////////////////////////////// */
void mhl_process_mscmsg(struct it668x_data *it668x, unsigned char sub_cmd, unsigned char sub_data)
{
	switch (sub_cmd) {
	case MSG_MSGE:
		MHL_MSC_LOG(" RX MSGE => ");
		switch (sub_data) {
		case 0x00:
			MHL_MSC_LOG(" No Error\n");
			break;
		case 0x01:
			MHL_MSC_LOG(" ERROR: Invalid sub-command code !!!\n");
			break;
		default:
			MHL_MSC_LOG(" ERROR: Unknown MSC_MSG status code 0x%02X !!!\n", sub_cmd);
		}
		break;

	case MSG_RCP:
		mhl_parse_rcpkey(it668x, sub_data);
		break;
	case MSG_RCPK:
		break;
	case MSG_RCPE:
		switch (sub_data) {
		case 0x00:
			MHL_MSC_LOG(" No Error\n");
			break;
		case 0x01:
			MHL_MSC_LOG(" ERROR: Ineffective RCP Key Code !!!\n");
			break;
		case 0x02:
			MHL_MSC_LOG(" Responder Busy ...\n");
			break;
		default:
			MHL_MSC_LOG(" ERROR: Unknown RCP status code !!!\n");
		}
		break;


	case MSG_RAP:
		mhl_parse_rapkey(it668x, sub_data);
		break;
	case MSG_RAPK:
		MHL_MSC_LOG(" RX RAPK  => ");
		switch (sub_data) {
		case 0x00:
			MHL_MSC_LOG(" No Error\n");
			break;
		case 0x01:
			MHL_MSC_LOG(" ERROR: Unrecognized Action Code !!!\n");
			break;
		case 0x02:
			MHL_MSC_LOG(" ERROR: Unsupported Action Code !!!\n");
			break;
		case 0x03:
			MHL_MSC_LOG(" Responder Busy ...\n");
			break;
		default:
			MHL_MSC_LOG(" ERROR: Unknown RAP status code 0x%02X !!!\n", sub_data);
		}
		break;

	case MSG_UCP:
		MHL_MSC_LOG(" recive UCP code %2.2x\n", sub_data);
		mhl_cbus_msg_send_lock(it668x, MSG_UCPK, sub_data);
		break;
	case MSG_UCPK:
		break;
	case MSG_UCPE:
		break;
	default:
		MHL_MSC_LOG(" ERROR: Unknown MSC_MSG sub-command code 0x%02X !!!\n", sub_cmd);
		/* it668x->txmsgdata[0] = MSG_MSGE; */
		/* it668x->txmsgdata[1] = 0x01; */
		/* cbus_send_mscmsg(it668x); */
		mhl_cbus_msg_send_lock(it668x, MSG_MSGE, 0x01);
	}
}

/*
static void mhl_cts_cbus_ctrl(struct work_struct *work)
{

    struct it668x_data *it668x = container_of(work, struct it668x_data, cbus_work);

    //struct it668x_data *it668x = dev_get_drvdata(it668xdev);

    struct i2c_client *mhl = it668x->pdata->mhl_client;

    //unsigned long rclk;
    //unsigned int fix_t10usint;
    //unsigned int fix_t10usflt;

    int cbusdelay = 500;


    MHL_VERB_LOG(" mhl_cts_cbus_ctrl...start !!!!\n");

    mutex_lock(&it668x->lock);
    mutex_lock(&it668x->cbus_lock);




    if(it668x->mhl_state != MHL_CBUS_DISCOVER){

	MHL_ERR_LOG(" mhl_cts_cbus_ctrl...ABORT(0) !!!!\n");
	goto err_exit;
    }


    if(it668x->mhl_pow_en == true)
	cbusdelay = 960;

    mutex_unlock(&it668x->lock);
    wait_event_interruptible_timeout(it668x->cbus_wq,NULL ,msecs_to_jiffies(cbusdelay));
    mutex_lock(&it668x->lock);

    if(it668x->mhl_state != MHL_CBUS_DISCOVER){
	MHL_ERR_LOG(" mhl_cts_cbus_ctrl...ABORT(1) !!!!\n");
	goto err_exit;
    }

    //i2c_write_reg(mhl, REG_MHL_10US_TIME_INTE, (unsigned char) (fix_t10usint&0xFF));
    //i2c_write_reg(mhl ,REG_MHL_10US_TIME_FLOAT, (unsigned char) (((fix_t10usint&0x100)>>1)+fix_t10usflt));

    i2c_write_reg(mhl, REG_MHL_10US_TIME_INTE, 0xCF);

    i2c_write_reg(mhl ,REG_MHL_10US_TIME_FLOAT, 0x70);

    //it668x_debgio_en(0);

err_exit:

    mutex_unlock(&it668x->cbus_lock);
    mutex_unlock(&it668x->lock);

    MHL_ERR_LOG(" mhl_cts_cbus_ctrl...end !!!!\n");
    return;

}
*/

static void it668x_setup_charging(struct it668x_data *it668x)
{
	/* struct i2c_client *mhl = it668x->pdata->mhl_client; */

	if (it668x->mhl_pow_support) {
		if (it668x->mhl_devcap[MHL_DEVCAP_DEV_CAT] & DEV_POW_SUPPLY)
			it668x_turn_on_vbus(it668x, false);
	}

	return;
}

static void mhl_msc_event(struct work_struct *work)
{
	int ret = -1;
	struct cbus_data *data, *next;
	struct it668x_data *it668x = container_of(work, struct it668x_data, msc_work);

	/* struct it668x_data *it668x = dev_get_drvdata(it668xdev); */

	IT668X_MUTEX_LOCK(&it668x->cbus_lock);
	IT668X_MUTEX_LOCK(&it668x->lock);

	list_for_each_entry_safe(data, next, &it668x->cbus_data_list, list) {
		/* * */
		/* TODO: check cbus abort */
		/* if(it668x->msc_err_abort == true){ */
		/* MHL_MSC_LOG("  MSC abort . wait 2s ......\n"); */

		/* mutex_unlock(&it668x->lock); */
		/* wait_event_interruptible_timeout(it668x->cbus_wq,NULL ,msecs_to_jiffies(2000)); */
		/* mutex_lock(&it668x->lock); */
		/* it668x->msc_err_abort =false; */
		/* } */

		/* * */

		MHL_VERB_LOG(" it668x->Ver = %X\n", it668x->ver);

		MHL_VERB_LOG(" mhl_msc_event(%2.2X, %2.2X), it668x->mhl_state =%d\n", data->cmd,
			     data->offset, it668x->mhl_state);

		if (it668x->mhl_state == MHL_CBUS_CONNECTED) {
			switch (data->cmd) {
			case MSC_MSG:
				mhl_process_mscmsg(it668x, data->offset, data->data);
				break;

			case READ_DEVCAP:
				MHL_VERB_LOG(" READ_DEVCAP : 0x%X\n", data->offset);

				ret = mhl_devcap_read_locked(it668x, data->offset);

				if (data->use_completion)
					complete(&data->complete);

				if (ret < 0) {
					MHL_ERR_LOG(" error offset%d\n", data->offset);
					break;
				}

				it668x->mhl_devcap[data->offset] = ret;

				if (data->offset == MHL_DEVCAP_DEV_CAT)
					it668x_setup_charging(it668x);

				ret = 0;
				break;
			case SET_INT:
				/* case WRITE_STAT: */
				/* * */
				/* TODO: SET_INT, WRITE_STAT use same opcode, */
				/* depends on offset value */
				/* * */
				/*  */
				MHL_INFO_LOG(" msc_event: WRITE_INT/SET_INT\n");

				ret =
				mhl_cbus_Write_state_int_lock(it668x, data->offset, data->data);

				if (ret < 0)
					MHL_ERR_LOG(" error STATE_INT req\n");

				if (data->offset == CBUS_MHL_STATUS_OFFSET_0
				&& data->data == MHL_STATUS_DCAP_READY) {
					mhl_cbus_Write_state_int_lock(it668x, CBUS_MHL_INTR_REG_0,
					      MHL_INT_DCAP_CHG);
				}

				break;

			case WRITE_BURST:
				/* TODO: */
				break;

			case GET_STATE:
				ret =
				mhl_cbus_misc_cmd_lock(it668x, FIRE_GET_STATE, data->offset,
				   data->data);
				break;
			case GET_VENDOR_ID:
				ret =
				mhl_cbus_misc_cmd_lock(it668x, FIRE_GET_VENDOR_ID, data->offset,
				   data->data);
			break;

			case GET_MSC_ERR_CODE:
				ret =
				mhl_cbus_misc_cmd_lock(it668x, FIRE_GET_MSC_ERRORCODE,
				   data->offset, data->data);
				break;
			/* case GET_SC3_ERR_CODE: */
			/* case GET_SC1_ERR_CODE: */
			case GET_DDC_ERR_CODE:
				ret =
				mhl_cbus_misc_cmd_lock(it668x, FIRE_GET_DDC_ERRORCODE,
				   data->offset, data->data);
				break;

			default:
				MHL_ERR_LOG(" invalid msc command\n");
				break;
			}
		}

		list_del(&data->list);
		if (!data->use_completion)
			kfree(data);
	}

	IT668X_MUTEX_UNLOCK(&it668x->lock);
	IT668X_MUTEX_UNLOCK(&it668x->cbus_lock);
}

static int it668x_irq_wakeup(void)
{
	MHL_VERB_LOG(" %s()\n", __func__);
	IT668X_DISABLE_IRQ();
	atomic_set(&it668x_irq_event, 1);
	wake_up_interruptible(&it668x_irq_wq);
	return 0;
}

static int it668x_irq_kthread(void *data)
{
	struct it668x_data *it668x = it668x_factory;
	struct sched_param param = {.sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(it668x_irq_wq, atomic_read(&it668x_irq_event));
		atomic_set(&it668x_irq_event, 0);
		MHL_VERB_LOG(" %s()\n", __func__);
		IT668X_MUTEX_LOCK(&it668x->lock);
		it668x_irq(it668x);
		IT668X_MUTEX_UNLOCK(&it668x->lock);
		IT668X_ENABLE_IRQ();
		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int it668x_mhl_detect(void *data)
{

	struct it668x_data *it668x = data;

	if (it668x == NULL)
		return -1;

	for (;;) {

		wait_event_interruptible_timeout(it668x->it668x_wq,
			it668x->mhl_state == MHL_CBUS_CONNECTED, msecs_to_jiffies(2500));	/* 100ms */

		if (kthread_should_stop() || (it668x->mhl_state == MHL_CBUS_CONNECTED))
			break;

	}

	return 0;
}

static bool is_key_supported(struct it668x_data *it668x, int keyindex)
{
	u8 log_dev = DEV_LOGICAL_DEV;

	if (mhl_rcp_keymap[keyindex].key_code != KEY_UNKNOWN &&
	    mhl_rcp_keymap[keyindex].key_code != KEY_RESERVED &&
	    (mhl_rcp_keymap[keyindex].log_dev_type & log_dev))
		return true;
	else
		return false;
}

static int it668x_init_input_devices(struct it668x_data *it668x)
{
	struct input_dev *input;
	int ret;
	u8 i;

	input = input_allocate_device();
	if (!input) {
		MHL_ERR_LOG(" failed to allocate input device\n");
		return -ENOMEM;
	}
	set_bit(EV_KEY, input->evbit);

	for (i = 0; i < MHL_RCP_NUM_KEYS; i++)
		it668x->keycode[i] = mhl_rcp_keymap[i].key_code;

	input->keycode = it668x->keycode;
	input->keycodemax = MHL_RCP_NUM_KEYS;
	input->keycodesize = sizeof(it668x->keycode[0]);
	for (i = 0; i < MHL_RCP_NUM_KEYS; i++) {
		if (is_key_supported(it668x, i))
			set_bit(it668x->keycode[i], input->keybit);
	}

	input->name = "it668x_rcp";
	input->id.bustype = BUS_I2C;
	input_set_drvdata(input, it668x);

	MHL_VERB_LOG(" registering input device\n");
	ret = input_register_device(input);
	if (ret < 0) {
		MHL_ERR_LOG(" failed to register input device\n");
		input_free_device(input);
		return ret;
	}

	IT668X_MUTEX_LOCK(&it668x->input_lock);
	it668x->rcp_input = input;
	IT668X_MUTEX_UNLOCK(&it668x->input_lock);

	return 0;
}


/* //////////////////////////////////////////////////////////////////////// */
/*  debug function                                                      */
/*                                                                      */
/*                                                                      */
/* //////////////////////////////////////////////////////////////////////// */
/*
static int it668x_hdmitx_pwron( struct it668x_data *it668x )
{

int ret = 0;
struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;


i2c_set_reg(hdmitx, 0x0F, 0x78, 0x38);   // PwrOn GRCLK
i2c_set_reg(hdmitx, 0x05, 0x01, 0x00);   // PwrOn PCLK

i2c_set_reg(hdmitx, 0x62, 0x44, 0x00);   // PwrOn XPLL


 // PLL Reset OFF
i2c_set_reg(hdmitx, 0x61, 0x10, 0x00);   // DRV_RST
i2c_set_reg(hdmitx, 0x62, 0x08, 0x08);   // XP_RESETB

return ret;
}
*/
static void it668x_mask_and_clear_all_interrupt(struct it668x_data *it668x)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	i2c_write_reg(hdmirx, 0x0F, 0x00);
	i2c_write_reg(hdmirx, 0x10, 0x00);
	i2c_set_reg(hdmitx, 0x05, 0x20, 0x00);
	i2c_set_reg(mhltx, 0x0E, 0x10, 0x00);
	i2c_set_reg(mhltx, 0xE8, 0x0C, 0x00);

	/* RX Int mask */
	i2c_write_reg(hdmirx, 0x0F, 0x00);
	i2c_write_reg(hdmirx, 0x10, 0x00);

	/* Clear all RX Interrupt */
	i2c_write_reg(hdmirx, 0x05, 0xff);
	i2c_write_reg(hdmirx, 0x3D, 0xff);

	/* TX Int mask */
	i2c_write_reg(hdmitx, 0x09, 0xff);
	i2c_write_reg(hdmitx, 0x0A, 0xff);
	i2c_write_reg(hdmitx, 0x0B, 0xff);

	/* Clear all TX Interrupt */
	i2c_write_reg(hdmitx, 0x06, 0xff);
	i2c_write_reg(hdmitx, 0x07, 0xff);
	i2c_write_reg(hdmitx, 0x08, 0xff);
	i2c_write_reg(hdmitx, 0xEE, 0xff);

	/* MHL Int mask */
	i2c_write_reg(mhltx, 0x08, 0xff);
	i2c_write_reg(mhltx, 0x09, 0xff);
	i2c_write_reg(mhltx, 0x0A, 0xff);
	i2c_write_reg(mhltx, 0x0B, 0xff);

	/* Clear all MHL Interrupt */
	i2c_write_reg(mhltx, 0x04, 0xff);
	i2c_write_reg(mhltx, 0x05, 0xff);
	i2c_write_reg(mhltx, 0x06, 0xff);
	i2c_write_reg(mhltx, 0x07, 0xff);
}

static int it668x_power_down(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	if (1 == it668x_is_usb_mode(it668x)) {
		/*
		   i2c_write_reg(hdmirx, 0x09, 0x01);
		   i2c_write_reg(hdmirx, 0x0a, 0x03);
		   i2c_write_reg(hdmirx, 0x0c, 0xff);
		   i2c_write_reg(hdmirx, 0x0d, 0xff);
		   //hdmirxwr(RXREG_09, 0x01);
		   //hdmirxwr(RXREG_0A, 0x03);
		   //hdmirxwr(RXREG_0C, 0xFF);
		   //hdmirxwr(RXREG_0D, 0xFF);

		   i2c_write_reg(hdmitx, 0xf8, 0xc3);
		   i2c_write_reg(hdmitx, 0xf8, 0xa5);
		   i2c_write_reg(hdmitx, 0x20, 0x80);
		   i2c_write_reg(hdmitx, 0x37, 0x01);
		   i2c_write_reg(hdmitx, 0xf8, 0xff);
		   i2c_write_reg(hdmitx, 0x62, 0x44);
		   i2c_write_reg(hdmitx, 0x61, 0x38);
		   i2c_write_reg(mhltx, REG_MHL_CONTROL, 0x11);
		   i2c_write_reg(hdmirx, 0x0e, 0x7b);

		   //      hdmitxwr(TXREG_F8, 0xC3);
		   //      hdmitxwr(TXREG_F8, 0xA5);
		   //      hdmitxwr(TXREG_20, 0x80);
		   //      hdmitxwr(TXREG_37, 0x01);
		   //      hdmitxwr(TXREG_F8, 0xFF);
		   //      hdmitxwr(TXREG_62, 0x44);
		   //      hdmitxwr(TXREG_61, 0x38);
		   //      mhltxwr(MHLREG_0F, 0x11);
		   //      hdmirxwr(RXREG_0E, 0x7B);
		 */
		MHL_DEF_LOG("power down in USB mode\n");

		it668x_mask_and_clear_all_interrupt(it668x);

		i2c_write_reg(hdmirx, 0x13, 0x10);
		i2c_write_reg(hdmirx, 0x14, 0x40);
		i2c_write_reg(hdmirx, 0x09, 0x01);
		i2c_write_reg(hdmirx, 0x0A, 0x03);
		i2c_write_reg(hdmirx, 0x0C, 0xFF);
		i2c_write_reg(hdmirx, 0x0D, 0xFF);

		i2c_write_reg(hdmitx, 0xf8, 0xc3);
		i2c_write_reg(hdmitx, 0xf8, 0xa5);
		i2c_write_reg(hdmitx, 0x20, 0x80);
		i2c_write_reg(hdmitx, 0x37, 0x01);
		i2c_write_reg(hdmitx, 0x62, 0x44);
		i2c_write_reg(hdmitx, 0x61, 0x38);
		i2c_write_reg(hdmitx, 0x65, 0x00);
		i2c_write_reg(hdmitx, 0x67, 0x00);
		i2c_write_reg(hdmitx, 0xf3, 0x00);

		i2c_write_reg(mhltx, REG_MHL_CONTROL, 0x11);
		i2c_write_reg(hdmirx, 0x0e, 0x7b);

		i2c_write_reg(hdmitx, 0xe8, 0x7c);

	} else {		/* mhl mode */
		MHL_DEF_LOG("power down in MHL mode\n");

		i2c_set_reg(hdmitx, 0x61, 0x38, 0x38);	/* PwrDn DRV */
		i2c_set_reg(hdmitx, 0x62, 0x44, 0x44);	/* PwrDn XPLL */
	}

	return ret;
}

static int it668x_power_down_after_driver_init(struct it668x_data *it668x)
{
	int ret = 0;
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	struct i2c_client *hdmirx = it668x->pdata->hdmirx_client;

	MHL_VERB_LOG(":: %p %p %p\n", hdmirx, hdmitx, mhltx);

	ret = i2c_write_reg(hdmirx, 0x2B, IT668x_HDMITX_ADDR | 0x01);
	if (ret < 0) {
		MHL_ERR_LOG("FAIL config HDMITX address..\n");
		return ret;
	}

	/* CBUS Slave address enable */
	ret = i2c_set_reg(hdmitx, REG_HDMITX_MHL_I2C_EN, 0xFF, IT668x_MHL_ADDR | B_EN_MHL_I2C);
	if (ret < 0) {
		MHL_ERR_LOG("FAIL config MHL address..\n");
		return ret;
	}

	it668x_switch_to_mhl(it668x);
	it668x_cbus_disconnect(it668x);
	it668x_switch_to_usb(it668x, false);	/* to make sure we are in USB mode. */
	ret = it668x_power_down(it668x);

	return ret;
}

static int it668x_i2c_suspend(struct device *dev)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	struct it6681_data *it6681 = i2c_get_clientdata(client);
	struct power_event_data *power_cmd;

	MHL_VERB_LOG("%s:++\n", __func__);

	power_cmd = kzalloc(sizeof(struct power_event_data), GFP_KERNEL);
	if (!power_cmd) {
		MHL_ERR_LOG("it6681: failed to allocate power event data\n");
		power_cmd = &it6681->power_cmd_su;
	}

	power_cmd->event = IT6681_PE_SUSPEND;

	mutex_lock(&it6681->power_event_list_lock);
	list_add_tail(&power_cmd->list, &it6681->power_event_list);
	mutex_unlock(&it6681->power_event_list_lock);

	queue_work(it6681->power_cmd_wqs, &it6681->power_work);

	return 0;
#else
	/* struct i2c_client *client = to_i2c_client(dev); */
	/* struct it6681_data *it6681 = i2c_get_clientdata(client); */

	/* it668x_set_power_down(it6681); */

	return 0;
#endif
}

static int it668x_i2c_resume(struct device *dev)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	struct it6681_data *it6681 = i2c_get_clientdata(client);
	struct power_event_data *power_cmd;

	MHL_VERB_LOG("%s:++\n", __func__);

	power_cmd = kzalloc(sizeof(struct power_event_data), GFP_KERNEL);
	if (!power_cmd) {
		MHL_ERR_LOG("it6681: failed to allocate power event data\n");
		power_cmd = &it6681->power_cmd_re;
	}

	power_cmd->event = IT6681_PE_RESUME;

	mutex_lock(&it6681->power_event_list_lock);
	list_add_tail(&power_cmd->list, &it6681->power_event_list);
	mutex_unlock(&it6681->power_event_list_lock);

	queue_work(it6681->power_cmd_wqs, &it6681->power_work);

	return 0;
#else
	/* struct i2c_client *client = to_i2c_client(dev); */
	/* struct it6681_data *it6681 = i2c_get_clientdata(client); */

	/* it668x_set_power_on(it6681); */

	return 0;
#endif
}

static int it668x_i2c_poweroff(struct it668x_data *it668x)
{
	struct i2c_client *hdmitx = it668x->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x->pdata->mhl_client;
	unsigned char tmp;
	int ret;

	ret = i2c_read_reg(mhltx, REG_MHL_CONTROL, &tmp);
	if (ret < 0)
		MHL_ERR_LOG(" %s() error at line %d", __func__, __LINE__);

	if (0 == (tmp & B_USB_SWITCH_ON)) {	/* if mhl mode, pull down CBUS */

		ret = i2c_set_reg(mhltx, REG_MHL_CONTROL, 0x10, 0x10);
		if (ret < 0)
			MHL_ERR_LOG(" %s() error at line %d", __func__, __LINE__);

		ret = i2c_set_reg(hdmitx, 0x64, 0x80, 0x80);
		if (ret < 0)
			MHL_ERR_LOG(" %s() error at line %d", __func__, __LINE__);
	}

	return ret;
}

static int it668x_power_off_cb(struct notifier_block *nb, unsigned long event, void *unused)
{
	struct it668x_data *it668x = it668x_factory;

	MHL_DEF_LOG(" rebooting cleanly.\n");
	switch (event) {
	case SYS_RESTART:
	case SYS_HALT:
	case SYS_POWER_OFF:
		it668x_i2c_poweroff(it668x);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

#define MHL_EDID_BLOCK_LEN      128
#define MHL_EDID_SIZE 512

static struct notifier_block it668x_reboot_notifier = {
	.notifier_call = it668x_power_off_cb,
};

ssize_t it668x_fop_read(struct file *file, char *buf, size_t count, loff_t *f_ops)
{
	return 0;
}

ssize_t it668x_fop_write(struct file *file, const char *buf, size_t count, loff_t *f_ops)
{
	struct it668x_data *it668x = it668x_factory;
	struct i2c_client *mhl;
	struct i2c_client *hdmitx;
	struct i2c_client *hdmirx;
	unsigned int cmp_len;
	unsigned char cmp_str[32];
	unsigned int val;
	unsigned int vadr_reg_start, vadr_reg_end, vadr_reg_len;
	int ret;
	int i;

	MHL_DBG_LOG("cmd = [%s]\n", buf);

	/* echo dbgtype:0xXXXX > sys/kernel/debug/it668x */
	strncpy(cmp_str, "dbgtype:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x", &val);
		if (ret >= 1) {
			s_mhl_log_on = val;
			MHL_DBG_LOG("s_mhl_log_on => 0x%08x\n", s_mhl_log_on);
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
		goto __leave;
	}

	if (NULL == it668x) {
		MHL_ERR_LOG("leak of it668x data\n");
		goto __leave;
	}

	mhl = it668x->pdata->mhl_client;
	hdmitx = it668x->pdata->hdmitx_client;
	hdmirx = it668x->pdata->hdmirx_client;

	/* status打印 */
	/* Echo status > sys/kernel/debug/it668x */
	strncpy(cmp_str, "status", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("[it668x] ver = %2.2X\n", it668x->ver);
		MHL_DBG_LOG("[it668x] tx_ver = %2.2X\n", it668x->tx_ver);
		MHL_DBG_LOG("[it668x] rclk = %lu\n", it668x->rclk);
		MHL_DBG_LOG("[it668x] oscdiv = %u\n", it668x->oscdiv);

		MHL_DBG_LOG("[it668x] device_powerdown=%d\n", it668x->device_powerdown);

		if (it668x->mhl_state == MHL_USB_PWRDN)
			MHL_DBG_LOG("[it668x] MHL_state = MHL_USB_PWRDN\n");
		else if (it668x->mhl_state == MHL_USB)
			MHL_DBG_LOG("[it668x] MHL_state = MHL_USB\n");
		else if (it668x->mhl_state == MHL_LINK_DISCONNECT)
			MHL_DBG_LOG("[it668x] MHL_state = MHL_LINK_DISCONNECT\n");
		else if (it668x->mhl_state == MHL_CBUS_START)
			MHL_DBG_LOG("[it668x] MHL_state = MHL_CBUS_START\n");
		else if (it668x->mhl_state == MHL_CBUS_CONNECTED)
			MHL_DBG_LOG("[it668x] MHL_state = MHL_CBUS_CONNECTED\n");
		else
			MHL_DBG_LOG("[it668x] MHL_state = MHL_Unknown\n");

		MHL_DBG_LOG("[it668x] input_color_mode = %2.2X\n", (int)it668x->input_color_mode);
		MHL_DBG_LOG("[it668x] output_color_mode = %2.2X\n", (int)it668x->output_color_mode);
		MHL_DBG_LOG("[it668x] auto_packet_pixel_mode = %2.2X\n",
			    (int)it668x->auto_packet_pixel_mode);
		MHL_DBG_LOG("[it668x] hdcp_enable = %2.2X\n", (int)it668x->hdcp_enable);
		MHL_DBG_LOG("[it668x] ddc_bypass = %2.2X\n", (int)it668x->ddc_bypass);
		MHL_DBG_LOG("[it668x] mhl_pow_support = %d\n", (int)it668x->mhl_pow_support);

		MHL_DBG_LOG("[it668x] sink_support_packet_pixel_mode= %2.2X\n",
			    (int)it668x->sink_support_packet_pixel_mode);
		MHL_DBG_LOG("[it668x] sink_support_hdmi = %2.2X\n", (int)it668x->sink_support_hdmi);

		MHL_DBG_LOG
		    ("[it668x] devcap:%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
		     it668x->mhl_devcap[0], it668x->mhl_devcap[1], it668x->mhl_devcap[2],
		     it668x->mhl_devcap[3], it668x->mhl_devcap[4], it668x->mhl_devcap[5],
		     it668x->mhl_devcap[6], it668x->mhl_devcap[7], it668x->mhl_devcap[8],
		     it668x->mhl_devcap[9], it668x->mhl_devcap[10], it668x->mhl_devcap[11],
		     it668x->mhl_devcap[12], it668x->mhl_devcap[13], it668x->mhl_devcap[14],
		     it668x->mhl_devcap[15]);
		MHL_DBG_LOG("[it668x] mhl_active_link_mode = 0x%X\n",
			    (int)it668x->mhl_active_link_mode);
		MHL_DBG_LOG("[it668x] hdcp_cmd = 0x%X\n", (int)it668x->hdcp_cmd);
		MHL_DBG_LOG("[it668x] avi_cmd = 0x%X\n", (int)it668x->avi_cmd);

		MHL_DBG_LOG("[it668x] mhl_pow_en = %d\n", it668x->mhl_pow_en);

		MHL_DBG_LOG("[it668x] hdmi_hpd_rxsen=%d\n", it668x->hdmi_hpd_rxsen);
		MHL_DBG_LOG("[it668x] recive_hdmi_mode = %2.2X\n", (int)it668x->recive_hdmi_mode);
		if (it668x->video_state == HDMI_VIDEO_REST)
			MHL_DBG_LOG("[it668x] video_state=HDMI_VIDEO_REST\n");
		else if (it668x->video_state == HDMI_VIDEO_WAIT)
			MHL_DBG_LOG("[it668x] video_state=HDMI_VIDEO_WAIT\n");
		else if (it668x->video_state == HDMI_VIDEO_ON)
			MHL_DBG_LOG("[it668x] video_state=HDMI_VIDEO_ON\n");
		else
			MHL_DBG_LOG("[it668x] video_state=HDMI_VIDEO_Unknown\n");


		goto __leave;
	}
	/* hdcp on/off */
	/* On:  Echo hdco:on > sys/kernel/debug/it668x */
	/* Off: Echo hdco:off > sys/kernel/debug/it668x */
	strncpy(cmp_str, "hdcp:on", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x->hdcp_enable = true;
		goto __leave;
	}

	strncpy(cmp_str, "edid", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		goto __leave;
	}

	strncpy(cmp_str, "hdcp:off", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x->hdcp_enable = false;
		goto __leave;
	}
	/* reg read, OFFSET偏移地址，LEN?取?度 */
	/* Echo r:rx:OFFSET/LEN > sys/kernel/debug/it668x */
	/* Echo r:tx:OFFSET/LEN > sys/kernel/debug/it668x */
	/* Echo r:mhl:OFFSET/LEN > sys/kernel/debug/it668x */
	strncpy(cmp_str, "r:rx:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x/%x", &vadr_reg_start, &vadr_reg_len);
		if (ret >= 2) {
			vadr_reg_end = vadr_reg_start + vadr_reg_len - 1;
			for (i = vadr_reg_start; i <= vadr_reg_end; i++) {
				if (i > 0xff) {
					MHL_ERR_LOG("i2c error offset = 0x%02x\n", i);
					break;
				}
				ret = i2c_read_reg(hdmirx, i, (u8 *) &val);
				if (ret != 0)
					MHL_ERR_LOG("i2c reg error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("RX:reg0x%02x = 0x%02x\n", i, (0xFF & val));
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
		goto __leave;
	}
	strncpy(cmp_str, "r:tx:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x/%x", &vadr_reg_start, &vadr_reg_len);
		if (ret >= 2) {
			vadr_reg_end = vadr_reg_start + vadr_reg_len - 1;
			for (i = vadr_reg_start; i <= vadr_reg_end; i++) {
				if (i > 0xff) {
					MHL_ERR_LOG("i2c error offset = 0x%02x\n", i);
					break;
				}
				ret = i2c_read_reg(hdmitx, i, (u8 *) &val);
				if (ret != 0)
					MHL_ERR_LOG("i2c reg error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("TX:reg0x%02x = 0x%02x\n", i, (0xFF & val));
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
		goto __leave;
	}
	strncpy(cmp_str, "r:mhl:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x/%x", &vadr_reg_start, &vadr_reg_len);
		if (ret >= 2) {
			vadr_reg_end = vadr_reg_start + vadr_reg_len - 1;
			for (i = vadr_reg_start; i <= vadr_reg_end; i++) {
				if (i > 0xff) {
					MHL_ERR_LOG("i2c error offset = 0x%02x\n", i);
					break;
				}
				ret = i2c_read_reg(mhl, i, (u8 *) &val);
				if (ret != 0)
					MHL_ERR_LOG("i2c reg error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("mhl:reg0x%02x = 0x%02x\n", i, (0xFF & val));
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
		goto __leave;
	}
	/* reg write, OFFSET偏移地址，DATA?入值 */
	/* Echo w:rx:OFFSET=DATA > sys/kernel/debug/it668x */
	/* Echo w:tx:OFFSET=DATA > sys/kernel/debug/it668x */
	/* Echo w:mhl:OFFSET=DATA > sys/kernel/debug/it668x */
	strncpy(cmp_str, "w:rx:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x=%x", &vadr_reg_start, &val);
		if (ret >= 2) {
			if (vadr_reg_start <= 0xFF) {
				ret = i2c_write_reg(hdmirx, vadr_reg_start, (0xFF & val));
				if (ret != 0)
					MHL_ERR_LOG("i2c reg write error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("RX:reg0x%02x set to 0x%02x\n", vadr_reg_start,
						    (0xFF & val));
			} else {
				MHL_ERR_LOG("i2c error offset = 0x%02x\n", vadr_reg_start);
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
	}
	strncpy(cmp_str, "w:tx:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x=%x", &vadr_reg_start, &val);
		if (ret >= 2) {
			if (vadr_reg_start <= 0xFF) {
				ret = i2c_write_reg(hdmitx, vadr_reg_start, (0xFF & val));
				if (ret != 0)
					MHL_ERR_LOG("i2c reg write error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("TX:reg0x%02x set to 0x%02x\n", vadr_reg_start,
						    (0xFF & val));
			} else {
				MHL_ERR_LOG("i2c error offset = 0x%02x\n", vadr_reg_start);
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
	}
	strncpy(cmp_str, "w:mhl:", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		ret = sscanf(buf + cmp_len, "%x=%x", &vadr_reg_start, &val);
		if (ret >= 2) {
			if (vadr_reg_start <= 0xFF) {
				ret = i2c_write_reg(mhl, vadr_reg_start, (0xFF & val));
				if (ret != 0)
					MHL_ERR_LOG("i2c reg write error, ret = %d\n", ret);
				else
					MHL_DBG_LOG("MHL:reg0x%02x set to 0x%02x\n", vadr_reg_start,
						    (0xFF & val));
			} else {
				MHL_ERR_LOG("i2c error offset = 0x%02x\n", vadr_reg_start);
			}
		} else {
			MHL_ERR_LOG("leak of command arguments\n");
		}
	}
	/* dump all reg */
	/* Echo dump > sys/kernel/debug/it668x */
	strncpy(cmp_str, "dump", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x_dump_register(it668x);
		goto __leave;
	}
	/* force connect */
	/* Echo force:on > sys/kernel/debug/it668x */
	strncpy(cmp_str, "force:on", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("command not supported\n");
		goto __leave;
	}
	/* force disconnect */
	/* Echo force:off > sys/kernel/debug/it668x */
	strncpy(cmp_str, "force:off", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x_cbus_drive_low(it668x);
		goto __leave;
	}
	/* normal connection */
	/* Echo connect:on > sys/kernel/debug/it668x */
	strncpy(cmp_str, "connect:on", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x_cbus_release(it668x);
		goto __leave;
	}
	/* disable cbus connection */
	/* Echo connect:off > sys/kernel/debug/it668x */
	strncpy(cmp_str, "connect:off", sizeof(cmp_str));
	cmp_len = strlen(cmp_str);
	if ((count > cmp_len) && (0 == strncmp(buf, cmp_str, cmp_len))) {
		MHL_DBG_LOG("exec cmd : %s", buf);
		it668x_cbus_drive_low(it668x);
		goto __leave;
	}

__leave:
	return count;
}

ssize_t it668x_fop_open(struct inode *inode, struct file *file)
{
	MHL_INFO_LOG("%s++\n", __func__);
	return 0;
}

ssize_t it668x_fop_release(struct inode *inode, struct file *file)
{
	MHL_INFO_LOG("%s++\n", __func__);
	return 0;
}

static ssize_t it668x_mhl_msg_store(struct class *dev,
				    struct class_attribute *attr, const char *buf, size_t size)
{
	unsigned int sub_code, sub_data;

	if (sscanf(buf, "%X %X ", &sub_code, &sub_data) >= 1) {

		MHL_VERB_LOG(" sub_code = %2.2X\n", sub_code);
		MHL_VERB_LOG(" sub_data = %2.2X\n", sub_data);

		IT668X_MUTEX_LOCK(&it668x_factory->lock);
		mhl_cbus_msg_send_lock(it668x_factory, sub_code, sub_data);
		IT668X_MUTEX_UNLOCK(&it668x_factory->lock);
	}

	return size;
}

static ssize_t it668x_ctrl_color_store(struct class *dev,
				       struct class_attribute *attr, const char *buf, size_t size)
{
	int enable, color;

	if (sscanf(buf, "%X %X ", &enable, &color) >= 1) {

		MHL_VERB_LOG(" sub_code = %2.2X\n", enable);
		MHL_VERB_LOG(" sub_data = %2.2X\n", color);

		MHL_VERB_LOG("it668x_ctrl_color_store(%d,%d)\n", enable, color);

		it668x_factory->force_color_mode = (unsigned char)enable;
		it668x_factory->output_color_mode = (unsigned char)color;
	}

	return size;
}

static ssize_t it668x_devctl_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct it668x_data *it668x = it668x_factory;	/* get_it6681_data(); */
	int size = sizeof(it668x->ioctl_data_buffer);

	MHL_INFO_LOG("%s - buf=%p, len=%u\n", __func__, buf, size);
	memcpy(buf, &it668x->ioctl_data_buffer[0], size);
	return size;
}

static ssize_t it668x_devctl_store(struct class *dev,
				   struct class_attribute *attr, const char *buf, size_t size)
{
	struct it668x_dev_ctl *devctl = (struct it668x_dev_ctl *)buf;
	struct it668x_data *it668x = it668x_factory;	/* get_it6681_data(); */
	struct i2c_client *hdmirx = it668x_factory->pdata->hdmirx_client;
	struct i2c_client *hdmitx = it668x_factory->pdata->hdmitx_client;
	struct i2c_client *mhltx = it668x_factory->pdata->mhl_client;
	unsigned char *data_buf;
	int ppmode;
	static int prev_ppmode = -1;
	int hdcp;
	static int prev_hdcp = -1;
	int i, len;

	MHL_INFO_LOG("%s - buf=%p, len=%zu\n", __func__, buf, size);

	if (devctl == NULL)
		return 0;

	MHL_INFO_LOG("%s - op = 0x%02x\n", __func__, devctl->op);

	switch (devctl->op) {
	case DEV_CTL_READ_REG:
		len = devctl->read_reg.length;
		if (len > sizeof(it668x->ioctl_data_buffer))
			len = sizeof(it668x->ioctl_data_buffer);
		data_buf = (unsigned char *)it668x->ioctl_data_buffer;

		switch (devctl->read_reg.dev) {
		case IT668X_I2C_HDMI_RX:
			for (i = 0; i < len; i++) {
				/* data_buf[i] = hdmirxrd(devctl->read_reg.offset+i); */
				i2c_read_reg(hdmirx, devctl->read_reg.offset + i, &data_buf[i]);
			}
			break;
		case IT668X_I2C_HDMI_TX:
			for (i = 0; i < len; i++) {
				/* data_buf[i] = hdmitxrd(devctl->read_reg.offset+i); */
				i2c_read_reg(hdmitx, devctl->read_reg.offset + i, &data_buf[i]);
			}
			break;
		case IT668X_I2C_MHL_TX:
			for (i = 0; i < len; i++) {
				/* data_buf[i] = mhltxrd(devctl->read_reg.offset+i); */
				i2c_read_reg(mhltx, devctl->read_reg.offset + i, &data_buf[i]);
			}
			break;
		default:
			break;
		}
		break;

	case DEV_CTL_WRITE_REG:
		switch (devctl->write_reg1.dev) {
		case IT668X_I2C_HDMI_RX:
			/* hdmirxwr( devctl->write_reg1.offset, devctl->write_reg1.data ); */
			i2c_write_reg(hdmirx, devctl->write_reg1.offset, devctl->write_reg1.data);
			break;
		case IT668X_I2C_HDMI_TX:
			/* hdmitxwr( devctl->write_reg1.offset, devctl->write_reg1.data ); */
			i2c_write_reg(hdmitx, devctl->write_reg1.offset, devctl->write_reg1.data);
			break;
		case IT668X_I2C_MHL_TX:
			/* mhltxwr( devctl->write_reg1.offset, devctl->write_reg1.data ); */
			i2c_write_reg(mhltx, devctl->write_reg1.offset, devctl->write_reg1.data);
			break;
		default:
			break;
		}
		break;

	case DEV_CTL_SET_REG:
		switch (devctl->set_reg.dev) {
		case IT668X_I2C_HDMI_RX:
			i2c_set_reg(hdmirx, devctl->set_reg.offset, devctl->set_reg.mask,
				    devctl->set_reg.data);
			break;
		case IT668X_I2C_HDMI_TX:
			i2c_set_reg(hdmitx, devctl->set_reg.offset, devctl->set_reg.mask,
				    devctl->set_reg.data);
			break;
		case IT668X_I2C_MHL_TX:
			i2c_set_reg(mhltx, devctl->set_reg.offset, devctl->set_reg.mask,
				    devctl->set_reg.data);
			break;
		default:
			break;
		}
		break;
		break;

	case DEV_CTL_CONTROL:
		switch (devctl->control.sop) {
		case IT668X_CTL_HOLD:
			if (it668x)
				atomic_set(&it668x->mcu_hold, 1);
			break;

		case IT668X_CTL_RELEASE:
			if (it668x) {
				atomic_set(&it668x->mcu_hold, 0);
				wake_up(&it668x->mcu_hold_wq);
			}
			break;

		case IT668X_CTL_PM_S:
#ifdef CONFIG_PM_SLEEP
			it668x_i2c_suspend(it668x->it668x_dev);
#endif
			break;

		case IT668X_CTL_PM_R:
#ifdef CONFIG_PM_SLEEP
			it668x_i2c_resume(it668x->it668x_dev);
#endif
			break;
		}
		break;

	case DEV_CTL_CONTROL_PPMODE:
		ppmode = devctl->ppmode.mode;
		if (prev_ppmode != ppmode) {
			/* it6681->EnPackPix = ppmode; */
			/* it6681->mode_overwrite = 1; */
			it668x->auto_packet_pixel_mode = (unsigned char)ppmode;
			atomic_set(&it668x->fw_reinit, 1);
			prev_ppmode = ppmode;
		}
		break;

	case DEV_CTL_CONTROL_HDCP:
		hdcp = devctl->hdcp.mode;
		if (prev_hdcp != hdcp) {
			/* it6681->HDCPEnable = hdcp; */
			/* it6681->mode_overwrite = 1; */
			atomic_set(&it668x->fw_reinit, 1);
			prev_hdcp = hdcp;
		}
		break;
	default:
		size = 0;
		break;
	}

	return size;
}

static CLASS_ATTR(devctl, 0666, it668x_devctl_show, it668x_devctl_store);
static CLASS_ATTR(mhl_msg, 0664, NULL, it668x_mhl_msg_store);
static CLASS_ATTR(mhl_color, 0664, NULL, it668x_ctrl_color_store);

int it668x_dev_class_init(struct it668x_data *it668x)
{
	int ret;
	struct class *sec_mhl;
	struct i2c_client *client = it668x->pdata->mhl_client;

	sec_mhl = class_create(THIS_MODULE, "mhl");
	if (IS_ERR(&sec_mhl)) {
		MHL_ERR_LOG("failed to create class sec_mhl");
		ret = -1;
		goto err_exit;
	}

	it668x_factory = it668x;

	ret = class_create_file(sec_mhl, &class_attr_devctl);
	if (ret) {
		dev_err(&client->dev, "failed to create class_attr_devctl\n");

		goto err_exit;
	}

	ret = class_create_file(sec_mhl, &class_attr_mhl_msg);
	if (ret) {
		dev_err(&client->dev, "failed to dev_attr_packet_pixel\n");
		goto err_exit;
	}

	ret = class_create_file(sec_mhl, &class_attr_mhl_color);
	if (ret) {
		dev_err(&client->dev, "failed to dev_attr_mhl_color\n");
		goto err_exit;
	}

	it668x->sec_mhl = sec_mhl;
	return ret;

err_exit:
	/* todo: remove files */
	it668x->sec_mhl = NULL;
	return ret;
}

static const struct file_operations it668x_debug_fops = {
	.owner = THIS_MODULE,
	.open = it668x_fop_open,
	.read = it668x_fop_read,
	.write = it668x_fop_write,
	/* .unlocked_ioctl = it668x_fop_ioctl, */
	.release = it668x_fop_release,
};

int it668x_debugfs_init(struct it668x_data *it668x)
{
	it668x->debugfs =
	    debugfs_create_file("it668x", S_IFREG | S_IALLUGO, NULL, (void *)0, &it668x_debug_fops);
	if (NULL == it668x->debugfs) {
		MHL_ERR_LOG(" debugfs_create_file failed.\n");
		return -1;
	}
	return 0;
}

int it668x_debugfs_deinit(struct it668x_data *it668x)
{
	if (it668x && it668x->debugfs) {
		debugfs_remove(it668x->debugfs);
		return 0;
	}
	return -1;
}

static int it668x_hdmi_rx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct it668x_platform_data *pdata = client->dev.platform_data;

	pdata->hdmirx_client = client;

	return 0;
}

static int it668x_hdmi_tx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct it668x_platform_data *pdata = client->dev.platform_data;

	pdata->hdmitx_client = client;

	return 0;
}

static int it668x_mhl_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct it668x_data *it668x;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	MHL_DEF_LOG("[it668x] -- %s ++\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		MHL_ERR_LOG(" i2c_check_functionality()    FAIL!!!)\n");
		/* dev_err(&client->dev, "fi2c_check_functionality()    FAIL!!!.\n"); */
		return -EIO;
	}

	it668x = kzalloc(sizeof(struct it668x_data), GFP_KERNEL);

	if (!it668x) {
		MHL_ERR_LOG("IT668x -- failed to allocate driver data\n");
		/* dev_err(&client->dev, "IT668x -- failed to allocate driver data.\n"); */
		return -ENOMEM;
	}

	memset(it668x, 0, sizeof(struct it668x_data));

	it668x->pdata = client->dev.platform_data;
	it668x->pdata->mhl_client = client;

	it668x->ver = 0x1234;

	i2c_set_clientdata(client, it668x);

	it668x->it668x_dev = &client->dev;

	mutex_init(&it668x->lock);
	mutex_init(&it668x->cbus_lock);
	mutex_init(&it668x->msc_lock);
	mutex_init(&it668x->ddc_lock);
	mutex_init(&it668x->input_lock);

	init_waitqueue_head(&it668x->it668x_wq);
	init_waitqueue_head(&it668x->avi_wq);
	init_waitqueue_head(&it668x->cbus_wq);
	init_waitqueue_head(&it668x->hdcp_wq);
	init_waitqueue_head(&it668x->power_resume_wq);
	init_waitqueue_head(&it668x->mcu_hold_wq);

	atomic_set(&it668x->power_down, 0);
	atomic_set(&it668x->fw_reinit, 0);
	atomic_set(&it668x->mcu_hold, 0);
	atomic_set(&it668x->dump_register, 0);

	it668x->cbus_cmd_wqs = create_workqueue("it668x-cmd_wq");
	if (it668x->cbus_cmd_wqs == NULL) {
		MHL_ERR_LOG(" it668x create_workqueue(  it668x-cmd_wq   ) FAIL !!!\n");
		ret = -ENXIO;
	}

	it668x->avi_cmd_wqs = create_workqueue("it668x-avi_wq");
	if (it668x->avi_cmd_wqs == NULL)
		ret = -ENXIO;

	/*workqueue for CBUS */
	INIT_WORK(&it668x->msc_work, mhl_msc_event);
	/* INIT_WORK(&it668x->cbus_work, mhl_cts_cbus_ctrl); */
	INIT_LIST_HEAD(&it668x->cbus_data_list);


	INIT_WORK(&it668x->avi_control_work, hdmi_avi_control_thread);
	INIT_WORK(&it668x->hdcp_control_work, hdmi_hdcp_control_thread);

/* reset and powerdown HW */
	MHL_VERB_LOG(" reset hw init\n");
	it668x_hw_reset();
	MHL_VERB_LOG(" ***>>>it668x it668x_cal_oclk !!!\n");
	ret = it668x_cal_oclk(it668x);
	if (ret < 0)
		goto err_exit;
	/* it668x standby */
	it668x_power_down_after_driver_init(it668x);

	it668x_init_input_devices(it668x);
	it668x_dev_class_init(it668x);
	it668x_debugfs_init(it668x);

	init_waitqueue_head(&it668x_irq_wq);
	it668x_irq_task = kthread_create(it668x_irq_kthread, NULL, "hdmi_irq_kthread");
	atomic_set(&it668x_irq_event, 0);
	wake_up_process(it668x_irq_task);

#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_set_sens(CUST_EINT_EINT_HDMI_HPD_NUM, MT_LEVEL_SENSITIVE);
	mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM, EINTF_TRIGGER_LOW, &it668x_irq_wakeup, 0);
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif

	it668x_timer_task = kthread_create(it668x_mhl_detect, it668x, "it668x_mhl_detect");
	wake_up_process(it668x_timer_task);

	MHL_DEF_LOG("[it668x] -- %s --\n", __func__);

	return ret;

err_exit:
	kfree(it668x);
	return ret;
}


static int it668x_hdmi_rx_remove(struct i2c_client *client)
{
	return 0;
}

static int it668x_hdmi_tx_remove(struct i2c_client *client)
{
	return 0;
}

static int it668x_mhl_remove(struct i2c_client *client)
{
	it668x_debugfs_deinit(it668x_factory);
	return 0;
}

static struct it668x_platform_data pdata_it668x;

static struct i2c_board_info it668x_hdmi_rx = {
	.type = "it668x_hdmi_rx",
	.platform_data = &pdata_it668x,
	.addr = IT668X_HDMI_RX_ADDR >> 1,
};

static struct i2c_board_info it668x_hdmi_tx = {
	.type = "it668x_hdmi_tx",
	.platform_data = &pdata_it668x,
	.addr = IT668X_HDMI_TX_ADDR >> 1,
};

static struct i2c_board_info it668x_mhl = {
	.type = "it668x_mhl",
	.platform_data = &pdata_it668x,
	.addr = IT668X_MHL_ADDR >> 1,
};

static const struct i2c_device_id it668x_hdmi_rx_id[] = {
	{"it668x_hdmi_rx", 0},
	{}
};

static const struct i2c_device_id it668x_hdmi_tx_id[] = {
	{"it668x_hdmi_tx", 0},
	{}
};

static const struct i2c_device_id it668x_mhl_id[] = {
	{"it668x_mhl", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, it668x_hdmi_rx_id);
MODULE_DEVICE_TABLE(i2c, it668x_hdmi_tx_id);
MODULE_DEVICE_TABLE(i2c, it668x_mhl_id);

static struct i2c_driver it668x_hdmi_rx_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "it668x_hdmi_rx",
		   },
	.id_table = it668x_hdmi_rx_id,
	.probe = it668x_hdmi_rx_i2c_probe,
	.remove = it668x_hdmi_rx_remove,

};

static struct i2c_driver it668x_hdmi_tx_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "it668x_hdmi_tx",
		   },
	.id_table = it668x_hdmi_tx_id,
	.probe = it668x_hdmi_tx_i2c_probe,
	.remove = it668x_hdmi_tx_remove,

};

static struct i2c_driver it668x_mhl_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "it668x_mhl",
		   },
	.id_table = it668x_mhl_id,
	.probe = it668x_mhl_i2c_probe,
	.remove = it668x_mhl_remove,
};

static int __init it668x_init(void)
{
	int ret = 0;

	/* i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_hdmi_rx, 1); */
	ret = i2c_add_driver(&it668x_hdmi_rx_i2c_driver);
	if (ret < 0)
		return ret;

	/* i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_hdmi_tx, 1); */
	ret = i2c_add_driver(&it668x_hdmi_tx_i2c_driver);
	if (ret < 0)
		goto err_exit1;

	/* i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_mhl, 1); */
	ret = i2c_add_driver(&it668x_mhl_i2c_driver);
	if (ret < 0)
		goto err_exit2;

	ret = register_reboot_notifier(&it668x_reboot_notifier);
	if (ret != 0)
		MHL_ERR_LOG(" register reboot notifier fail !\n");
	MHL_DEF_LOG("[it668x]it668x_init done\n");

	return ret;

err_exit2:

	i2c_del_driver(&it668x_hdmi_tx_i2c_driver);

err_exit1:
	i2c_del_driver(&it668x_hdmi_rx_i2c_driver);
	MHL_ERR_LOG("i2c_add_driver fail  return\n");

	return ret;
}

static int __init it668x_board_init(void)
{
	int ret = 0;

	MHL_DEF_LOG("[it668x]it668x_board_init\n");

	i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_hdmi_rx, 1);

	i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_hdmi_tx, 1);

	i2c_register_board_info(IT668X_IIC_NUMBER, &it668x_mhl, 1);

	return ret;
}


static void __exit it668x_exit(void)
{
	i2c_del_driver(&it668x_mhl_i2c_driver);
	i2c_del_driver(&it668x_hdmi_tx_i2c_driver);
	i2c_del_driver(&it668x_hdmi_rx_i2c_driver);
}
core_initcall(it668x_board_init);
module_init(it668x_init);
module_exit(it668x_exit);


int it668x_read_edid(void *it668x_data, void *pedid, unsigned short max_length)
{
	struct it668x_data *it668x = it668x_factory;

	MHL_DEF_LOG("%s ++, pedid=%p, len=%u\n", __func__, pedid, max_length);

	if (it668x_data != NULL)
		it668x = it668x_data;

	if (pedid) {
		if (max_length > IT668X_EDID_BUF_LEN)
			max_length = IT668X_EDID_BUF_LEN;
		memcpy(pedid, it668x->edidbuf, max_length);
		return 0;
	}

	return -1;
}

static void it668x_turn_on_vbus(struct it668x_data *it668x, bool enable)
{
	if (it668x->pdata->enable_vbus)
		it668x->pdata->enable_vbus(enable);
	it668x->mhl_pow_en = enable;
}

static void it668x_switch_to_usb(struct it668x_data *it668x, bool keep_detection)
{
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	int ret;

	MHL_DEF_LOG("[it668x] %s ++\n", __func__);

	if (keep_detection)
		ret = i2c_set_reg(mhl, REG_MHL_CONTROL, B_USB_SWITCH_ON | B_DISABLE_MHL, B_USB_SWITCH_ON);
	else
		ret = i2c_set_reg(mhl, REG_MHL_CONTROL,
			B_USB_SWITCH_ON | B_DISABLE_MHL, B_USB_SWITCH_ON | B_DISABLE_MHL);

	if (ret < 0)
		MHL_VERB_LOG(" %s: i2c_set_reg=%d\n", __func__, ret);
	else
		it668x->switch_at_mhl = false;
}

static void it668x_switch_to_mhl(struct it668x_data *it668x)
{
	struct i2c_client *mhl = it668x->pdata->mhl_client;
	int ret;

	MHL_DEF_LOG("[it668x] %s ++\n", __func__);

	ret = i2c_set_reg(mhl, REG_MHL_CONTROL, B_USB_SWITCH_ON | B_DISABLE_MHL, 0x00);
	if (ret < 0)
		MHL_VERB_LOG(" %s: i2c_set_reg=%d\n", __func__, ret);
	else
		it668x->switch_at_mhl = true;
}

/*  */
/* MTK MHL Interface */
/*  */

int mhl_bridge_init(void)
{
	return 0;
}

int mhl_bridge_enter(void)
{
	return 0;
}

int mhl_bridge_exit(void)
{
	return 0;
}

void mhl_bridge_suspend(void)
{

}

void mhl_bridge_resume(void)
{

}

void mhl_bridge_power_on(void)
{
	struct it668x_data *it668x = it668x_factory;

	if (it668x) {
		MHL_DEF_LOG("[it668x] mhl_bridge_power_on\n");
		IT668X_MUTEX_LOCK(&it668x->lock);
		it668x_initial_chip(it668x);
		it668x->device_powerdown = 0;
		IT668X_ENABLE_IRQ();
		IT668X_MUTEX_UNLOCK(&it668x->lock);
	}
}

void mhl_bridge_power_off(void)
{
	struct it668x_data *it668x = it668x_factory;

	if (it668x) {
		IT668X_MUTEX_LOCK(&it668x->lock);
		IT668X_DISABLE_IRQ();
		it668x->device_powerdown = 1;
		it668x_cbus_drive_low(it668x);
		it668x_power_down(it668x);
		IT668X_MUTEX_UNLOCK(&it668x->lock);
		MHL_DEF_LOG("[it668x] mhl_bridge_power_off\n");
	}
}

MHLTX_CONNECT_STATE mhl_bridge_get_state(void)
{
	struct it668x_data *it668x = it668x_factory;
	MHLTX_CONNECT_STATE state = MHLTX_CONNECT_NO_DEVICE;

	if (NULL == it668x)
		return state;

	IT668X_MUTEX_LOCK(&it668x->lock);
	if (it668x->mhl_state == MHL_CBUS_CONNECTED)
		state = HDMITX_CONNECT_ACTIVE;
	IT668X_MUTEX_UNLOCK(&it668x->lock);

	return state;
}

void mhl_bridge_debug(unsigned char *pcmdbuf)
{
	/* move to mhl debugfs */
}

void mhl_bridge_enablehdcp(unsigned char u1hdcponoff)
{
	struct it668x_data *it668x = it668x_factory;
	if (it668x) {
		IT668X_MUTEX_LOCK(&it668x->lock);
		it668x->hdcp_enable = (u1hdcponoff > 0) ? 1 : 0;
		it668x_cbus_disconnect(it668x);
		/* it668x_cbus_drive_low( it668x ); */
		/* it668x_initial_chip( it668x ); */
		/* it668x_cbus_release( it668x ); */
		IT668X_MUTEX_UNLOCK(&it668x->lock);
	}
}

int mhl_bridge_getedid(unsigned char *pedidbuf)
{
	struct it668x_data *it668x = it668x_factory;

	if (NULL == pedidbuf)
		return -1;
	if (NULL == it668x)
		return -1;

	if (it668x->is_edid_ready) {
		MHL_DEF_LOG("[it668x] get from sink\n");
		IT668X_MUTEX_LOCK(&it668x->lock);
		memcpy(pedidbuf, &it668x->edidbuf[0], MHL_EDID_SIZE);
		IT668X_MUTEX_UNLOCK(&it668x->lock);
	} else {
		if (it668x->use_internal_edid_before_edid_ready) {
			MHL_DEF_LOG("[it668x] get from inter edid\n");
			memcpy(pedidbuf, &s_it668x_internal_edid[0], MHL_EDID_SIZE);
		} else {
			return -1;
		}
	}

	return 0;
}

void mhl_bridge_mutehdmi(unsigned char enable)
{
	struct it668x_data *it668x = it668x_factory;
	struct i2c_client *hdmitx;

	if (NULL == it668x || NULL == it668x->pdata || NULL == it668x->pdata->hdmitx_client)
		return;

	IT668X_MUTEX_LOCK(&it668x->lock);
	hdmitx = it668x->pdata->hdmitx_client;
	if (enable > 0)
		i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, B_EN_AVMUTE);	/* Enable AVMute */
	else {
		i2c_set_reg(hdmitx, REG_HDMITX_AVMUTE_CTRL, B_EN_AVMUTE, 0x00);	/* Disable AVMute */
		msleep(50);
	}
	IT668X_MUTEX_UNLOCK(&it668x->lock);

}

unsigned char mhl_bridge_is_sink_support_ppmode(void)
{
	struct it668x_data *it668x = it668x_factory;

	if (NULL == it668x)
		return false;

	return it668x->sink_support_packet_pixel_mode;
}

void mhl_bridge_res(unsigned char res, unsigned char cs)
{
	MHL_DEF_LOG("[it668x] mhl_bridge_res:%x,cs:%x\n", res, cs);
}

static MHL_BRIDGE_DRIVER s_mhl_bridge_api = {
	.init = mhl_bridge_init,
	.enter = mhl_bridge_enter,
	.exit = mhl_bridge_exit,
	.suspend = mhl_bridge_suspend,
	.resume = mhl_bridge_resume,
	.power_on = mhl_bridge_power_on,
	.power_off = mhl_bridge_power_off,
	.get_state = mhl_bridge_get_state,
	.enablehdcp = mhl_bridge_enablehdcp,
	.getedid = mhl_bridge_getedid,
	.mutehdmi = mhl_bridge_mutehdmi,
	.getppmodesupport = mhl_bridge_is_sink_support_ppmode,
	.resolution = mhl_bridge_res,
};

const MHL_BRIDGE_DRIVER *MHL_Bridge_GetDriver(void)
{
	return &s_mhl_bridge_api;
}
