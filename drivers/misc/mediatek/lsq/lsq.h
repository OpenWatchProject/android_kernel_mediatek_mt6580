#ifndef _LSQ_MISC_CONTROL_SECTION_
#define _LSQ_MISC_CONTROL_SECTION_

#include <mach/mt_typedefs.h>
enum lsq_idle_clock_enum
{
	LSQ_IDLE_CLOCK_OFF = 0,
	LSQ_IDLE_CLOCK_ON,
};



static int debug_enable_lsq = 1;
#define LSQ_DRV_DEBUG(format, args...) do { \
	if (debug_enable_lsq) \
	{\
		printk(KERN_WARNING format, ##args);\
	} \
} while (0)


#endif

