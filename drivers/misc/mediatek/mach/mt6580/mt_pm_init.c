#include <linux/pm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "mach/irqs.h"
#include "mach/sync_write.h"
#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_spm.h"
#include "mach/mt_sleep.h"
#include "mach/mt_dcm.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_dormant.h"
#include "mach/mt_cpuidle.h"
#include "mach/mt_clkbuf_ctl.h"


#define pminit_write(addr, val)         mt_reg_sync_writel((val), ((void *)(addr)))
#define pminit_read(addr)               __raw_readl(IOMEM(addr))

extern int mt_clkmgr_init(void);
extern void mt_idle_init(void);
extern void mt_power_off(void);
extern void mt_dcm_init(void);

#define TOPCK_LDVT

#ifdef TOPCK_LDVT
/***************************
*For TOPCKGen Meter LDVT Test
****************************/

#define DEBUG_FQMTR
#ifdef DEBUG_FQMTR

#ifdef __KERNEL__ // for kernel

#define FQMTR_TAG     "Power/swap"

#define fqmtr_err(fmt, args...)       \
    pr_debug(fmt, ##args)
#define fqmtr_warn(fmt, args...)      \
    pr_debug(fmt, ##args)
#define fqmtr_info(fmt, args...)      \
    pr_debug(fmt, ##args)
#define fqmtr_dbg(fmt, args...)       \
    pr_debug(fmt, ##args)
#define fqmtr_ver(fmt, args...)       \
    pr_debug(fmt, ##args)

#define fqmtr_read(addr)            __raw_readl(IOMEM(addr))
#define fqmtr_write(addr, val)      mt_reg_sync_writel(val, addr)

#else // for preloader

#define fqmtr_err(fmt, args...)       \
    print(fmt, ##args)
#define fqmtr_warn(fmt, args...)      \
    print(fmt, ##args)
#define fqmtr_info(fmt, args...)      \
    print(fmt, ##args)
#define fqmtr_dbg(fmt, args...)       \
    print(fmt, ##args)
#define fqmtr_ver(fmt, args...)       \
    print(fmt, ##args)

#define fqmtr_read(addr)            DRV_Reg32(addr)
#define fqmtr_write(addr, val)      DRV_WriteReg32(addr, val)

#endif

#if 1 // using clk_cksys_base
#define FREQ_MTR_CTRL_REG               (CLK_TOPCKSYS_BASE + 0x10)
#define FREQ_MTR_CTRL_RDATA		(CLK_TOPCKSYS_BASE + 0x14)
#define TEST_DBG_CTRL			(CLK_TOPCKSYS_BASE + 0x38)
#else // using hardcode address for rainier
#define FREQ_MTR_CTRL_REG               (0x10000000 + 0x10)
#define FREQ_MTR_CTRL_RDATA		(0x10000000 + 0x14)
#endif

#define RG_FQMTR_CKDIV_GET(x)           (((x) >> 28) & 0x3)
#define RG_FQMTR_CKDIV_SET(x)           (((x)& 0x3) << 28)
#define RG_FQMTR_FIXCLK_SEL_GET(x)      (((x) >> 24) & 0x3)
#define RG_FQMTR_FIXCLK_SEL_SET(x)      (((x)& 0x3) << 24)
#define RG_FQMTR_MONCLK_SEL_GET(x)      (((x) >> 16) & 0x3f)
#define RG_FQMTR_MONCLK_SEL_SET(x)      (((x)& 0x3f) << 16)
#define RG_FQMTR_MONCLK_EN_GET(x)       (((x) >> 15) & 0x1)
#define RG_FQMTR_MONCLK_EN_SET(x)       (((x)& 0x1) << 15)
#define RG_FQMTR_MONCLK_RST_GET(x)      (((x) >> 14) & 0x1)
#define RG_FQMTR_MONCLK_RST_SET(x)      (((x)& 0x1) << 14)
#define RG_FQMTR_MONCLK_WINDOW_GET(x)   (((x) >> 0) & 0xfff)
#define RG_FQMTR_MONCLK_WINDOW_SET(x)   (((x)& 0xfff) << 0)

#define RG_FQMTR_CKDIV_DIV_2    0
#define RG_FQMTR_CKDIV_DIV_4    1
#define RG_FQMTR_CKDIV_DIV_8    2
#define RG_FQMTR_CKDIV_DIV_16   3

#define RG_FQMTR_FIXCLK_26MHZ   0
#define RG_FQMTR_FIXCLK_32KHZ   2

enum rg_fqmtr_monclk
{
    RG_FQMTR_MONCLK_PDN = 0,
    RG_FQMTR_MONCLK_MAINPLL_DIV8,
    RG_FQMTR_MONCLK_MAINPLL_DIV11,
    RG_FQMTR_MONCLK_MAINPLL_DIV12,
    RG_FQMTR_MONCLK_MAINPLL_DIV20,
    RG_FQMTR_MONCLK_MAINPLL_DIV7,
    RG_FQMTR_MONCLK_MAINPLL_DIV16,
    RG_FQMTR_MONCLK_MAINPLL_DIV24,
    RG_FQMTR_MONCLK_NFI2X,
    RG_FQMTR_MONCLK_WHPLL,
    RG_FQMTR_MONCLK_WPLL,
    RG_FQMTR_MONCLK_26MHZ,
    RG_FQMTR_MONCLK_USB_48MHZ,
    RG_FQMTR_MONCLK_EMI1X ,
    RG_FQMTR_MONCLK_AP_INFRA_FAST_BUS,
    RG_FQMTR_MONCLK_SMI_MMSYS,
    RG_FQMTR_MONCLK_UART0,
    RG_FQMTR_MONCLK_UART1,
    RG_FQMTR_MONCLK_GPU,
    RG_FQMTR_MONCLK_MSDC0,
    RG_FQMTR_MONCLK_MSDC1,
    RG_FQMTR_MONCLK_AD_DSI0_LNTC_DSICLK,
    RG_FQMTR_MONCLK_AD_MPPLL_TST_CK,
    RG_FQMTR_MONCLK_AP_PLLLGP_TST_CK,
    RG_FQMTR_MONCLK_52MHZ,
    RG_FQMTR_MONCLK_ARMPLL,
    RG_FQMTR_MONCLK_32KHZ,
    RG_FQMTR_MONCLK_AD_MEMPLL_MONCLK,
    RG_FQMTR_MONCLK_AD_MEMPLL2_MONCLK,
    RG_FQMTR_MONCLK_AD_MEMPLL3_MONCLK,
    RG_FQMTR_MONCLK_AD_MEMPLL4_MONCLK,
    RG_FQMTR_MONCLK_RESERVED,
    RG_FQMTR_MONCLK_CAM_SENINF,
    RG_FQMTR_MONCLK_SCAM,
    RG_FQMTR_MONCLK_PWM_MMSYS,
    RG_FQMTR_MONCLK_DDRPHYCFG,
    RG_FQMTR_MONCLK_PMIC_SPI,
    RG_FQMTR_MONCLK_SPI,
    RG_FQMTR_MONCLK_104MHZ,
    RG_FQMTR_MONCLK_USB_78MHZ,
    RG_FQMTR_MONCLK_MAX,
};

const char *rg_fqmtr_monclk_name[] =
{
    [RG_FQMTR_MONCLK_PDN]                   = "power done",
    [RG_FQMTR_MONCLK_MAINPLL_DIV8]          = "mainpll div8",
    [RG_FQMTR_MONCLK_MAINPLL_DIV11]         = "mainpll div11",
    [RG_FQMTR_MONCLK_MAINPLL_DIV12]         = "mainpll div12",
    [RG_FQMTR_MONCLK_MAINPLL_DIV20]         = "mainpll div20",
    [RG_FQMTR_MONCLK_MAINPLL_DIV7]          = "mainpll div7",
    [RG_FQMTR_MONCLK_MAINPLL_DIV16]         = "mainpll div16",
    [RG_FQMTR_MONCLK_MAINPLL_DIV24]         = "mainpll div24",
    [RG_FQMTR_MONCLK_NFI2X]                 = "nfi2x",
    [RG_FQMTR_MONCLK_WHPLL]                 = "whpll",
    [RG_FQMTR_MONCLK_WPLL]                  = "wpll",
    [RG_FQMTR_MONCLK_26MHZ]                 = "26MHz",
    [RG_FQMTR_MONCLK_USB_48MHZ]             = "USB 48MHz",
    [RG_FQMTR_MONCLK_EMI1X]                 = "emi1x",
    [RG_FQMTR_MONCLK_AP_INFRA_FAST_BUS]     = "AP infra fast bus",
    [RG_FQMTR_MONCLK_SMI_MMSYS]             = "smi mmsys",
    [RG_FQMTR_MONCLK_UART0]                 = "uart0",
    [RG_FQMTR_MONCLK_UART1]                 = "uart1",
    [RG_FQMTR_MONCLK_GPU]                   = "gpu",
    [RG_FQMTR_MONCLK_MSDC0]                 = "msdc0",
    [RG_FQMTR_MONCLK_MSDC1]                 = "msdc1",
    [RG_FQMTR_MONCLK_AD_DSI0_LNTC_DSICLK]   = "AD_DSI0_LNTC_DSICLK(mipi)",
    [RG_FQMTR_MONCLK_AD_MPPLL_TST_CK]       = "AD_MPPLL_TST_CK(mipi)",
    [RG_FQMTR_MONCLK_AP_PLLLGP_TST_CK]      = "AP_PLLLGP_TST_CK",
    [RG_FQMTR_MONCLK_52MHZ]                 = "52MHz",
    [RG_FQMTR_MONCLK_ARMPLL]                = "ARMPLL",
    [RG_FQMTR_MONCLK_32KHZ]                 = "32Khz",
    [RG_FQMTR_MONCLK_AD_MEMPLL_MONCLK]      = "AD_MEMPLL_MONCLK",
    [RG_FQMTR_MONCLK_AD_MEMPLL2_MONCLK]     = "AD_MEMPLL_2MONCLK",
    [RG_FQMTR_MONCLK_AD_MEMPLL3_MONCLK]     = "AD_MEMPLL_3MONCLK",
    [RG_FQMTR_MONCLK_AD_MEMPLL4_MONCLK]     = "AD_MEMPLL_4MONCLK",
    [RG_FQMTR_MONCLK_RESERVED]              = "Reserved",
    [RG_FQMTR_MONCLK_CAM_SENINF]            = "CAM seninf",
    [RG_FQMTR_MONCLK_SCAM]                  = "SCAM",
    [RG_FQMTR_MONCLK_PWM_MMSYS]             = "PWM mmsys",
    [RG_FQMTR_MONCLK_DDRPHYCFG]             = "ddrphycfg",
    [RG_FQMTR_MONCLK_PMIC_SPI]              = "PMIC SPI",
    [RG_FQMTR_MONCLK_SPI]                   = "SPI",
    [RG_FQMTR_MONCLK_104MHZ]                = "104MHz",
    [RG_FQMTR_MONCLK_USB_78MHZ]             = "USB 78MHz",
    [RG_FQMTR_MONCLK_MAX]                   = "MAX",
};

/*
#define RG_FQMTR_MONCLK_PDN                 0
#define RG_FQMTR_MONCLK_MAINPLL_DIV8        1
#define RG_FQMTR_MONCLK_MAINPLL_DIV11       2
#define RG_FQMTR_MONCLK_MAINPLL_DIV12       3
#define RG_FQMTR_MONCLK_MAINPLL_DIV20       4
#define RG_FQMTR_MONCLK_MAINPLL_DIV7        5
#define RG_FQMTR_MONCLK_MAINPLL_DIV16       6
#define RG_FQMTR_MONCLK_MAINPLL_DIV24       7
#define RG_FQMTR_MONCLK_NFI2X               8
#define RG_FQMTR_MONCLK_WHPLL               9
#define RG_FQMTR_MONCLK_WPLL                10
#define RG_FQMTR_MONCLK_26MHZ               11
#define RG_FQMTR_MONCLK_USB_48MHZ           12
#define RG_FQMTR_MONCLK_EMI1X               13
#define RG_FQMTR_MONCLK_AP_INFRA_FAST_BUS   14
#define RG_FQMTR_MONCLK_SMI_MMSYS           15
#define RG_FQMTR_MONCLK_UART0               16
#define RG_FQMTR_MONCLK_UART1               17
#define RG_FQMTR_MONCLK_GPU                 18
#define RG_FQMTR_MONCLK_MSDC0               19
#define RG_FQMTR_MONCLK_MSDC1               20
#define RG_FQMTR_MONCLK_AD_DSI0_LNTC_DSICLK 21
#define RG_FQMTR_MONCLK_AD_MPPLL_TST_CK     22
#define RG_FQMTR_MONCLK_AP_PLLLGP_TST_CK    23
#define RG_FQMTR_MONCLK_52MHZ               24
#define RG_FQMTR_MONCLK_ARMPLL              25
#define RG_FQMTR_MONCLK_32KHZ               26
#define RG_FQMTR_MONCLK_AD_MEMPLL_MONCLK    27
#define RG_FQMTR_MONCLK_AD_MEMPLL2_MONCLK   28
#define RG_FQMTR_MONCLK_AD_MEMPLL3_MONCLK   29
#define RG_FQMTR_MONCLK_AD_MEMPLL4_MONCLK   30
#define RG_FQMTR_MONCLK_RESERVED            31
#define RG_FQMTR_MONCLK_CAM_SENINF          32
#define RG_FQMTR_MONCLK_SCAM                33
#define RG_FQMTR_MONCLK_PWM_MMSYS           34
#define RG_FQMTR_MONCLK_DDRPHYCFG           35
#define RG_FQMTR_MONCLK_PMIC_SPI            36
#define RG_FQMTR_MONCLK_SPI                 37
#define RG_FQMTR_MONCLK_104MHZ              38
#define RG_FQMTR_MONCLK_USB_78MHZ           39
*/

#define RG_FQMTR_EN     1
#define RG_FQMTR_RST    1

#define RG_FRMTR_WINDOW     0x100

unsigned int do_fqmtr_ctrl(int fixclk, int monclk_sel)
{
    u32 value = 0;

    BUG_ON(!((fixclk == RG_FQMTR_FIXCLK_26MHZ) | (fixclk == RG_FQMTR_FIXCLK_32KHZ)));

    // out for spec
    if (monclk_sel == RG_FQMTR_MONCLK_ARMPLL)
    DRV_WriteReg32(TEST_DBG_CTRL, DRV_Reg32(TEST_DBG_CTRL) & ~0xff | 0x1);

    // reset
    DRV_WriteReg32(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
    // reset deassert
    DRV_WriteReg32(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));
    // set window and target
    DRV_WriteReg32(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_WINDOW_SET(RG_FRMTR_WINDOW) |
                RG_FQMTR_MONCLK_SEL_SET(monclk_sel) |
                RG_FQMTR_FIXCLK_SEL_SET(fixclk)	|
		RG_FQMTR_MONCLK_EN_SET(RG_FQMTR_EN));
   // gpt_busy_wait_us(100);
	while(DRV_Reg32(FREQ_MTR_CTRL_RDATA)& 0x80000000){
		value++;
		if (value > 2000)
			break; 
	}	
    value = DRV_Reg32(FREQ_MTR_CTRL_RDATA);

    // reset
    DRV_WriteReg32(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(RG_FQMTR_RST));
    // reset deassert
    DRV_WriteReg32(FREQ_MTR_CTRL_REG, RG_FQMTR_MONCLK_RST_SET(!RG_FQMTR_RST));

    if (monclk_sel == RG_FQMTR_MONCLK_ARMPLL)
        value *= 2;
    if (fixclk == RG_FQMTR_FIXCLK_26MHZ)
        return ((26 * value) / (RG_FRMTR_WINDOW + 1));
    else
        return ((32000 * value) / (RG_FRMTR_WINDOW + 1));

}


void dump_fqmtr(void)
{
    int i = 0;
    unsigned int ret;
/*
    // fixclk = RG_FQMTR_FIXCLK_32KHZ
    for (i = 0; i < RG_FQMTR_MONCLK_MAX; i++)
    {
        if (i == RG_FQMTR_MONCLK_RESERVED)
            continue;
        ret = do_fqmtr_ctrl(RG_FQMTR_FIXCLK_32KHZ, i);
        fqmtr_dbg("%s - %d", rg_fqmtr_monclk_name[i], ret);
    }
*/
    // fixclk = RG_FQMTR_FIXCLK_26MHZ
    for (i = 0; i < RG_FQMTR_MONCLK_MAX; i++)
    {
        if (i == RG_FQMTR_MONCLK_RESERVED)
            continue;
        ret = do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, i);
        fqmtr_dbg("%s - %d MHz\n", rg_fqmtr_monclk_name[i], ret);
    }
}
#endif /* DEBUG_FQMTR */
  
static int fqmtr_read_fs(struct seq_file *m, void *v)
{
    int i;

    for (i = 0; i < RG_FQMTR_MONCLK_MAX; i++)
    {
        if (i == RG_FQMTR_MONCLK_RESERVED)
            continue;
        seq_printf(m, "%d:%s - %d\n", i, rg_fqmtr_monclk_name[i], do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, i));
    }

    return 0;
}

static int proc_fqmtr_open(struct inode *inode, struct file *file)
{
    return single_open(file, fqmtr_read_fs, NULL);
}
static const struct file_operations fqmtr_fops = {
    .owner = THIS_MODULE,
    .open  = proc_fqmtr_open,
    .read  = seq_read,
};

#endif

/*********************************************************************
 * FUNCTION DEFINATIONS
 ********************************************************************/

unsigned int mt_get_emi_freq(void)
{
	return do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, RG_FQMTR_MONCLK_EMI1X);
}
EXPORT_SYMBOL(mt_get_emi_freq);

unsigned int mt_get_bus_freq(void)
{
	return do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, RG_FQMTR_MONCLK_AP_INFRA_FAST_BUS);
}
EXPORT_SYMBOL(mt_get_bus_freq);

unsigned int mt_get_cpu_freq(void)
{
	return do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, RG_FQMTR_MONCLK_ARMPLL);

}
EXPORT_SYMBOL(mt_get_cpu_freq);

unsigned int mt_get_mmclk_freq(void)
{
	return do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, RG_FQMTR_MONCLK_EMI1X);
}
EXPORT_SYMBOL(mt_get_mmclk_freq);

unsigned int mt_get_mfgclk_freq(void)
{
	return do_fqmtr_ctrl(RG_FQMTR_FIXCLK_26MHZ, RG_FQMTR_MONCLK_GPU);
}
EXPORT_SYMBOL(mt_get_mfgclk_freq);

static int cpu_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_cpu_freq());
    return 0;
}

//static int bigcpu_speed_dump_read(struct seq_file *m, void *v)
//{
//    seq_printf(m, "%d\n", mt_get_bigcpu_freq());
//    return 0;
//}

static int emi_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_emi_freq());
    return 0;
}

