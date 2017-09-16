#include <linux/leds.h>
/* #include <leds_sw.h> */
#include <cust_leds.h>
#include <cust_leds_def.h>

/****************************************************************************
 * LED DRV functions
 ***************************************************************************/
extern int mt65xx_leds_brightness_set(enum mt65xx_led_type type, enum led_brightness level);
#ifdef CONFIG_MTK_LEDS
extern int backlight_brightness_set(int level);
#else
#define backlight_brightness_set(level) do { } while (0)
#endif
