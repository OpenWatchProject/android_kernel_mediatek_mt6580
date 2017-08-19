#
# Makefile for misc devices that really don't fit anywhere else.
#
include $(srctree)/drivers/misc/mediatek/Makefile.custom

define FILTER_PROJECT_DRV
ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/imgsensor/$(1)),)
platform_drv += $(1)
endif
endef

obj-y				+= kd_sensorlist.o

$(warning "xilog ARCH_MTK_PROJECT:$(ARCH_MTK_PROJECT)")
# for tb6580p1 special
ifeq ($(ARCH_MTK_PROJECT),tb6580p1)
  obj-y += ov5670_mipi_raw_32bit/
  obj-y += ov5671_mipi_raw/
else
  # for tb6580p2 special
  ifeq ($(ARCH_MTK_PROJECT),tb6580p2)
    obj-y += ov5670_mipi_raw_32bit/
    obj-y += ov5671_mipi_raw/
  else
    $(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_PROJECT_DRV,$(drv))))
    custom_kernel_imgsensor := $(addsuffix /, $(shell echo $(platform_drv)))
    obj-y += $(custom_kernel_imgsensor)
    $(info imgsensor drv by platform $(platform_drv))
  endif
endif