static int bus_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_bus_freq());
    return 0;
}

static int mmclk_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_mmclk_freq());
    return 0;
}

static int mfgclk_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_mfgclk_freq());
    return 0;
}

static int proc_cpu_open(struct inode *inode, struct file *file)
{
    return single_open(file, cpu_speed_dump_read, NULL);
}
static const struct file_operations cpu_fops = {
    .owner = THIS_MODULE,
    .open  = proc_cpu_open,
    .read  = seq_read,
};

//static int proc_bigcpu_open(struct inode *inode, struct file *file)
//{
//    return single_open(file, bigcpu_speed_dump_read, NULL);
//}
//static const struct file_operations bigcpu_fops = {
//    .owner = THIS_MODULE,
//    .open  = proc_bigcpu_open,
//    .read  = seq_read,
//};

static int proc_emi_open(struct inode *inode, struct file *file)
{
    return single_open(file, emi_speed_dump_read, NULL);
}
static const struct file_operations emi_fops = {
    .owner = THIS_MODULE,
    .open  = proc_emi_open,
    .read  = seq_read,
};

static int proc_bus_open(struct inode *inode, struct file *file)
{
    return single_open(file, bus_speed_dump_read, NULL);
}
static const struct file_operations bus_fops = {
    .owner = THIS_MODULE,
    .open  = proc_bus_open,
    .read  = seq_read,
};

