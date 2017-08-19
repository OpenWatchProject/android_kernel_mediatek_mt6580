include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/inc

ifeq ($(CONFIG_CUSTOM_KERNEL_HDMI),"it668x")
  ccflags-y +=	-DMHL_BRIDGE_SUPPORT/
endif

obj-y	+= it668x.o
