include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_FPGA)		:= mt_gpio_fpga.o
obj-$(CONFIG_MTK_EVB_BOARD)	:= mt_gpio_base.o 

obj-$(CONFIG_MTK_GPIO)	+= mt_gpio_ext.o mt_gpio_affix.o mt_gpio_debug.o

obj-$(CONFIG_GPIOLIB)	+= mt_gpio_base_linux.o

obj-$(CONFIG_MTK_MT6306_SUPPORT)	+= dcl_sim_gpio.o
