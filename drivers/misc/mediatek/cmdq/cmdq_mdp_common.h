#ifndef __CMDQ_MDP_COMMON_H__
#define __CMDQ_MDP_COMMON_H__

#include "cmdq_def.h"

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif


	void cmdq_mdp_enable(uint64_t engineFlag, CMDQ_ENG_ENUM engine);

	int cmdq_mdp_loop_reset(CMDQ_ENG_ENUM engine,
				const unsigned long resetReg,
				const unsigned long resetStateReg,
				const uint32_t resetMask,
				const uint32_t resetValue, const bool pollInitResult);

	void cmdq_mdp_loop_off(CMDQ_ENG_ENUM engine,
			       const unsigned long resetReg,
			       const unsigned long resetStateReg,
			       const uint32_t resetMask,
			       const uint32_t resetValue, const bool pollInitResult);

	void cmdq_mdp_dump_venc(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rdma(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rsz(const unsigned long base, const char *label);
	void cmdq_mdp_dump_rot(const unsigned long base, const char *label);
	void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label);
	void cmdq_mdp_dump_wdma(const unsigned long base, const char *label);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_MDP_COMMON_H__ */
