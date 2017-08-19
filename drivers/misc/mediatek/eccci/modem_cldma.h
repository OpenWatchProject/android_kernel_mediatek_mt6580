#ifndef __MODEM_CD_H__
#define __MODEM_CD_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>

#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>

/*
  * hardcode, max queue number should be synced with port array in port_cfg.c
  * following number should sync with MAX_TXQ/RXQ_NUM in ccci_core.h and bitmask in modem_cldma.c
  */
#define CLDMA_TXQ_NUM 8
#define CLDMA_RXQ_NUM 8

/* CLDMA feature options:
 * CHECKSUM_SIZE: 0 to disable checksum function, non-zero for number of checksum bytes
 * CLDMA_TIMER_LOOP: user a timer to collect TX_DONE skb for network queues, and use workqueue
 *     for normal queues; if this is not defined, use delayed_work for all queues.
 * CLDMA_NO_TX_IRQ: mask all TX interrupts, collect TX_DONE skb when get Rx interrupt or Tx busy.
 * ENABLE_CLDMA_TIMER: use a timer to detect TX packet sent or not. not usable if TX interrupts are masked.
 */
#define CHECKSUM_SIZE 0 /* 12 */
//#define CLDMA_TIMER_LOOP 10 //ms
//#define CLDMA_NO_TX_IRQ
#ifndef CLDMA_NO_TX_IRQ
#define ENABLE_CLDMA_TIMER
#endif

struct md_cd_queue {
	unsigned char index;
	struct ccci_modem *modem;
	struct ccci_port *napi_port;

	/*
	 * Tx has 2 writers: 1 for packet fill and 1 for packet free, so an extra pointer and a lock 
	 * is used. Tx also has 1 reader: CLDMA OUT.
	 * Rx has 1 reader: packet read&free, and 1 writer: CLDMA IN.
	 */
	struct list_head *tr_ring; /* a "pure" ring of request, NO "head" */
	struct ccci_request *tr_done;
	struct ccci_request *tx_xmit;
	int free_slot; // counter, only for Tx
	spinlock_t ring_lock; // lock for the counter, only for Tx
	wait_queue_head_t req_wq; // only for Tx
	int budget; // ring buffer size

	struct workqueue_struct *worker;
	struct work_struct cldma_rx_work;
#ifdef CLDMA_TIMER_LOOP
	struct hrtimer cldma_poll_timer; /* only for Tx */
	struct work_struct cldma_tx_work;
#else
	struct delayed_work cldma_tx_work;
#endif

#ifdef ENABLE_CLDMA_TIMER
	struct timer_list timeout_timer;
    unsigned long long timeout_start;
    unsigned long long timeout_end;
#endif
	u16 debug_id;
	DIRECTION dir;
	unsigned int busy_count;
};

#define QUEUE_LEN(a) (sizeof(a)/sizeof(struct md_cd_queue))

struct md_cd_ctrl {
	struct md_cd_queue txq[CLDMA_TXQ_NUM];
	struct md_cd_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	atomic_t reset_on_going;
	atomic_t wdt_enabled;
	char   trm_wakelock_name[32];
	struct wake_lock trm_wake_lock;
	char   peer_wakelock_name[32];
	struct wake_lock peer_wake_lock;
	struct work_struct ccif_work;
	struct delayed_work ccif_delayed_work;
	struct timer_list bus_timeout_timer;
	spinlock_t cldma_timeout_lock; // this lock is using to protect CLDMA, not only for timeout checking
	struct work_struct cldma_irq_work;
	struct workqueue_struct *cldma_irq_worker;
	int channel_id; // CCIF channel
	struct work_struct wdt_work;
#if TRAFFIC_MONITOR_INTERVAL
	unsigned tx_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned rx_traffic_monitor[CLDMA_RXQ_NUM];
	unsigned tx_pre_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned long long tx_done_last_start_time[CLDMA_TXQ_NUM];
	unsigned int tx_done_last_count[CLDMA_TXQ_NUM];

	struct timer_list traffic_monitor;
	unsigned long traffic_stamp;
#endif

	struct dma_pool *tgpd_dmapool;
	struct dma_pool *rgpd_dmapool;
	void __iomem *cldma_ap_ao_base;
	void __iomem *cldma_md_ao_base;
	void __iomem *cldma_ap_pdn_base;
	void __iomem *cldma_md_pdn_base;    
	void __iomem *md_rgu_base;
	void __iomem *md_boot_slave_Vector;
	void __iomem *md_boot_slave_Key;
	void __iomem *md_boot_slave_En;
	void __iomem *md_global_con0;
	void __iomem *ap_ccif_base;
	void __iomem *md_ccif_base;
#ifdef MD_PEER_WAKEUP
	void __iomem *md_peer_wakeup;
#endif
	void __iomem *md_bus_status;
	void __iomem *md_pc_monitor;
	void __iomem *md_topsm_status;
	void __iomem *md_ost_status;
	void __iomem *md_pll;

	unsigned int cldma_irq_id;
	unsigned int ap_ccif_irq_id;
	unsigned int md_wdt_irq_id;
	unsigned int ap2md_bus_timeout_irq_id;

	unsigned long cldma_irq_flags;
	unsigned long ap_ccif_irq_flags;
	unsigned long md_wdt_irq_flags;
	unsigned long ap2md_bus_timeout_irq_flags;

	struct md_hw_info *hw_info;
};

struct cldma_tgpd {
	u8	gpd_flags;
	u8	gpd_checksum;
	u16	debug_id;
	u32	next_gpd_ptr;
	u32	data_buff_bd_ptr;
	u16	data_buff_len;
	u8	desc_ext_len;
	u8	non_used;
} __attribute__ ((packed));

struct cldma_rgpd {
	u8	gpd_flags;
	u8	gpd_checksum;
	u16	data_allow_len;
	u32	next_gpd_ptr;
	u32	data_buff_bd_ptr;
	u16	data_buff_len;
	u16	debug_id;
} __attribute__ ((packed));

typedef enum {
	UNDER_BUDGET,
	REACH_BUDGET,
	PORT_REFUSE,
	NO_SKB,
	NO_REQ
} RX_COLLECT_RESULT;

enum{
	CCCI_TRACE_TX_IRQ = 0,
	CCCI_TRACE_RX_IRQ = 1,
};

static void inline md_cd_queue_struct_init(struct md_cd_queue *queue, struct ccci_modem *md,
	DIRECTION dir, unsigned char index, int bg)
{
	queue->dir = OUT;
	queue->index = index;
	queue->modem = md;
	queue->napi_port = NULL;
	
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
	init_waitqueue_head(&queue->req_wq);
	queue->budget = bg;
	spin_lock_init(&queue->ring_lock);
	queue->debug_id = 0;
	queue->busy_count = 0;
}

#endif //__MODEM_CD_H__