static int proc_mmclk_open(struct inode *inode, struct file *file)
{
    return single_open(file, mmclk_speed_dump_read, NULL);
}
static const struct file_operations mmclk_fops = {
    .owner = THIS_MODULE,
    .open  = proc_mmclk_open,
    .read  = seq_read,
};

static int proc_mfgclk_open(struct inode *inode, struct file *file)
{
    return single_open(file, mfgclk_speed_dump_read, NULL);
}
static const struct file_operations mfgclk_fops = {
    .owner = THIS_MODULE,
    .open  = proc_mfgclk_open,
    .read  = seq_read,
};



static int __init mt_power_management_init(void)
{
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *pm_init_dir = NULL;

    pm_power_off = mt_power_off;

    #if !defined (CONFIG_FPGA_CA7)
     //FIXME: for FPGA early porting
    #if 0
    pr_debug("Power/PM_INIT Bus Frequency = %d KHz\n", mt_get_bus_freq());
    #endif

    // CPU Dormant Driver Init
    mt_cpu_dormant_init();

    // SPM driver init
    spm_module_init();

    // Sleep driver init (for suspend)
    slp_module_init();

    mt_clkmgr_init();

    //mt_pm_log_init(); // power management log init

    //FIXME: for FPGA early porting

//    mt_dcm_init(); // dynamic clock management init


    pm_init_dir = proc_mkdir("pm_init", NULL);
    pm_init_dir = proc_mkdir("pm_init", NULL);
    if (!pm_init_dir)
    {
        pr_err("[%s]: mkdir /proc/pm_init failed\n", __FUNCTION__);
    }
    else
    {
        entry = proc_create("cpu_speed_dump", S_IRUGO, pm_init_dir, &cpu_fops);

        //entry = proc_create("bigcpu_speed_dump", S_IRUGO, pm_init_dir, &bigcpu_fops);

        entry = proc_create("emi_speed_dump", S_IRUGO, pm_init_dir, &emi_fops);

        entry = proc_create("bus_speed_dump", S_IRUGO, pm_init_dir, &bus_fops);

        entry = proc_create("mmclk_speed_dump", S_IRUGO, pm_init_dir, &mmclk_fops);

        entry = proc_create("mfgclk_speed_dump", S_IRUGO, pm_init_dir, &mfgclk_fops);
#ifdef TOPCK_LDVT
        entry = proc_create("fqmtr_test", S_IRUGO, pm_init_dir, &fqmtr_fops);
#endif
    }

    #endif

    return 0;
}

arch_initcall(mt_power_management_init);


#if !defined (MT_DORMANT_UT)
static int __init mt_pm_late_init(void)
{
	mt_idle_init();
	clk_buf_init();
	return 0;
}

late_initcall(mt_pm_late_init);
#endif //#if !defined (MT_DORMANT_UT)


MODULE_DESCRIPTION("MTK Power Management Init Driver");
MODULE_LICENSE("GPL");
