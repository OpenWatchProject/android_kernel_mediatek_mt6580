#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>  

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/irqs.h>
#include <mach/mt_cirq.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_cpuidle.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_boot.h>
#include <mach/upmu_common.h>

#include "mt_spm_internal.h"


/**************************************
 * only for internal debug
 **************************************/

#define SODI_TAG     "[SODI] "
#define sodi_emerg(fmt, args...)	pr_emerg(SODI_TAG fmt, ##args)
#define sodi_alert(fmt, args...)	pr_alert(SODI_TAG fmt, ##args)
#define sodi_crit(fmt, args...)		pr_crit(SODI_TAG fmt, ##args)
#define sodi_err(fmt, args...)		pr_err(SODI_TAG fmt, ##args)
#define sodi_warn(fmt, args...)		pr_warn(SODI_TAG fmt, ##args)
#define sodi_notice(fmt, args...)	pr_notice(SODI_TAG fmt, ##args)
#define sodi_info(fmt, args...)		pr_info(SODI_TAG fmt, ##args)
#define sodi_debug(fmt, args...)	pr_info(SODI_TAG fmt, ##args)	/* pr_debug show nothing */

#define SPM_BYPASS_SYSPWREQ         0 //JTAG is used

#define SODI_DVT_APxGPT             0

#if SODI_DVT_APxGPT
#define SODI_DVT_STEP_BY_STEP       0
#define SODI_DVT_WAKEUP             0
#define SODI_DVT_PCM_TIMER_DISABLE  0
#define SODI_DVT_SPM_MEM_RW_TEST    0
#else
#define SODI_DVT_STEP_BY_STEP       0 
#define SODI_DVT_WAKEUP             0
#define SODI_DVT_PCM_TIMER_DISABLE  0
#define SODI_DVT_SPM_MEM_RW_TEST    0
#endif

#define SPM_USE_TWAM_DEBUG	        0

#if SODI_DVT_SPM_MEM_RW_TEST
#define SODI_DVT_MAGIC_NUM 			0xa5a5a5a5 
static u32 magicArray[16]=
{	
	SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,SODI_DVT_MAGIC_NUM,	
};
#endif

#if SODI_DVT_WAKEUP
#define WAKE_SRC_FOR_SODI (WAKE_SRC_GPT | WAKE_SRC_EINT)
#else
#define WAKE_SRC_FOR_SODI \
    (WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_EINT | WAKE_SRC_WDT | WAKE_SRC_CCIF0_MD | WAKE_SRC_CONN2AP | \
     WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_AFE | \
     WAKE_SRC_CIRQ | WAKE_SRC_SEJ | WAKE_SRC_SYSPWREQ | WAKE_SRC_MD1_WDT)
#endif 

#ifdef CONFIG_MTK_RAM_CONSOLE
#define SPM_AEE_RR_REC 1
#else
#define SPM_AEE_RR_REC 0
#endif

#if SPM_AEE_RR_REC
enum spm_sodi_step
{
	SPM_SODI_ENTER=0,
	SPM_SODI_ENTER_SPM_FLOW,
	SPM_SODI_ENTER_WFI,
	SPM_SODI_LEAVE_WFI,
	SPM_SODI_LEAVE_SPM_FLOW,
	SPM_SODI_LEAVE,
	SPM_SODI_VCORE_HPM,
	SPM_SODI_VCORE_LPM
};
#endif

