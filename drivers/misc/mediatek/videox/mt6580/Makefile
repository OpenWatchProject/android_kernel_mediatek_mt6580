# drivers/video/mtk/Makefile
#

include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_FB) += mtkfb_console.o        \
						disp_assert_layer.o    \
						mtkfb.o                \
						mtkfb_vsync.o          \
						debug.o                \
						primary_display.o      \
						disp_lcm.o             \
						disp_utils.o           \
						mtkfb_fence.o          \
						mtk_disp_mgr.o         \
						mtk_mira.o             \
						mtk_ovl.o              \
						disp_helper.o          \
						fbconfig_kdebug_x.o

ifneq ($(CONFIG_MTK_FB), y)
obj-y += mtkfb_dummy.o
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/     \
             -I$(srctree)/drivers/misc/mediatek/sync/      \
						 -I$(srctree)/drivers/misc/mediatek/videox/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/dispsys/$(MTK_PLATFORM)   \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)       \
             -I$(srctree)/drivers/misc/mediatek/cmdq/$(MTK_PLATFORM)      \
             -I$(srctree)/drivers/misc/mediatek/externaldisplay/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach/ \
             -I$(srctree)/drivers/staging/android/ion

ccflags-y += -I$(srctree)/drivers/staging/android/ion

ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mhltx/
endif
ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(CONFIG_MTK_GPU_SUPPORT), y)
ccflags-y += -DHWGPU_SUPPORT
endif
