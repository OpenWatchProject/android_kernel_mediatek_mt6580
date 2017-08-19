/**
 * @file mt_cpufreq.h
 * @brief CPU DVFS driver interface
 */

#ifndef __MT_CPUFREQ_H__
#define __MT_CPUFREQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MT_CPUFREQ_C__
#define CPUFREQ_EXTERN
#else
#define CPUFREQ_EXTERN extern
#endif

/*=============================================================*/
// Include files
/*=============================================================*/

// system includes

// project includes

// local includes

// forward references


/*=============================================================*/
// Macro definition
/*=============================================================*/


/*=============================================================*/
// Type definition
/*=============================================================*/
// Fequency meter
typedef struct
{
	//- input
	unsigned int divider;
	unsigned int ref_clk_sel;
	unsigned int mon_sel;
	unsigned int mon_len_in_ref_clk;
	unsigned int polling_to_getresult;

	//- output
	unsigned int result_in_count;

	//- internal control
	unsigned int owner;
} FREQMETER_CTRL;



#define FREQMETER_SUCCESS				0

#define TOPCKGEN_BASE				0x10000000
#define FREQ_MTR_CTRL			(TOPCKGEN_BASE + 0x0010)
#define FREQ_MTR_DATA			(TOPCKGEN_BASE + 0x0014)
#define TEST_DBG_CTRL_REG       (TOPCKGEN_BASE + 0x38)

#define MCUSYS_CFGREG_BASE			0x10200000
#define DBG_CTRL						(MCUSYS_CFGREG_BASE + 0x0080)

#define TEST_DBG_CTRL       			(TOPCKGEN_BASE + 0x0038)

#define MCUCFG_REGBASE          (0x10200000)
#define DBG_CTRL_REG            (MCUCFG_REGBASE + 0x80)

#define INFRACFG_AO_REGBASE     (0x10001000)
#define INFRA_AO_DBG_CON0_REG   (INFRACFG_AO_REGBASE + 0x500)

#define INFRA_SYS_CFG_AO_BASE		0x10001000
#define INFRA_AO_DBG_CON0				(INFRA_SYS_CFG_AO_BASE + 0x0500)

#define FQMTR_APMCU_CLOCK_PRE_DIV (4)

#define  FQMTR_SRC_APMCU_CLOCK					0x19

#define RG_FQMTR_CKDIV_MASK			0x3
 #define RG_FQMTR_CKDIV_BIT			28

  #define RG_FQMTR_FIXCLK_SEL_MASK		0x3
  #define RG_FQMTR_FIXCLK_SEL_BIT		24

  #define RG_FQMTR_MONCLK_SEL_MASK	0x1F
  #define RG_FQMTR_MONCLK_SEL_BIT		16

  #define RG_FQMTR_WINDOW_MASK		0xFFF
  #define RG_FQMTR_WINDOW_BIT			0
  
#define RG_FQMTR_DATA_MASK			0xFFFF
#define RG_FQMTR_DATA_BIT			0

#define RG_FQMTR_BUSY_MASK			0x1
#define RG_FQMTR_BUSY_BIT			31
#define RG_FQMTR_RST_BIT			14

  #define RG_FQMTR_EN_BIT				15
  
//- ref_clk_sel
#define  RG_FQMTR_CKDIV_D1				0x00
#define  RG_FQMTR_CKDIV_D2				0x01
#define  RG_FQMTR_CKDIV_D4				0x02
#define  RG_FQMTR_CKDIV_D8				0x03
#define  RG_FQMTR_CKDIV_D16				0x04
//- divider
#define	 RG_FQMTR_FIXCLK_SEL_26MHZ		0x00
#define	 RG_FQMTR_FIXCLK_SEL_32KHZ		0x02

//- freq meter error code
#define FREQMETER_SUCCESS				0
#define FREQMETER_NO_RESOURCE			-1
#define FREQMETER_NOT_OWNER				-2
#define FREQMETER_COUNTING				-3
#define FREQMETER_OUT_BOUNDARY			-255

/*=============================================================*/
enum mt_cpu_dvfs_id {
    // K2 has little core only
    MT_CPU_DVFS_LITTLE,
    NR_MT_CPU_DVFS,
};

enum top_ckmuxsel {
    TOP_CKMUXSEL_CLKSQ   = 0, /* i.e. reg setting */
    TOP_CKMUXSEL_ARMPLL  = 1,
    TOP_CKMUXSEL_UNIVPLL = 2,
    TOP_CKMUXSEL_MAINPLL = 3,

    NR_TOP_CKMUXSEL,
} ;

/*
 * PMIC_WRAP
 */

/* Phase */
enum pmic_wrap_phase_id {
    PMIC_WRAP_PHASE_NORMAL,
    PMIC_WRAP_PHASE_DEEPIDLE,

    NR_PMIC_WRAP_PHASE,
};

/* IDX mapping */
enum {
    IDX_NM_VCORE,		/* 0 */

    NR_IDX_NM,
};

enum {
	IDX_DI_VCORE_NORMAL,		/* 0 */ /* PMIC_WRAP_PHASE_DEEPIDLE*/
	IDX_DI_VCORE_SLEEP,			/* 1 */

    NR_IDX_DI,
};
typedef void (*cpuVoltsampler_func)(enum mt_cpu_dvfs_id , unsigned int mv);
/*=============================================================*/
// Global variable definition
/*=============================================================*/


/*=============================================================*/
// Global function definition
/*=============================================================*/

/* PMIC WRAP */
CPUFREQ_EXTERN void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase);
CPUFREQ_EXTERN void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata);
CPUFREQ_EXTERN void mt_cpufreq_apply_pmic_cmd(int idx);

/* PTP-OD */
CPUFREQ_EXTERN unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl);
CPUFREQ_EXTERN void mt_cpufreq_restore_default_volt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id);

/* Thermal */
CPUFREQ_EXTERN void mt_cpufreq_thermal_protect(unsigned int limited_power);

/* SDIO */
CPUFREQ_EXTERN void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled);
CPUFREQ_EXTERN void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt);
CPUFREQ_EXTERN unsigned int mt_vcore_dvfs_volt_get_by_sdio(void);

CPUFREQ_EXTERN unsigned int mt_get_cur_volt_vcore_ao(void);
//CPUFREQ_EXTERN unsigned int mt_get_cur_volt_vcore_pdn(void);

/* Generic */
CPUFREQ_EXTERN int mt_cpufreq_state_set(int enabled);
CPUFREQ_EXTERN int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel);
CPUFREQ_EXTERN enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);
CPUFREQ_EXTERN bool mt_cpufreq_earlysuspend_status_get(void);

CPUFREQ_EXTERN void mt_cpufreq_set_ramp_down_count_const(enum mt_cpu_dvfs_id id, int count);

#ifndef __KERNEL__
CPUFREQ_EXTERN int mt_cpufreq_pdrv_probe(void);
CPUFREQ_EXTERN int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq);
CPUFREQ_EXTERN unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN unsigned int dvfs_get_cpu_volt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int volt);
CPUFREQ_EXTERN void dvfs_set_gpu_volt(int pmic_val);
CPUFREQ_EXTERN void dvfs_set_vcore_ao_volt(int pmic_val);
//CPUFREQ_EXTERN void dvfs_set_vcore_pdn_volt(int pmic_val);
CPUFREQ_EXTERN void dvfs_disable_by_ptpod(int id);
CPUFREQ_EXTERN void dvfs_enable_by_ptpod(int id);
#endif /* ! __KERNEL__ */

#undef CPUFREQ_EXTERN

#ifdef __cplusplus
}
#endif

#endif // __MT_CPUFREQ_H__
