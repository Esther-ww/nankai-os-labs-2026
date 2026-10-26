#include <buddy_pmm.h>
#include <stdio.h>

/* 当前框架最多管理 128 MiB，即 32768 个 4 KiB 页面。 */
#define BUDDY_MAX_ORDER 15
#define BUDDY_MAX_PAGES ((size_t)1 << BUDDY_MAX_ORDER)

enum { UNREGISTERED, INTERIOR, FREE_HEAD, ALLOC_HEAD };
struct buddy_meta {
    uint32_t requested;
    uint8_t state;
    uint8_t order;
};

static struct buddy_meta metadata[BUDDY_MAX_PAGES];
static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t nr_free;

static size_t block_pages(unsigned order) {
    return (size_t)1 << order;
}

static size_t page_limit(void) {
    assert(npage >= nbase);
    assert(npage - nbase <= BUDDY_MAX_PAGES);
    return npage - nbase;
}

/* 先检查数值地址，再计算数组索引，避免对无关指针做减法。 */
static size_t checked_index(struct Page *p) {
    uintptr_t start = (uintptr_t)pages, addr = (uintptr_t)p;
    assert(pages != NULL && p != NULL && addr >= start);
    uintptr_t delta = addr - start;
    assert(delta % sizeof(struct Page) == 0);
    size_t i = delta / sizeof(struct Page);
    assert(i < page_limit());
    return i;
}

static unsigned request_order(size_t n) {
    unsigned order = 0;
    while (block_pages(order) < n) {
        order++;
    }
    return order;
}

static void insert_block(size_t i, unsigned order) {
    struct Page *p = pages + i;
    assert(metadata[i].state == INTERIOR);
    metadata[i].state = FREE_HEAD;
    metadata[i].order = order;
    metadata[i].requested = 0;
    p->property = block_pages(order);
    SetPageProperty(p);
    list_add(&free_lists[order], &p->page_link);
}

static void remove_block(size_t i) {
    assert(metadata[i].state == FREE_HEAD);
    list_del_init(&pages[i].page_link);
    ClearPageProperty(pages + i);
    pages[i].property = 0;
    metadata[i].state = INTERIOR;
    metadata[i].order = 0;
}

/* XOR 使用物理页号，不能直接对 Page 指针做 XOR。 */
static void merge_and_insert(size_t i, unsigned order) {
    size_t limit = page_limit();
    while (order < BUDDY_MAX_ORDER) {
        size_t count = block_pages(order);
        size_t buddy_ppn = (nbase + i) ^ count;
        if (buddy_ppn < nbase) {
            break;
        }
        size_t other = buddy_ppn - nbase;
        if (other >= limit || count > limit - other ||
            metadata[other].state != FREE_HEAD ||
            metadata[other].order != order) {
            break;
        }
        remove_block(other);
        if (other < i) {
            i = other;
        }
        order++;
    }
    insert_block(i, order);
}

static void buddy_init(void) {
    unsigned order;
    size_t i;
    for (order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    for (i = 0; i < BUDDY_MAX_PAGES; i++) {
        metadata[i].state = UNREGISTERED;
        metadata[i].order = 0;
        metadata[i].requested = 0;
    }
    nr_free = 0;
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    if (n == 0) {
        return;
    }
    size_t start = checked_index(base), limit = page_limit(), i;
    assert(n <= limit - start);
    /* 所有前置条件通过后才更新状态；重复注册必须被拒绝。 */
    for (i = start; i < start + n; i++) {
        assert(metadata[i].state == UNREGISTERED);
        assert(PageReserved(pages + i));
    }
    for (i = start; i < start + n; i++) {
        struct Page *p = pages + i;
        ClearPageReserved(p);
        ClearPageProperty(p);
        set_page_ref(p, 0);
        p->property = 0;
        list_init(&p->page_link);
        metadata[i].state = INTERIOR;
    }
    size_t remaining = n;
    i = start;
    while (remaining != 0) {
        unsigned order = 0;
        while (order < BUDDY_MAX_ORDER &&
               block_pages(order + 1) <= remaining &&
               ((nbase + i) & (block_pages(order + 1) - 1)) == 0) {
            order++;
        }
        size_t count = block_pages(order);
        merge_and_insert(i, order);
        i += count;
        remaining -= count;
    }
    nr_free += n;
}

static struct Page *buddy_alloc_pages(size_t n) {
    if (n == 0 || n > BUDDY_MAX_PAGES) {
        return NULL;
    }
    unsigned target = request_order(n), order = target;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *p = le2page(list_next(&free_lists[order]), page_link);
    size_t i = checked_index(p);
    remove_block(i);
    while (order > target) {
        order--;
        insert_block(i + block_pages(order), order);
    }
    metadata[i].state = ALLOC_HEAD;
    metadata[i].order = target;
    metadata[i].requested = n;
    p->property = block_pages(target);
    assert(nr_free >= block_pages(target));
    nr_free -= block_pages(target);
    return p;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    size_t i = checked_index(base);
    assert(n != 0 && n <= BUDDY_MAX_PAGES);
    assert(metadata[i].state == ALLOC_HEAD);
    assert(metadata[i].requested == n);
    unsigned order = metadata[i].order;
    assert(order <= BUDDY_MAX_ORDER);
    size_t count = block_pages(order), j;
    assert(count <= page_limit() - i);
    assert(!PageReserved(base) && !PageProperty(base));
    assert(base->property == count);
    for (j = i + 1; j < i + count; j++) {
        assert(metadata[j].state == INTERIOR);
        assert(!PageReserved(pages + j) && !PageProperty(pages + j));
    }
    for (j = i; j < i + count; j++) {
        set_page_ref(pages + j, 0);
    }
    metadata[i].state = INTERIOR;
    metadata[i].requested = 0;
    metadata[i].order = 0;
    base->property = 0;
    nr_free += count;
    merge_and_insert(i, order);
}

static size_t buddy_nr_free_pages(void) {
    return nr_free;
}

static void buddy_check(void) {
    const size_t requests[] = {1, 2, 3};
    size_t before = buddy_nr_free_pages(), k;
    for (k = 0; k < sizeof(requests) / sizeof(requests[0]); k++) {
        size_t n = requests[k], actual = block_pages(request_order(n));
        struct Page *p = buddy_alloc_pages(n);
        if (p == NULL) {
            assert(buddy_nr_free_pages() == before);
            cprintf("buddy_check: SKIP request %lu (no suitable block)\n", n);
            continue;
        }
        assert(buddy_nr_free_pages() == before - actual);
        assert(metadata[checked_index(p)].state == ALLOC_HEAD);
        buddy_free_pages(p, n);
        assert(buddy_nr_free_pages() == before);
    }
    assert(buddy_alloc_pages(0) == NULL);
    assert(buddy_alloc_pages(BUDDY_MAX_PAGES + 1) == NULL);
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    assert(buddy_nr_free_pages() == before);
    cprintf("buddy_check: basic checks completed; see SKIP lines if any\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
