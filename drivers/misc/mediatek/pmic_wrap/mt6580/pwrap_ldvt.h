#ifndef _PWRAP_LDVT_H_
#define _PWRAP_LDVT_H_

/* FIXME: use _IO(magic, num) instead */
#define WRAP_UVVF_INIT                  0x0600
#define WRAP_UVVF_WACS_TEST             0x0601
#define WRAP_UVVF_STATUS_UPDATE         0x0602
#define WRAP_UVVF_EVENT_TEST            0x0603
#define WRAP_UVVF_DUAL_IO               0x0604
#define WRAP_UVVF_REG_RW                0x0605
#define WRAP_UVVF_MUX_SWITCH            0x0606
#define WRAP_UVVF_RESET_PATTERN         0x0607
#define WRAP_UVVF_SOFT_RESET            0x0608
#define WRAP_UVVF_HIGH_PRI              0x0609
#define WRAP_UVVF_IN_ORDER_PRI          0x0610
#define WRAP_UVVF_SPI_ENCRYPTION_TEST   0x0612
#define WRAP_UVVF_WDT_TEST              0x0613
#define WRAP_UVVF_INT_TEST              0x0614
#define WRAP_UVVF_PERI_WDT_TEST         0x0615
#define WRAP_UVVF_PERI_INT_TEST         0x0616
#define WRAP_UVVF_CONCURRENCE_TEST      0x0617
#define WRAP_UVVF_CLOCK_GATING          0x0618
#define WRAP_UVVF_THROUGHPUT            0x0619
#define WRAP_UVVF_EINT_NORMAL_TEST      0x0620
#define WRAP_UVVF_STATUS_UPDATE_SCHEME  0x0621
/* define macro and inline function (for do while loop) */
typedef unsigned int (*loop_condition_fp)(unsigned int);

static inline U32 wait_for_fsm_idle(U32 x)
{
	return (GET_WACS0_FSM( x ) != WACS_FSM_IDLE );
}
static inline U32 wait_for_fsm_vldclr(U32 x)
{
	return (GET_WACS0_FSM( x ) != WACS_FSM_WFVLDCLR);
}
static inline U32 wait_for_sync(U32 x)
{
	return (GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}
static inline U32 wait_for_idle_and_sync(U32 x)
{
	return ((GET_WACS2_FSM(x) != WACS_FSM_IDLE) || (GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE)) ;
}
static inline U32 wait_for_wrap_idle(U32 x)
{
	return ((GET_WRAP_FSM(x) != 0x0) || (GET_WRAP_CH_DLE_RESTCNT(x) != 0x0));
}
static inline U32 wait_for_wrap_state_idle(U32 x)
{
	return ( GET_WRAP_AG_DLE_RESTCNT( x ) != 0 ) ;
}
static inline U32 wait_for_man_idle_and_noreq(U32 x)
{
	return ( (GET_MAN_REQ(x) != MAN_FSM_NO_REQ ) || (GET_MAN_FSM(x) != MAN_FSM_IDLE) );
}
static inline U32 wait_for_man_vldclr(U32 x)
{
	return  (GET_MAN_FSM( x ) != MAN_FSM_WFVLDCLR) ;
}
static inline U32 wait_for_cipher_ready(U32 x)
{
	return (x!=3) ;
}
static inline U32 wait_for_stdupd_idle(U32 x)
{
	return ( GET_STAUPD_FSM(x) != 0x0) ;
}

#endif /* _PWRAP_LDVT_H_ */