// please put firmware to vendor/mediatek/proprietary/hardware/spm/mtxxxx/
#define USE_DYNA_LOAD_SODI
#ifndef USE_DYNA_LOAD_SODI
static const u32 sodi_binary[] = {
	0x814e0001, 0xd8200465, 0x17c07c1f, 0x81491801, 0xd80001c5, 0x17c07c1f,
	0x18c0001f, 0x102085cc, 0x1910001f, 0x102085cc, 0x813f8404, 0xe0c00004,
	0x1910001f, 0x102085cc, 0x81411801, 0xd80002e5, 0x17c07c1f, 0x18c0001f,
	0x10006240, 0xe0e00016, 0xe0e0001e, 0xe0e0000e, 0xe0e0000f, 0x803e0400,
	0x1b80001f, 0x20000222, 0x80380400, 0x1b80001f, 0x20000280, 0x803b0400,
	0x1b80001f, 0x2000001a, 0x803d0400, 0x1b80001f, 0x20000208, 0x80340400,
	0x80310400, 0x1950001f, 0x10006b04, 0x81439401, 0xd8000b85, 0x17c07c1f,
	0x81431801, 0xd8000b85, 0x17c07c1f, 0x1b80001f, 0x2000000a, 0x18c0001f,
	0x10006240, 0xe0e0000d, 0x81411801, 0xd8000a25, 0x17c07c1f, 0x1b80001f,
	0x20000020, 0x18c0001f, 0x102080f0, 0x1910001f, 0x102080f0, 0xa9000004,
	0x10000000, 0xe0c00004, 0x1b80001f, 0x2000000a, 0x89000004, 0xefffffff,
	0xe0c00004, 0x18c0001f, 0x102070f4, 0x1910001f, 0x102070f4, 0xa9000004,
	0x02000000, 0xe0c00004, 0x1b80001f, 0x2000000a, 0x89000004, 0xfdffffff,
	0xe0c00004, 0x1910001f, 0x102070f4, 0x81491801, 0xd8000b85, 0x17c07c1f,
	0x18c0001f, 0x102085cc, 0x1910001f, 0x102085cc, 0xa11f8404, 0xe0c00004,
	0x1910001f, 0x102085cc, 0x81fa0407, 0x81f08407, 0xe8208000, 0x10006354,
	0xfffff421, 0xa1d80407, 0xa1df0407, 0xc20024c0, 0x1211041f, 0x1b00001f,
	0xbfffc7ff, 0xf0000000, 0x17c07c1f, 0x1a50001f, 0x10006608, 0x80c9a401,
	0x810ba401, 0x10920c1f, 0xa0979002, 0xa0958402, 0x8080080d, 0xd8201002,
	0x17c07c1f, 0x81f08407, 0xa1d80407, 0xa1df0407, 0x1b00001f, 0x3fffc7ff,
	0x1b80001f, 0x20000004, 0xd8001a2c, 0x17c07c1f, 0x1b00001f, 0xbfffc7ff,
	0xd0001a20, 0x17c07c1f, 0x81f80407, 0x81ff0407, 0x1880001f, 0x10006320,
	0xc0c01c20, 0xe080000f, 0xd8000e63, 0x17c07c1f, 0x1880001f, 0x10006320,
	0xe080001f, 0xa0c01403, 0xd8000e63, 0x17c07c1f, 0x81491801, 0xd8001325,
	0x17c07c1f, 0x18c0001f, 0x102085cc, 0x1910001f, 0x102085cc, 0x813f8404,
	0xe0c00004, 0x1910001f, 0x102085cc, 0xa0110400, 0xa0140400, 0x1950001f,
	0x10006b04, 0x81439401, 0xd8001985, 0x17c07c1f, 0x81431801, 0xd8001985,
	0x17c07c1f, 0xa1da0407, 0x18c0001f, 0x110040d8, 0x1910001f, 0x110040d8,
	0xa11f8404, 0xe0c00004, 0x1910001f, 0x110040d8, 0xa0180400, 0xa01b0400,
	0xa01d0400, 0x17c07c1f, 0x17c07c1f, 0xa01e0400, 0x17c07c1f, 0x17c07c1f,
	0x18c0001f, 0x102085cc, 0x1910001f, 0x102085cc, 0xa11f8404, 0xe0c00004,
	0x81411801, 0xd8001825, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c01b80,
	0x17c07c1f, 0x81491801, 0xd8001985, 0x17c07c1f, 0x18c0001f, 0x102085cc,
	0x1910001f, 0x102085cc, 0xa11f8404, 0xe0c00004, 0x1910001f, 0x102085cc,
	0x0280040a, 0xc20024c0, 0x1211841f, 0x1b00001f, 0x7fffc7ff, 0xf0000000,
	0x17c07c1f, 0xe0e00016, 0x1380201f, 0xe0e0001e, 0x1380201f, 0xe0e0000e,
	0xe0e0000c, 0xe0e0000d, 0xf0000000, 0x17c07c1f, 0xe0e0000f, 0xe0e0001e,
	0xe0e00012, 0xf0000000, 0x17c07c1f, 0x1112841f, 0xa1d08407, 0x1a50001f,
	0x10006608, 0x8209a401, 0x814ba401, 0x1092201f, 0xa0979402, 0xa0958402,
	0xd8201e24, 0x8140080d, 0xd8001e85, 0x10c07c1f, 0x80eab401, 0xd8001d43,
	0x01200404, 0x1a00001f, 0x10006814, 0xe2000003, 0xf0000000, 0x17c07c1f,
	0xd8001fca, 0x17c07c1f, 0xe2e00036, 0x17c07c1f, 0x17c07c1f, 0xe2e0003e,
	0x1380201f, 0xe2e0003c, 0xd820210a, 0x17c07c1f, 0x1b80001f, 0x20000018,
	0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000,
	0x17c07c1f, 0xa1d40407, 0x1391841f, 0xf0000000, 0x17c07c1f, 0xd800230a,
	0x17c07c1f, 0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd82023aa, 0x17c07c1f,
	0xe2e0002e, 0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f, 0x18d0001f,
	0x10006604, 0x10cf8c1f, 0xd82023e3, 0x17c07c1f, 0xf0000000, 0x17c07c1f,
	0x18c0001f, 0x10006b18, 0x1910001f, 0x10006b18, 0xa1002004, 0xe0c00004,
	0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0xe8208000, 0x10006b18, 0x00000000, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x3fffc7ff, 0x1b80001f, 0xd00f0000, 0x8880000c,
	0x3fffc7ff, 0xd80053c2, 0x17c07c1f, 0xe8208000, 0x10006354, 0xfffff421,
	0xc0c02140, 0x81401801, 0xd8004725, 0x17c07c1f, 0x81f60407, 0x18c0001f,
	0x10006200, 0xc0c02260, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c02260, 0x1280041f, 0x18c0001f, 0x10006208,
	0xc0c02260, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c02260, 0x1280041f, 0x18c0001f, 0x10006290, 0xc0c02260,
	0x12807c1f, 0xc0c02260, 0x1280041f, 0xc20024c0, 0x1212041f, 0x18c0001f,
	0x10006294, 0xe0f07fff, 0xe0e00fff, 0xe0e000ff, 0xa1d38407, 0x18c0001f,
	0x11004078, 0x1910001f, 0x11004078, 0xa11f8404, 0xe0c00004, 0x1910001f,
	0x11004078, 0xa0108400, 0xa0148400, 0xa01b8400, 0xa0188400, 0xe8208000,
	0x10006310, 0x0b160038, 0x1b00001f, 0xbfffc7ff, 0x81439801, 0xd8204b05,
	0x17c07c1f, 0xe8208000, 0x10006310, 0x0b160008, 0x1b00001f, 0x3fffc7ff,
	0x12807c1f, 0x1b80001f, 0x90100000, 0x1ac0001f, 0x10006b6c, 0xe2c0000a,
	0xe8208000, 0x10006310, 0x0b160008, 0x80c10001, 0xc8c00003, 0x17c07c1f,
	0x1b00001f, 0x3fffc7ff, 0x18c0001f, 0x10006294, 0xe0e001fe, 0xe0e003fc,
	0xe0e007f8, 0xe0e00ff0, 0x1b80001f, 0x20000020, 0xe0f07ff0, 0xe0f07f00,
	0x80388400, 0x1b80001f, 0x20000300, 0x803b8400, 0x1b80001f, 0x20000300,
	0x80348400, 0x1b80001f, 0x20000104, 0x80308400, 0x80320400, 0x81f38407,
	0x81401801, 0xd80053c5, 0x17c07c1f, 0x18c0001f, 0x10006290, 0x1212841f,
	0xc0c01ec0, 0x12807c1f, 0xc0c01ec0, 0x1280041f, 0x18c0001f, 0x10006208,
	0x1212841f, 0xc0c01ec0, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c01ec0, 0x1280041f, 0x18c0001f, 0x10006200,
	0x1212841f, 0xc0c01ec0, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c01ec0, 0x1280041f, 0x81f10407, 0x81f48407,
	0xa1d60407, 0x1ac0001f, 0x55aa55aa, 0x10007c1f, 0xf0000000
};
static struct pcm_desc sodi_pcm = {
	.version	= "pcm_sodi_v3.0_new",
	.base		= sodi_binary,
	.size		= 677,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(30, 1, 0, 0),	/* FUNC_APSRC_WAKEUP */
	.vec1		= EVENT_VEC(31, 1, 0, 105),	/* FUNC_APSRC_SLEEP */
};
#endif

