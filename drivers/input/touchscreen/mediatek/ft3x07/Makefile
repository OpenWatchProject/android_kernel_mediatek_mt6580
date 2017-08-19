include $(srctree)/drivers/misc/mediatek/Makefile.custom


ifeq ($(CONFIG_MTK_TOUCHPANEL_PHYSICAL_ROTATION), "0")
ccflags-y  += -DMTK_LCM_PHYSICAL_ROTATION=0
endif

ifeq ($(CONFIG_MTK_TOUCHPANEL_PHYSICAL_ROTATION), "90")
ccflags-y += -DMTK_LCM_PHYSICAL_ROTATION=90
endif

ifeq ($(CONFIG_MTK_TOUCHPANEL_PHYSICAL_ROTATION), "270")
ccflags-y += -DMTK_LCM_PHYSICAL_ROTATION=270
endif

ifeq ($(CONFIG_MTK_TOUCHPANEL_PHYSICAL_ROTATION), "180")
ccflags-y += -DMTK_LCM_PHYSICAL_ROTATION=180
endif


# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ft3x07/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y	+=  focaltech_driver.o focaltech_ex_fun.o focaltech_ctl.o
obj-y += ft_gesture_lib.a

