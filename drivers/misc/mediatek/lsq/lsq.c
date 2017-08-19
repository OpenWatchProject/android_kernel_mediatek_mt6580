
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include <linux/gpio.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#include "lsq.h"

extern unsigned int idle_clock_mode;

static ssize_t show_build_time(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Build time: %s %s\n", __DATE__,__TIME__);
}

static ssize_t show_gpio_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	//64 65 67 69
	return sprintf(buf, "GPIO64 Mode:(%d)   GPIO65 Mode:(%d)    GPIO67 Mode:(%d)   GPIO69 Mode:(%d)\n", mt_get_gpio_mode(GPIO64),mt_get_gpio_mode(GPIO65),mt_get_gpio_mode(GPIO67),mt_get_gpio_mode(GPIO69));
}


static ssize_t show_idle_clock_control(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", idle_clock_mode);
}

static ssize_t store_idle_clock_control(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (count > 0) {
		switch (buf[0]) {
		case '0':
			idle_clock_mode = LSQ_IDLE_CLOCK_OFF;
			break;
		case '1':
			idle_clock_mode = LSQ_IDLE_CLOCK_ON;
			break;
		default:
			break;
		}
	}
	return count;
}


static DEVICE_ATTR(idle_clock_control, S_IRUGO | S_IWUGO,
			show_idle_clock_control, store_idle_clock_control);

static DEVICE_ATTR(build_time, S_IRUGO | S_IWUGO,
			show_build_time, NULL);


static DEVICE_ATTR(gpio_mode, S_IRUGO | S_IWUGO,
			show_gpio_mode, NULL);



static struct attribute *lsq_misc_sysfs_entries[] = {
	&dev_attr_idle_clock_control.attr,
	&dev_attr_build_time.attr,
	&dev_attr_gpio_mode.attr,
	NULL,
};


static struct attribute_group lsq_misc_attr_group = {
	.attrs	= lsq_misc_sysfs_entries,
};

static int __init lsq_misc_probe(struct platform_device *pdev)
{
	int i;
	int ret, rc;
	ret = sysfs_create_group(&pdev->dev.kobj,
					&lsq_misc_attr_group);
	if(ret)
	{
		LSQ_DRV_DEBUG("[LSQ]%s Create device sys attribute group fail\n", __func__);
		return ret;
	}
	return ret;
}




static struct platform_driver lsq_misc_driver = {
	.driver = {
		   .name = "lsq_misc",
		   .owner = THIS_MODULE,
		   },
	.probe = lsq_misc_probe,
	/*.remove = mt65xx_leds_remove,*/
	/* .suspend      = mt65xx_leds_suspend, */
	/* .shutdown = mt65xx_leds_shutdown,*/
};

static struct platform_device lsq_misc_device = {
	.name = "lsq_misc",
	.id = -1
};



static int __init lsq_init(void)
{
	int ret;
	LSQ_DRV_DEBUG("[LSQ]%s\n", __func__);

	ret = platform_device_register(&lsq_misc_device);
	if (ret)
		printk("[LSQ]lsq_init:dev:E%d\n", ret);
	ret = platform_driver_register(&lsq_misc_driver);

	if (ret)
	{
		LSQ_DRV_DEBUG("[LSQ]lsq_init:drv:E%d\n", ret);
		platform_device_unregister(&lsq_misc_device); 
		return ret;
	}
	return ret;
}

static void __exit lsq_exit(void)
{
	platform_driver_unregister(&lsq_misc_driver);
 	platform_device_unregister(&lsq_misc_device);
}



module_init(lsq_init);
module_exit(lsq_exit);
MODULE_AUTHOR("LSQ");
MODULE_DESCRIPTION("LSQ driver for misc control");
MODULE_LICENSE("GPL");
MODULE_ALIAS("lsq");