static struct pwr_ctrl sodi_ctrl = {
	.wake_src		= WAKE_SRC_FOR_SODI,
	
	.r0_ctrl_en		= 1,
	.r7_ctrl_en		= 1,

	.ca7_wfi0_en	= 1,
	.ca7_wfi1_en	= 1,
	.ca7_wfi2_en	= 1,
	.ca7_wfi3_en	= 1,
	.ca15_wfi0_en	= 1,
	.ca15_wfi1_en	= 1,
	.ca15_wfi2_en	= 1,
	.ca15_wfi3_en	= 1,

	/* SPM_AP_STANBY_CON */
	.wfi_op			= WFI_OP_AND,
	.mfg_req_mask		= 1,
	.lte_mask			= 1,
	.md2_req_mask		= 1,
#if (SODI_DVT_APxGPT)
	//.ca7top_idle_mask   = 1,
	//.ca15top_idle_mask  = 1,
	//.mcusys_idle_mask   = 1,
	//.disp_req_mask		= 1,
	.md1_req_mask		= 1,
	.conn_mask			= 1,
#endif	

    /* SPM_PCM_SRC_REQ */
#if 0 
    .pcm_apsrc_req      = 0,
    .pcm_f26m_req       = 0,
	.ccif0_to_ap_mask   = 1,
	.ccif0_to_md_mask   = 1,
	.ccif1_to_ap_mask   = 1,
	.ccif1_to_md_mask   = 1,
	.ccifmd_md1_event_mask = 1,
	.ccifmd_md2_event_mask = 1,
#endif

#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask		= 1,
#endif

#if SODI_DVT_STEP_BY_STEP
	.pcm_reserve		= 0x000001ff, //SPM DVT test step by step (will be defined by Hank)
#endif
};

