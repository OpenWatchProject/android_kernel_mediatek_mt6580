#ifndef _IT668x_H_
#define _IT668x_H_

#define IT668x_HDMIRX_ADDR 0x90
#define IT668x_HDMITX_ADDR 0x98
#define IT668x_MHL_ADDR  0xC8

/*  */
/* ioctl interfaces */
/*  */

enum {
	IT668X_I2C_HDMI_RX = 1,
	IT668X_I2C_HDMI_TX,
	IT668X_I2C_MHL_TX,
};

struct it668x_ioctl_reg_op {
	unsigned char i2c_dev;
	unsigned char offset;
	unsigned char bank;
	unsigned char length;
	void *buffer;
};

struct it668x_ioctl_reg_op_buffered {
	unsigned char i2c_dev;
	unsigned char offset;
	unsigned char bank;
	unsigned char length;
	unsigned char buffer[64];
};


struct it668x_ioctl_reg_op_set {
	unsigned char i2c_dev;
	unsigned char offset;
	unsigned char bank;
	unsigned char data;
	unsigned char mask;
};

struct it668x_ioctl_read_edid {
	void *out_buffer;
	int out_buffer_length;
	int use_cached_data;
};

enum {
	IT668X_CTL_HOLD,
	IT668X_CTL_RELEASE,
	IT668X_CTL_PM_S,
	IT668X_CTL_PM_R,
	IT668X_CTL_MAX,
};

struct it668x_ioctl_data {
	unsigned char op;
	unsigned char rop;
};

enum {
	IT668X_DVAR_GET_LEN,
	IT668X_DVAR_GET,
	IT668X_DVAR_SET,
};

struct it668x_ioctl_driver_var {
	unsigned char op;

	union {
		struct {
			unsigned long driver_var_size;
		} getlen;

		struct {
			unsigned long length;
			void *buffer;
		} get;

		struct {
			unsigned long offset;
			unsigned long length;
			void *buffer;
		} set;
	};

};

/*
struct it668x_ioctl_read_edid {
    unsigned char op;
    unsigned char status;
    unsigned short length;
    void *buffer;
};
*/

#define IT668X_DEV_MAJOR 123
#define IT668X_DEV_IOCTLID 0xD0

#define	IT668X_IOCTL_READ_REG	_IOR(IT668X_DEV_IOCTLID, 1, struct it668x_ioctl_reg_op)
#define	IT668X_IOCTL_WRITE_REG	_IOW(IT668X_DEV_IOCTLID, 2, struct it668x_ioctl_reg_op)
#define	IT668X_IOCTL_SET_REG	_IOW(IT668X_DEV_IOCTLID, 3, struct it668x_ioctl_reg_op_set)
#define	IT668X_IOCTL_CONTROL	_IOW(IT668X_DEV_IOCTLID, 4, struct it668x_ioctl_data)
#define	IT668X_IOCTL_DRIVER_VAR	_IOW(IT668X_DEV_IOCTLID, 5, struct it668x_ioctl_driver_var)
#define	IT668X_IOCTL_READ_EDID	_IOR(IT668X_DEV_IOCTLID, 7, struct it668x_ioctl_read_edid)

/*  */
/* dev control */
/*  */
enum				/* for op */
{
	DEV_CTL_READ_REG = 0x10,
	DEV_CTL_WRITE_REG,
	DEV_CTL_SET_REG,
	DEV_CTL_CONTROL,
	DEV_CTL_CONTROL_PPMODE,
	DEV_CTL_CONTROL_HDCP,
};

struct it668x_dev_ctl {
	unsigned char op;
	union {
		struct {
			unsigned char dev;
			unsigned char status;
			unsigned char offset;
			unsigned char length;
		} read_reg;

		struct {
			unsigned char dev;
			unsigned char status;
			unsigned char offset;
			unsigned char data;
		} write_reg1;

		struct {
			unsigned char dev;
			unsigned char status;
			unsigned char offset;
			unsigned char mask;
			unsigned char data;
		} set_reg;

		struct {
			unsigned char sop;
		} control;

		struct {
			unsigned char mode;
		} ppmode;

		struct {
			unsigned char mode;
		} hdcp;
	};
};

/*  */
/* Platform data */
/*  */

struct it668x_platform_data {

	/* Called to setup board-specific power operations */
	void (*power) (int on);
	/* In case,when connectors are not able to automatically switch the
	 * D+/D- Path to SII8240,do the switching manually.
	 */
	void (*enable_path) (bool enable);

	struct i2c_client *mhl_client;
	struct i2c_client *hdmirx_client;
	struct i2c_client *hdmitx_client;


	/* to handle board-specific connector info & callback */
	int (*reg_notifier) (struct notifier_block *nb);
	int (*unreg_notifier) (struct notifier_block *nb);

	u8 power_state;
	u8 swing_level;
	int ddc_i2c_num;
	int mhl_i2c_num;
	void (*init) (void);
	void (*mhl_sel) (bool enable);
	void (*hw_onoff) (bool on);
	void (*hw_reset) (void);
	void (*enable_vbus) (bool enable);
	void (*switch_to_usb) (bool keep_detection);
	void (*switch_to_mhl) (void);

#if defined(__MHL_NEW_CBUS_MSC_CMD__)
	void (*vbus_present) (bool on, int value);
#else
	void (*vbus_present) (bool on);
#endif
#ifdef CONFIG_SAMSUNG_MHL_UNPOWERED
	int (*get_vbus_status) (void);
	void (it668x_otg_control) (bool onoff);
#endif
	void (*it668x_muic_cb) (bool attached, int charger);

#ifdef CONFIG_EXTCON
	const char *extcon_name;
#endif
};

#endif
