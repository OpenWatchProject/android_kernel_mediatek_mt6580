#include "cmdq_device_common.h"
#include "cmdq_core.h"

/* device tree */
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

#ifdef CONFIG_MTK_LEGACY
uint32_t cmdq_dev_enable_mtk_clock(bool enable, enum cg_clk_id gateId, const char *name)
{
	if (enable){
		enable_clock(gateId, name);
	}else{
		disable_clock(gateId, name);
	}
	return 0;
}

bool cmdq_dev_mtk_clock_is_enable(enum cg_clk_id gateId)
{
	return clock_is_on(gateId);
}

#else

void cmdq_dev_get_module_clock_by_dev(struct device *dev, const char *clkName, struct clk **clk_module)
{
	*clk_module = devm_clk_get(dev, clkName);
	if (IS_ERR(*clk_module)) {
		CMDQ_ERR("DEV: cannot get module clock: %s\n", clkName);
	}else{
		CMDQ_LOG("DEV: get module clock: %s\n", clkName);
	}
}

void cmdq_dev_get_module_clock_by_name(const char *name, const char *clkName, struct clk **clk_module)
{
	struct device_node *node = NULL;
	node = of_find_compatible_node(NULL, NULL, name);

	*clk_module = of_clk_get_by_name(node, clkName);
	if (IS_ERR(*clk_module)) {
		CMDQ_ERR("DEV: byName: cannot get module clock: %s\n", clkName);
	}else{
		CMDQ_LOG("DEV: byName: get module clock: %s\n", clkName);
	}
}

uint32_t cmdq_dev_enable_device_clock(bool enable, struct clk *clk_module, const char *clkName)
{
	int result = 0;
	if (enable){
		result = clk_prepare_enable(clk_module);
		CMDQ_LOG("enable clock with module: %s, result: %d\n", clkName, result);
	}else{
		clk_disable_unprepare(clk_module);
		CMDQ_LOG("disable clock with module: %s\n", clkName);
	}
	return result;
}

bool cmdq_dev_device_clock_is_enable(struct clk *clk_module){
	return true;
}

#endif /* !defined(CONFIG_MTK_LEGACY) */

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, name);
	VA = (unsigned long)of_iomap(node, 0);
	CMDQ_LOG("DEV: VA(%s): 0x%lx\n", name, VA);
	return VA;
}

void cmdq_dev_free_module_base_VA(const long VA)
{
	iounmap((void*)VA);
}