struct spm_lp_scen __spm_sodi = {
#ifndef USE_DYNA_LOAD_SODI
	.pcmdesc	= &sodi_pcm,
#endif
	.pwrctrl	= &sodi_ctrl,
};

//0:power-down mode, 1:CG mode
static bool gSpm_SODI_mempll_pwr_mode = 1;

static bool gSpm_sodi_en=0;

extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);

extern void soidle_before_wfi(int cpu);
extern void soidle_after_wfi(int cpu);
extern void spm_i2c_control(u32 channel, bool onoff);

#if SPM_AEE_RR_REC
extern void aee_rr_rec_sodi_val(u32 val);
extern u32 aee_rr_curr_sodi_val(void);
#endif

static void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl)
{
    if (is_cpu_pdn(pwrctrl->pcm_flags)) {    
        //printk("\nSODI: enter mt_cpu_dormant(CPU_SODI_MODE)\n");
        mt_cpu_dormant(CPU_SODI_MODE);
        //printk("SODI: exit mt_cpu_dormant(CPU_SODI_MODE)\n");
    } else {
        //printk("\nSODI: enter legacy WFI\n");
        
        //enter WFI
        wfi_with_sync();
        
        //printk("SODI: exit legacy WFI\n");
    }
}

static void spm_sodi_pre_process(void)
{
#if 0
    /* set PMIC WRAP table for deepidle power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);	

    vsram_vosel_on_lb = pmic_get_register_value(PMIC_VSRAM_VOSEL_ON_LB);
    spm_write(SPM_PCM_RESERVE3,(pmic_get_register_value(PMIC_VSRAM_VOSEL_OFFSET)<<8)|pmic_get_register_value(PMIC_VSRAM_VOSEL_DELTA));//delta = 0v
    pmic_set_register_value(PMIC_VSRAM_VOSEL_ON_LB,(vsram_vosel_on_lb&0xff80)|0x28);//0.85v
#endif
}

static void spm_sodi_post_process(void)
{
#if 0    
    pmic_set_register_value(PMIC_VSRAM_VOSEL_ON_LB,vsram_vosel_on_lb);  
    
    /* set PMIC WRAP table for normal power control */
    mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);
