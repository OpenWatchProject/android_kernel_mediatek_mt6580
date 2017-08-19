#ifndef __CMDQ_DEVICE_COMMON_H__
#define __CMDQ_DEVICE_COMMON_H__

#include <linux/platform_device.h>
#include <linux/device.h>

#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif /* !defined(CONFIG_MTK_LEGACY) */

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name);
void cmdq_dev_free_module_base_VA(const long VA);

#ifdef CONFIG_MTK_LEGACY
uint32_t cmdq_dev_enable_mtk_clock(bool enable, enum cg_clk_id gateId, const char *name);
bool cmdq_dev_mtk_clock_is_enable(enum cg_clk_id gateId);

#else
void cmdq_dev_get_module_clock_by_name(const char *name, const char *clkName, struct clk **clk_module);
uint32_t cmdq_dev_enable_device_clock(bool enable, struct clk *clk_module, const char *clkName);
bool cmdq_dev_device_clock_is_enable(struct clk *clk_module);
	
#endif /* CONFIG_MTK_LEGACY */

#endif				/* __CMDQ_DEVICE_COMMON_H__ */
