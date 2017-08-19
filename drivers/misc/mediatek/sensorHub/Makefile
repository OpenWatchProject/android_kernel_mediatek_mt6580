include $(srctree)/drivers/misc/mediatek/Makefile.custom

# In case the platform does NOT support this type of sensors

### Workaround ###
ifneq ($(CONFIG_ARCH_MT2601),y)
obj-y += SCP_sensorHub/
obj-y += SCP_shf/
else
obj-$(CONFIG_INPUT_CWSTM32)	+= CwMcuSensor/
obj-$(CONFIG_MTK_ICM30628)	+= icm30628/
endif
ifeq ($(CONFIG_CUSTOM_KERNEL_SENSORHUB),"OTHER_VENDOR")
obj-y += other_vendor/
endif
