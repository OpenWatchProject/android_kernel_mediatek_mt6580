include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sw_tx_power/$(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-$(CONFIG_MTK_SWITCH_TX_POWER) += sw_tx_power_eint.o
obj-$(CONFIG_MTK_SWITCH_TX_POWER) += sw_tx_power_sysfs.o

obj-$(CONFIG_MTK_SWITCH_TX_POWER) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
