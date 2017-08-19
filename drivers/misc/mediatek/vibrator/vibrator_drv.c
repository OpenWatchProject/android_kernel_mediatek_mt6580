/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 *
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include "timed_output.h"


#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/jiffies.h>
#include <linux/timer.h>

#include <mach/mt_typedefs.h>
#include <cust_vibrator.h>
#include <vibrator_hal.h>


#include <linux/kthread.h>
#include <linux/delay.h>



#define VERSION					"v 0.2"
#define VIB_DEVICE				"mtk_vibrator"


static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(vib_waiter);
static int vib_flag = 0;


/******************************************************************************
Error Code No.
******************************************************************************/
#define RSUCCESS        0

/******************************************************************************
Debug Message Settings
******************************************************************************/

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INT			0x00000001	/* Interrupt related event */
#define DBG_EVT_TASKLET		0x00000002	/* Tasklet related event */

#define DBG_EVT_ALL			0xffffffff

#define DBG_EVT_MASK		(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		printk(fmt, ##args); \
	} \
} while (0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __func__)
#else
#define MSG(evt, fmt, args...) do {} while (0)
#define MSG_FUNC_ENTRY(f)	   do {} while (0)
#endif


/******************************************************************************
Global Definations
******************************************************************************/
static spinlock_t vibe_lock;
static int vibe_state;
static int ldo_state;
static int shutdown_flag;
static int vibe_time;

/**********************************************************************************************/
/*Vibrate operate function*/
static int vibr_Enable(void)
{
	if (!ldo_state) {
		vibr_Enable_HW();
		ldo_state = 1;
	}
	return 0;
}

static int vibr_Disable(void)
{
	if (ldo_state) {
		vibr_Disable_HW();
		ldo_state = 0;
	}
	return 0;
}


static int vibr_time_remine  = 0;
static int vibr_kernel_thread_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

	printk("tpd_event_handler\n");

    sched_setscheduler(current, SCHED_RR, &param);
	
	do
	{
		set_current_state(TASK_INTERRUPTIBLE);
		while (vib_flag)
		{
			vib_flag = 0;
			msleep(10);
		} 
		
		wait_event_interruptible(vib_waiter, vib_flag != 0);
		vib_flag = 0;

		
		set_current_state(TASK_RUNNING);
		vibr_Enable();
		vibe_state = 1;
		while(vibr_time_remine)
		{
			if(vibr_time_remine >= 20)
			{
				vibr_time_remine -= 20;
				msleep(12);
			}
			else
			{
				msleep(vibr_time_remine);
				vibr_time_remine = 0;
			}
		}
		vibr_Disable();
		vibe_state = 0;
    }
	while (!kthread_should_stop());

    return RSUCCESS;
}


static int vibrator_get_time(struct timed_output_dev *dev)
{
	return vibr_time_remine;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	unsigned long flags;
int ret;
	
	 /**********************modify begin*************************************************/
	
	if (value == 0 && vibe_state == 1)   //vibrator is on and need to shutdown
	
	{
		vibr_time_remine = 0;
		msleep(10);
		return;
	
	}
	
	/***************************modify end**************************************************/


#if 1
	struct vibrator_hw *hw = mt_get_cust_vibrator_hw();
#endif
	printk("[vibrator]vibrator_enable: vibrator first in value = %d\n", value);

	spin_lock_irqsave(&vibe_lock, flags);


	if (value == 0 || shutdown_flag == 1) {
		printk("[vibrator]vibrator_enable: shutdown_flag = %d\n", shutdown_flag);
		vibr_time_remine = 0;
	} else {
#if 1
		printk("[vibrator]vibrator_enable: vibrator cust timer: %d\n", hw->vib_timer);
#ifdef CUST_VIBR_LIMIT
		if (value > hw->vib_limit && value < hw->vib_timer)
#else
		if (value >= 10 && value < hw->vib_timer)
#endif
			value = hw->vib_timer;
#endif
		value =  ((value < hw->vib_timer) ? hw->vib_timer : value ); 
		value = (value > 15000 ? 15000 : value);
		vibr_time_remine = vibe_time = value;
		printk("[vibrator]vibrator_enable: vibrator start: %d\n", value);
		wake_up_interruptible(&vib_waiter);vib_flag = 1;
		printk("[vibrator] vibrator_enable queue_work = %d\n",ret);
	}

	spin_unlock_irqrestore(&vibe_lock, flags);
	
	
	
}



