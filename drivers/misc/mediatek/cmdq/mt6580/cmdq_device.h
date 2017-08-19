#ifndef __CMDQ_DEVICE_H__
#define __CMDQ_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/device.h>
#include "cmdq_def.h"

struct device *cmdq_dev_get(void);
const uint32_t cmdq_dev_get_irq_id(void);
const uint32_t cmdq_dev_get_irq_secure_id(void);
const long cmdq_dev_get_module_base_VA_GCE(void);
const long cmdq_dev_get_module_base_PA_GCE(void);

const long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void);
const long cmdq_dev_get_module_base_VA_MDP_RDMA(void);
const long cmdq_dev_get_module_base_VA_MDP_RSZ0(void);
const long cmdq_dev_get_module_base_VA_MDP_RSZ1(void);
const long cmdq_dev_get_module_base_VA_MDP_WDMA(void);
const long cmdq_dev_get_module_base_VA_MDP_WROT(void);
const long cmdq_dev_get_module_base_VA_MDP_TDSHP(void);
const long cmdq_dev_get_module_base_VA_MM_MUTEX(void);
const long cmdq_dev_get_module_base_VA_VENC(void);

const long cmdq_dev_alloc_module_base_VA_by_name(const char *name);
void cmdq_dev_free_module_base_VA(const long VA);

void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);

void cmdq_dev_enable_clock_gce(bool enable);
void cmdq_dev_enable_mdp_clock(bool enable, CMDQ_ENG_ENUM engine);
bool cmdq_dev_mdp_clock_is_on(CMDQ_ENG_ENUM engine);

#define DECLARE_ENABLE_HW_CLOCK(HW_NAME) uint32_t cmdq_dev_enable_clock_##HW_NAME(bool enable);
DECLARE_ENABLE_HW_CLOCK(SMI_COMMON);
DECLARE_ENABLE_HW_CLOCK(SMI_LARB0);
DECLARE_ENABLE_HW_CLOCK(CAM_MDP);
DECLARE_ENABLE_HW_CLOCK(MDP_RDMA0);
DECLARE_ENABLE_HW_CLOCK(MDP_RSZ0);
DECLARE_ENABLE_HW_CLOCK(MDP_RSZ1);
DECLARE_ENABLE_HW_CLOCK(MDP_WDMA);
DECLARE_ENABLE_HW_CLOCK(MDP_WROT0);
DECLARE_ENABLE_HW_CLOCK(MDP_TDSHP0);
#define DECLARE_ENABLE_HW_CLOCK

#endif				/* __CMDQ_DEVICE_H__ */
