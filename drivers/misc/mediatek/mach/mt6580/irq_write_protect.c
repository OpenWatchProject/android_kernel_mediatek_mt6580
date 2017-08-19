#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/mt-gic.h>

#include <mach/mt_secure_api.h>


static void mt_set_pol_via_smc(void __iomem *addr, u32 value)
{
	mcusys_smc_write(addr, value);
}

static int __init mt_irq_w_protect_init(void){
	pr_notice("irq pol register write protection workaround init...\n");
	irq_pol_workaround = mt_set_pol_via_smc;

	return 0;
}

arch_initcall(mt_irq_w_protect_init);