static struct timed_output_dev mtk_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int vib_probe(struct platform_device *pdev)
{
	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	printk("[vibrator]vib_shutdown: enter!\n");
	spin_lock_irqsave(&vibe_lock, flags);
	shutdown_flag = 1;
	if (vibe_state) {
		printk("[vibrator]vib_shutdown: vibrator will disable\n");
		vibe_state = 0;
		spin_unlock_irqrestore(&vibe_lock, flags);
		vibr_Disable();
	} else {
		spin_unlock_irqrestore(&vibe_lock, flags);
	}
}

/******************************************************************************
Device driver structure
*****************************************************************************/
static struct platform_driver vibrator_driver = {
	.probe = vib_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
	.driver = {
		   .name = VIB_DEVICE,
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device vibrator_device = {
	.name = "mtk_vibrator",
	.id = -1,
};

static ssize_t store_vibr_on(struct device *dev, struct device_attribute *attr, const char *buf,
			     size_t size)
{
	if (buf != NULL && size != 0) {
		if (buf[0] == '0') {
			vibr_Disable();
		} else {
			vibr_Enable();
		}
	}
	return size;
}

static DEVICE_ATTR(vibr_on, 0777, NULL, store_vibr_on);

/******************************************************************************
 * vib_mod_init
 *
 * DESCRIPTION:
 *   Register the vibrator device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   None
 *
 * NOTES:
 *   RSUCCESS : Success
 *
 ******************************************************************************/

static int vib_mod_init(void)
{
	s32 ret;
	s32 err;

	printk("MediaTek MTK vibrator driver register, version %s\n", VERSION);
	vibr_power_set();	/* set vibr voltage if needs.  Before MT6320 vibr default voltage=2.8v but in MT6323 vibr default voltage=1.2v */
	ret = platform_device_register(&vibrator_device);
	if (ret != 0) {
		printk("[vibrator]Unable to register vibrator device (%d)\n", ret);
		return ret;
	}

	spin_lock_init(&vibe_lock);
	shutdown_flag = 0;
	vibe_state = 0;

	timed_output_dev_register(&mtk_vibrator);

	ret = platform_driver_register(&vibrator_driver);

	if (ret) {
		printk("[vibrator]Unable to register vibrator driver (%d)\n", ret);
		return ret;
	}

	ret = device_create_file(mtk_vibrator.dev, &dev_attr_vibr_on);
	if (ret) {
		printk("[vibrator]device_create_file vibr_on fail!\n");
	}

	//start kernel thread to 
	thread = kthread_run(vibr_kernel_thread_handler, 0, VIB_DEVICE);
    if (IS_ERR(thread))
	{
        err = PTR_ERR(thread);
        printk("[vibrator] failed to create kernel thread: %d\n", err);
    }
	
	printk("[vibrator]vib_mod_init Done\n");

	return RSUCCESS;
}

/******************************************************************************
 * vib_mod_exit
 *
 * DESCRIPTION:
 *   Free the device driver !
 *
 * PARAMETERS:
 *   None
 *
 * RETURNS:
 *   None
 *
 * NOTES:
 *   None
 *
 ******************************************************************************/

static void vib_mod_exit(void)
{
	printk("MediaTek MTK vibrator driver unregister, version %s\n", VERSION);

	printk("[vibrator]vib_mod_exit Done\n");
}
module_init(vib_mod_init);
module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
