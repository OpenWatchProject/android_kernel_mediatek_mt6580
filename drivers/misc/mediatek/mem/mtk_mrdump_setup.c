#include <linux/kconfig.h>
#include <linux/memblock.h>
#include <linux/mrdump.h>
#include <linux/reboot.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <linux/io.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>

#if defined(CONFIG_OF_FLATTREE)

reservedmem_of_init_fn free_reserve_memory_mrdump_fn(struct reserved_mem *rmem,
						unsigned long node,
						const char *uname)
{
	pr_alert("%s, name: %s, uname: %s, base: 0x%llx, size: 0x%llx\n",
		 __func__, rmem->name, uname,
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->size);
	if (memblock_is_region_reserved(rmem->base, rmem->size)) {
		pr_alert
	    ("mrdump disabled , return reserve memory base = %x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);

		if (memblock_free(rmem->base, rmem->size)) {
			pr_alert
			("memblock_free failed on memory base 0x%x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);
		}

	} else {
		pr_alert
		("mrdump disabled and DTS reserved failed on memory base = %x size=%x\n",
			(unsigned int)rmem->base, (unsigned int)rmem->size);
	}
    return 0;
}

RESERVEDMEM_OF_DECLARE(mrdump_reserved_memory, "mrdump-reserved-memory",
		       free_reserve_memory_mrdump_fn);
RESERVEDMEM_OF_DECLARE(mrdump_preloader_reserved_memory,
		       "preloader-reserved-memory", free_reserve_memory_mrdump_fn);
RESERVEDMEM_OF_DECLARE(mrdump_lk_reserved_memory, "lk-reserved-memory",
		       free_reserve_memory_mrdump_fn);
#endif
