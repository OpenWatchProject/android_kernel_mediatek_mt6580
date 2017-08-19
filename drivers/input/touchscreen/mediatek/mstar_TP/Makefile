include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/mstar_TP/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y	+=  mstar_drv_common.o mstar_drv_fw_control.o mstar_drv_hotknot.o mstar_drv_hotknot_queue.o mstar_drv_ic_fw_porting_layer.o mstar_drv_jni_interface.o \
          mstar_drv_main.o mstar_drv_mp_test.o mstar_drv_mtk.o mstar_drv_platform_interface.o mstar_drv_platform_porting_layer.o mstar_drv_utility_adaption.o