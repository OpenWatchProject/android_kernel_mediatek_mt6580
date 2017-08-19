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
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/it7250/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y	+=  IT7260_driver.o

