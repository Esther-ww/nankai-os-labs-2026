#include <default_pmm.h>
#include <best_fit_pmm.h>
#ifdef USE_BUDDY
#include <buddy_pmm.h>
#endif
#if defined(USE_BUDDY) && defined(USE_FIRST_FIT)
#error "Select only one physical memory manager"
#endif
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#ifdef USE_BUDDY
    pmm_manager = &buddy_pmm_manager;
#elif defined(USE_FIRST_FIT)
    pmm_manager = &default_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2413685


/* lab2 function1 2413685
 * Initialize physical page descriptors and free memory.
 */
static void page_init(void) {
    extern char end[];

    // 1. 从设备树获取物理内存信息
    uintptr_t mem_base = get_memory_base();
    uintptr_t mem_size = get_memory_size();

    if (mem_size == 0 || mem_base != DRAM_BASE ||
        mem_size > (uintptr_t)-1 - mem_base) {
        panic("page_init: invalid physical memory");
    }

    uintptr_t mem_end = mem_base + mem_size;

    // 2. 限制在当前内核能够管理的物理地址范围内
    if (mem_end > PHYSICAL_MEMORY_END) {
        mem_end = PHYSICAL_MEMORY_END;
    }

    // 物理页必须完整，因此末端向下按页对齐
    mem_end &= ~((uintptr_t)PGSIZE - 1);

    if (mem_end <= KERNEL_BEGIN_PADDR) {
        panic("page_init: insufficient physical memory");
    }

    // 3. 初始化虚拟地址和物理地址的映射偏移
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    // npage 是物理页号上界，不是 pages 数组长度
    npage = PPN(mem_end);

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            mem_size, mem_base, mem_base + mem_size - 1);

    // 4. 在内核镜像结束后放置 Page 描述符数组
    uintptr_t pages_start =
        ((uintptr_t)end + PGSIZE - 1) &
        ~((uintptr_t)PGSIZE - 1);

    pages = (struct Page *)pages_start;

    size_t page_count = npage - nbase;

    // 检查描述符数组所需空间
    if (page_count > ((size_t)-1) / sizeof(struct Page)) {
        panic("page_init: page descriptor size overflow");
    }

    size_t pages_bytes = page_count * sizeof(struct Page);

    if (pages_start > (uintptr_t)-1 - pages_bytes) {
        panic("page_init: descriptor address overflow");
    }

    uintptr_t pages_end = pages_start + pages_bytes;

    if (PADDR(pages_start) >= mem_end ||
        PADDR(pages_end) > mem_end) {
        panic("page_init: insufficient space for descriptors");
    }

    // 5. 初始化全部物理页描述符为保留状态
    for (size_t i = 0; i < page_count; i++) {
        struct Page *page = &pages[i];

        page->flags = 0;
        page->property = 0;
        set_page_ref(page, 0);
        SetPageReserved(page);
        list_init(&(page->page_link));
    }

    // 6. 描述符数组后方才是可以分配的物理内存
    uintptr_t free_start = PADDR(pages_end);

    // 空闲区起点向上按页对齐
    free_start = (free_start + PGSIZE - 1) &
                 ~((uintptr_t)PGSIZE - 1);

    uintptr_t free_end = mem_end;

    // 7. 只注册完整的空闲物理页
    if (free_start < free_end) {
        size_t free_count =
            (free_end - free_start) / PGSIZE;

        init_memmap(pa2page(free_start), free_count);
    }

    cprintf("page_init: initialization completed\n");
    cprintf("page_init: free pages = %lu\n", nr_free_pages());
}



/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