#endif
}

void spm_go_to_sodi(u32 spm_flags, u32 spm_data)
{
    struct wake_status wakesta;
    unsigned long flags;
    struct mtk_irq_mask mask;
    wake_reason_t wr = WR_NONE;
#ifndef USE_DYNA_LOAD_SODI
    struct pcm_desc *pcmdesc = __spm_sodi.pcmdesc;
#else
    struct pcm_desc *pcmdesc;
#endif
    struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;

#ifdef USE_DYNA_LOAD_SODI
    if (dyna_load_pcm[DYNA_LOAD_PCM_SODI].ready)
	    pcmdesc = &(dyna_load_pcm[DYNA_LOAD_PCM_SODI].desc);
    else
	    BUG();
#endif

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(1<<SPM_SODI_ENTER);
#endif 
    
	if(gSpm_SODI_mempll_pwr_mode == 1)
	{
		spm_flags |= SPM_MEMPLL_CG_EN; //MEMPLL CG mode
	}
	else
	{
		spm_flags &= ~SPM_MEMPLL_CG_EN; //DDRPHY power down mode
	}

    set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
    
    //enable APxGPT timer
	soidle_before_wfi(0);
	
	lockdep_off();
    spin_lock_irqsave(&__spm_lock, flags);

    mt_irq_mask_all(&mask);
    mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
    mt_cirq_clone_gic();
    mt_cirq_enable();

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_ENTER_SPM_FLOW));
#endif   

    __spm_reset_and_init_pcm(pcmdesc);

	/*
	 * When commond-queue is in shut-down mode, SPM will hang if it tries to access commond-queue status.  
     * Follwoing patch is to let SODI driver to notify SPM that commond-queue is in shut-down mode or not to avoid above SPM hang issue. 
     * But, now display can automatically notify SPM that command-queue is shut-down or not, so following code is not needed anymore.
	 */
	#if 0 
    //check GCE
	if(clock_is_on(MT_CG_INFRA_GCE))
	{
		pwrctrl->pcm_flags &= ~SPM_DDR_HIGH_SPEED; 
	}
	else
	{
		pwrctrl->pcm_flags |= SPM_DDR_HIGH_SPEED; 
	}
	#endif

    __spm_kick_im_to_fetch(pcmdesc);
    
    __spm_init_pcm_register();
    
    __spm_init_event_vector(pcmdesc);
    
#if 0 //In D2, can not set apsrc_req bit in SODI. It is used by Vcore DVS for GPU 550M in HPM mode
    //Display set SPM_PCM_SRC_REQ[0]=1'b1 to force DRAM not enter self-refresh mode
   	if((spm_read(SPM_PCM_SRC_REQ)&0x00000001))
    {
    	pwrctrl->pcm_apsrc_req = 1;
    }
    else
    {
    	pwrctrl->pcm_apsrc_req = 0;
    }
#endif

    __spm_set_power_control(pwrctrl);
    
    __spm_set_wakeup_event(pwrctrl);
    
#if SODI_DVT_PCM_TIMER_DISABLE
	//PCM_Timer is enable in above '__spm_set_wakeup_event(pwrctrl);', disable PCM Timer here
	spm_write(SPM_PCM_CON1 ,spm_read(SPM_PCM_CON1)&(~CON1_PCM_TIMER_EN));
#endif

    spm_sodi_pre_process();
    
    __spm_kick_pcm_to_run(pwrctrl);
      
