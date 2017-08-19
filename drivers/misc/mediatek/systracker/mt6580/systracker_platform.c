#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/mtk_ram_console.h>
#include <asm/system_misc.h>
#include <asm/traps.h>
#include <asm/signal.h>
#include <mach/sync_write.h>
#include "../systracker_v2.h"

#ifdef SYSTRACKER_TEST_SUIT
void __iomem *p1;
void __iomem *mm_area1;
#endif

int systracker_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	int i;

	aee_dump_backtrace(regs, NULL);
	printk("[TRACKER] BUS_DBG_CON = %x, T0= %x, T1 = %x\n", readl(IOMEM(BUS_DBG_CON)), readl(IOMEM(BUS_DBG_TIMER_CON0)), readl(IOMEM(BUS_DBG_TIMER_CON1)));
	if (readl(IOMEM(BUS_DBG_CON)) & (BUS_DBG_CON_IRQ_AR_STA0|BUS_DBG_CON_IRQ_AR_STA1)){
		for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
			printk(KERN_ALERT "AR_TRACKER Timeout Entry[%d]: ReadAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n",
				i, readl(IOMEM(BUS_DBG_AR_TRACK_L(i))), readl(IOMEM(BUS_DBG_AR_TRACK_H(i))), readl(IOMEM(BUS_DBG_AR_TRANS_TID(i))));
		}
	}
	if (readl(IOMEM(BUS_DBG_CON)) & (BUS_DBG_CON_IRQ_AW_STA0|BUS_DBG_CON_IRQ_AW_STA1)){
		for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
			printk(KERN_ALERT "AW_TRACKER Timeout Entry[%d]: WriteAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n",
				i, readl(IOMEM(BUS_DBG_AW_TRACK_L(i))), readl(IOMEM(BUS_DBG_AW_TRACK_H(i))), readl(IOMEM(BUS_DBG_AW_TRANS_TID(i))));
		}
	}

	return -1;
}

//ARM32 version
static int systracker_platform_hook_fault(void)
{

#ifdef CONFIG_ARM_LPAE
	hook_fault_code(0x10, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
	hook_fault_code(0x11, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
#else
	hook_fault_code(0x8, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
	hook_fault_code(0x16, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
#endif
	return 0;
}

#ifdef SYSTRACKER_TEST_SUIT
void __iomem *p1;
void __iomem *mm_area1;

static int systracker_platform_test_init(void)
{
	return 0;
}

static void systracker_platform_test_cleanup(void)
{
}

static void systracker_platform_wp_test(void)
{
	void __iomem *ptr;
	/* use eint reg base as our watchpoint */
	ptr = ioremap(0x1000b000, 0x4); 
	pr_notice("%s:%d: we got p = 0x%p\n", __func__, __LINE__, ptr);
	systracker_set_watchpoint_addr(0x1000b000);
	systracker_watchpoint_enable();
	/* touch it */
	writel(0, ptr);
	pr_notice("after we touched watchpoint\n");
	iounmap(ptr);
	return;
}

extern void systracker_reset_default(void);
extern void systracker_enable_default_ex(void);

static void systracker_platform_read_timeout_test(void)
{
#if 1
	systracker_reset_default();
	systracker_enable_default_ex();
#else
	u32 con = 0;
	void __iomem *ptr;

	printk("B [TRACKER] BUS_DBG_CON = %x, T0= %x, T1 = %x\n", readl(IOMEM(BUS_DBG_CON)), readl(IOMEM(BUS_DBG_TIMER_CON0)), readl(IOMEM(BUS_DBG_TIMER_CON1)));
	printk("\n\n************ READ TIMEOUT TEST *********\n\n");

	writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SW_RST, IOMEM(BUS_DBG_CON));
	writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_IRQ_CLR, IOMEM(BUS_DBG_CON));
	dsb();
	printk("M [TRACKER] BUS_DBG_CON = %x, T0= %x, T1 = %x\n", readl(IOMEM(BUS_DBG_CON)), readl(IOMEM(BUS_DBG_TIMER_CON0)), readl(IOMEM(BUS_DBG_TIMER_CON1)));
	writel(0x195e24, IOMEM(BUS_DBG_TIMER_CON0));
	writel(0, IOMEM(BUS_DBG_TIMER_CON1));
	con = BUS_DBG_CON_BUS_DBG_EN | BUS_DBG_CON_BUS_OT_EN;
	con |= BUS_DBG_CON_TIMEOUT_EN;
	con |= BUS_DBG_CON_SLV_ERR_EN;
	con |= BUS_DBG_CON_IRQ_EN;
	con &= ~BUS_DBG_CON_IRQ_WP_EN;

	con |= BUS_DBG_CON_HALT_ON_EN;
	writel(con, IOMEM(BUS_DBG_CON));
	dsb();
	con |= BUS_DBG_CON_IRQ_WP_EN;
	writel(con, IOMEM(BUS_DBG_CON));
	dsb();

	printk("A [TRACKER] BUS_DBG_CON = %x, T0= %x, T1 = %x\n", readl(IOMEM(BUS_DBG_CON)), readl(IOMEM(BUS_DBG_TIMER_CON0)), readl(IOMEM(BUS_DBG_TIMER_CON1)));

	/* use eint reg base as our watchpoint */
	ptr = ioremap(0x1000b000, 0x4); 
	/* touch it */
	printk("\n\n\n*** TOUCH ***\n\n\n");
	printk("0x1000b000 = %x\n", readl(ptr));
	iounmap(ptr);
#endif
}

static void systracker_platform_write_timeout_test(void)
{
}

static void systracker_platform_withrecord_test(void)
{
}

static void systracker_platform_notimeout_test(void)
{
}
#endif //end of SYSTRACKER_TEST_SUIT

/*
 * mt_systracker_init: initialize driver.
 * Always return 0.
 */
static int __init mt_systracker_init(void)
{
	struct mt_systracker_driver *systracker_drv;
	systracker_drv = get_mt_systracker_drv();

	systracker_drv->systracker_hook_fault = systracker_platform_hook_fault;
#ifdef SYSTRACKER_TEST_SUIT
	systracker_drv->systracker_test_init = systracker_platform_test_init;
	systracker_drv->systracker_test_cleanup = systracker_platform_test_cleanup;
	systracker_drv->systracker_wp_test = systracker_platform_wp_test;
	systracker_drv->systracker_read_timeout_test = systracker_platform_read_timeout_test;
	systracker_drv->systracker_write_timeout_test = systracker_platform_write_timeout_test;
	systracker_drv->systracker_withrecord_test = systracker_platform_withrecord_test;
	systracker_drv->systracker_notimeout_test = systracker_platform_notimeout_test;
#endif
	return 0;
}

arch_initcall(mt_systracker_init);
MODULE_DESCRIPTION("system tracker driver v2.0");
