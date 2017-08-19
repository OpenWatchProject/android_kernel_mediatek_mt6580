#include <linux/kconfig.h>
#include <linux/memblock.h>
#include <linux/mrdump.h>
#include <linux/reboot.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/io.h>

#ifdef CONFIG_HAVE_DDR_RESERVE_MODE
#include <mach/wd_api.h>
#endif

/* change to DTS
    #define MRDUMP_CB_ADDR (PHYS_OFFSET + 0x1F00000)
    #define MRDUMP_CB_SIZE 0x2000
*/
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
static phys_addr_t mrdump_base = 0;
static phys_addr_t mrdump_size = 0;
uint64_t aee_get_mrdump_base(void);
uint64_t aee_get_mrdump_size(void);

#define LK_LOAD_ADDR (PHYS_OFFSET + 0x1E00000)
#define LK_LOAD_SIZE 0x100000

static void mrdump_hw_enable(bool enabled)
{
#ifdef CONFIG_HAVE_DDR_RESERVE_MODE
    struct wd_api *wd_api = NULL;
    get_wd_api(&wd_api);
    if(wd_api)
        wd_api->wd_dram_reserved_mode(enabled);
#endif
}

static void mrdump_reboot(void)
{
	emergency_restart();
}

const struct mrdump_platform mrdump_v2_platform = {
	.hw_enable = mrdump_hw_enable,
	.reboot = mrdump_reboot
};

void mrdump_reserve_memory(void)
{

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6752) || defined(CONFIG_ARCH_MT6795)
#define PRELOADER_ADDR (PHYS_OFFSET + 0x2000000)
#define PRELOADER_SIZE 0x200000

	memblock_reserve(PRELOADER_ADDR, PRELOADER_SIZE);
#endif

	/* We must reserved the lk block, can we pass it from lk? */
	memblock_reserve(LK_LOAD_ADDR, LK_LOAD_SIZE);

}

reservedmem_of_init_fn reserve_memory_mrdump_fn(struct reserved_mem *rmem,
						unsigned long node,
						const char *uname)
{
	struct mrdump_control_block *cblock = NULL;
	pr_alert("%s, name: %s, uname: %s, base: 0x%llx, size: 0x%llx\n",
		 __func__, rmem->name, uname,
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->size);
	if (!memblock_is_region_reserved((rmem->base + rmem->size) - PAGE_SIZE, PAGE_SIZE)) {
		pr_alert
		    ("!ALERT mrdump reserve failed on  base = %x size=%x last page is not in memblock.reserved \n",
		     (unsigned int)rmem->base, (unsigned int)rmem->size);
		return 0;
	}

	if (strncmp
	    (uname, "mrdump-reserved-memory",
	     strlen("mrdump-reserved-memory")) == 0) {
		mrdump_base = rmem->base;
		mrdump_size = rmem->size;
		cblock = (struct mrdump_control_block *)__va(mrdump_base);
		mrdump_platform_init(cblock, &mrdump_v2_platform);
	}
	if (strncmp
	    (uname, "preloader-reserved-memory",
	     strlen("preloader-reserved-memory")) == 0) {

	}

	if (strncmp(uname, "lk-reserved-memory", strlen("lk-reserved-memory"))
	    == 0) {

	}

	return 0;
}

uint64_t aee_get_mrdump_base(void)
{
	return (uint64_t) mrdump_base;
}

uint64_t aee_get_mrdump_size(void)
{
	return (uint64_t) mrdump_size;
}

RESERVEDMEM_OF_DECLARE(mrdump_reserved_memory, "mrdump-reserved-memory",
		       reserve_memory_mrdump_fn);
RESERVEDMEM_OF_DECLARE(mrdump_preloader_reserved_memory,
		       "preloader-reserved-memory", reserve_memory_mrdump_fn);
RESERVEDMEM_OF_DECLARE(mrdump_lk_reserved_memory, "lk-reserved-memory",
		       reserve_memory_mrdump_fn);
