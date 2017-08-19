#ifndef LED_CTRL_H__
#define LED_CTRL_H__
#include <linux/types.h>
/* LED control settings */
#define LED_INC_DEC_STEP				2
#define LED_CTRL_EXPO_TIME_HI_BOUND		496	//480  (20140328)
#define LED_CTRL_EXPO_TIME_LOW_BOUND	32	// 32   (20140328)
#define LED_CTRL_EXPO_TIME_HI			420	// 320 (20140328)
#define LED_CTRL_EXPO_TIME_LOW			64	// 96   (20140328)
#define LED_CURRENT_HI				31 	// 25   (20140328)
#define LED_CURRENT_LOW				1  	// 2     (20140328)
#define STATE_COUNT_TH				3
#define DEFAULT_LED_STEP 10

void led_ctrl(uint8_t touch);
uint8_t get_led_current_change_flag(void);

#endif /* LED_CTRL_H__ */