#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_ENTER_WFI));
#endif

    spm_trigger_wfi_for_sodi(pwrctrl);

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_LEAVE_WFI));
#endif  

    spm_sodi_post_process();
    
    __spm_get_wakeup_status(&wakesta);
    
    sodi_debug("emi-selfrefrsh cnt = %d, pcm_flag = 0x%x, SPM_PCM_RESERVE2 = 0x%x, SPM_PCM_MMDDR_MASK=0x%x, %s\n", 
    			spm_read(SPM_PCM_PASR_DPD_3), spm_read(SPM_PCM_FLAGS), spm_read(SPM_PCM_RESERVE2), spm_read(SPM_PCM_MMDDR_MASK), pcmdesc->version);
    
    __spm_clean_after_wakeup();
    
    wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);
    if (wr == WR_PCM_ASSERT)
    {
        sodi_err("PCM ASSERT AT %u (%s), r13 = 0x%x, debug_flag = 0x%x\n", wakesta.assert_pc, pcmdesc->version, wakesta.r13, wakesta.debug_flag);   
    }
    
#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(aee_rr_curr_sodi_val()|(1<<SPM_SODI_LEAVE_SPM_FLOW));
#endif  

    mt_cirq_flush();
    mt_cirq_disable();
    mt_irq_mask_restore(&mask);
	
    spin_unlock_irqrestore(&__spm_lock, flags);
    lockdep_on();

    //stop APxGPT timer and enable caore0 local timer
    soidle_after_wfi(0);  

#if SODI_DVT_SPM_MEM_RW_TEST
    {	
        static u32 magic_init = 0;
        int i =0;

        if(magic_init == 0){
		    magic_init++;
		    printk("magicNumArray:0x%p",magicArray);
	    }

    	for(i=0;i<16;i++)
    	{
    		if(magicArray[i]!=SODI_DVT_MAGIC_NUM)
    		{
    			printk("Error: sodi magic number no match!!!");
    			ASSERT(0);
    		}
    	}
    	
    	if (i>=16)
    	    printk("SODI_DVT_SPM_MEM_RW_TEST pass (count = %d)\n", magic_init);
    }
#endif

#if SPM_AEE_RR_REC
    aee_rr_rec_sodi_val(0);
#endif 
}

void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
	//printk("[SODI]set pwr_mode = %d\n",pwr_mode);
    gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

void spm_enable_sodi(bool en)
{
    gSpm_sodi_en=en;
}

bool spm_get_sodi_en(void)
{
    return gSpm_sodi_en;
}

#if SPM_AEE_RR_REC
static void spm_sodi_aee_init(void)
{
    aee_rr_rec_sodi_val(0);
}
#endif

#if SPM_USE_TWAM_DEBUG
#define SPM_TWAM_MONITOR_TICK 333333
static void twam_handler(struct twam_sig *twamsig)
{
	spm_crit("sig_high = %u%%  %u%%  %u%%  %u%%, r13 = 0x%x\n",
		 get_percent(twamsig->sig0,SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig1,SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig2,SPM_TWAM_MONITOR_TICK),
		 get_percent(twamsig->sig3,SPM_TWAM_MONITOR_TICK),
		 spm_read(SPM_PCM_REG13_DATA));
}
#endif

void spm_sodi_init(void)
{
#if SPM_USE_TWAM_DEBUG
	unsigned long flags;
	struct twam_sig twamsig = {
		.sig0 = 10,	/* disp_req */
		.sig1 = 23,	/* self-refresh */
		.sig2 = 25,	/* md2_srcclkena */
		.sig3 = 21,	/* md2_apsrc_req_mux */		
	};
#endif
	
#if SPM_AEE_RR_REC
    spm_sodi_aee_init();
#endif

#if SPM_USE_TWAM_DEBUG	
    #if 0
	spin_lock_irqsave(&__spm_lock, flags);
	spm_write(SPM_AP_STANBY_CON, spm_read(SPM_AP_STANBY_CON) | ASC_MD_DDR_EN_SEL);
	spin_unlock_irqrestore(&__spm_lock, flags);
    #endif
    
	spm_twam_register_handler(twam_handler);
	spm_twam_enable_monitor(&twamsig, false,SPM_TWAM_MONITOR_TICK);
#endif
}

MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
