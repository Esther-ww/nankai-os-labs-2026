/* 宿主机验证：直接编译交付的 Buddy 源码，复用实验头文件和链表。 */
#include "../kern/mm/buddy_pmm.c"

int printf(const char *, ...);
int vprintf(const char *, __builtin_va_list);
void exit(int) __attribute__((noreturn));

static struct Page storage[32768];
struct Page *pages = storage;
size_t npage;
const size_t nbase = 0x80000000ULL / PGSIZE;
uint64_t va_pa_offset;
static int expected_panic;
static unsigned long long saved_hash;

int cprintf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int result = vprintf(fmt, ap);
    __builtin_va_end(ap);
    return result;
}

static unsigned long long state_hash(void) {
    unsigned long long h = nr_free;
    size_t i;
    for (i = 0; i < BUDDY_MAX_PAGES; i++) {
        h = h * 131 + metadata[i].state;
        h = h * 131 + metadata[i].order;
        h = h * 131 + metadata[i].requested;
        h = h * 131 + storage[i].flags + storage[i].property;
        h = h * 131 + storage[i].ref;
        h = h * 131 + (uintptr_t)storage[i].page_link.next;
        h = h * 131 + (uintptr_t)storage[i].page_link.prev;
    }
    for (i = 0; i <= BUDDY_MAX_ORDER; i++) {
        h = h * 131 + (uintptr_t)free_lists[i].next;
        h = h * 131 + (uintptr_t)free_lists[i].prev;
    }
    return h;
}

void __panic(const char *file, int line, const char *fmt, ...) {
    (void)fmt;
    if (expected_panic && saved_hash == state_hash()) {
        printf("PASS: invalid operation rejected without mutation (%s:%d)\n", file, line);
        exit(77);
    }
    printf("FAIL: unexpected panic or state mutation at %s:%d\n", file, line);
    exit(1);
}

static void setup(size_t count) {
    size_t i;
    buddy_init();
    npage = nbase + count;
    for (i = 0; i < BUDDY_MAX_PAGES; i++) {
        storage[i].flags = 0;
        SetPageReserved(storage + i);
        storage[i].property = 0;
        storage[i].ref = 0;
        list_init(&storage[i].page_link);
    }
}

/* 独立遍历验证所有块覆盖、对齐、双向链表及总空闲页数。 */
static void invariant(void) {
    static unsigned char covered[32768];
    size_t i, j, sum = 0, limit = page_limit();
    unsigned order;
    for (i = 0; i < limit; i++) covered[i] = 0;
    for (order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = free_lists + order, *le = head->next;
        size_t seen = 0, count = (size_t)1 << order;
        while (le != head) {
            assert(++seen <= limit);
            assert(le->next->prev == le && le->prev->next == le);
            i = checked_index(le2page(le, page_link));
            assert(metadata[i].state == FREE_HEAD && metadata[i].order == order);
            assert(PageProperty(pages + i) && pages[i].property == count);
            assert(((nbase + i) % count) == 0 && count <= limit - i);
            for (j = i; j < i + count; j++) {
                assert(!covered[j] && !PageReserved(pages + j));
                if (j != i) assert(metadata[j].state == INTERIOR);
                covered[j] = 1;
            }
            sum += count;
            le = le->next;
        }
    }
    assert(sum == nr_free);
    for (i = 0; i < limit; i++) {
        if (metadata[i].state == ALLOC_HEAD) {
            size_t count = (size_t)1 << metadata[i].order;
            assert(count <= limit-i && ((nbase+i)%count)==0);
            assert(!PageProperty(pages+i));
            for (j = i; j < i+count; j++) {
                assert(!covered[j] && !PageReserved(pages+j));
                if (j != i) assert(metadata[j].state == INTERIOR);
                covered[j] = 1;
            }
        }
    }
    for (i = 0; i < limit; i++) {
        assert(covered[i] == (metadata[i].state != UNREGISTERED));
    }
}

int main(int argc, char **argv) {
    size_t i;
    if (argc > 1) {
        setup(64);
        buddy_init_memmap(pages, 32);
        struct Page *p = buddy_alloc_pages(3);
        char mode = argv[1][0];
        if (mode == 'd') buddy_free_pages(p, 3);
        saved_hash = state_hash(); expected_panic = 1;
        switch (mode) {
        case 'n': buddy_free_pages(NULL, 3); break;
        case 'z': buddy_free_pages(p, 0); break;
        case 'w': buddy_free_pages(p, 4); break;
        case 'i': buddy_free_pages(p+1, 3); break;
        case 'd': buddy_free_pages(p, 3); break;
        case 'r': buddy_init_memmap(pages, 32); break;
        case 'o': buddy_init_memmap(pages+63, 2); break;
        case 'u': buddy_free_pages(pages+50, 1); break;
        case 'a': buddy_free_pages((struct Page *)((uintptr_t)p+1),3); break;
        case 'x': buddy_free_pages(pages+64,1); break;
        default: exit(2);
        }
        printf("FAIL: invalid operation accepted\n");return 1;
    }
    setup(32768);buddy_init_memmap(pages,32768);invariant();
    struct Page *whole=buddy_alloc_pages(32768);
    assert(whole==pages && nr_free==0 && buddy_alloc_pages(1)==NULL);
    buddy_free_pages(whole,32768);invariant();buddy_check();invariant();
    printf("PASS: maximum block, recursive split/merge, basic checks\n");

    setup(64);buddy_init_memmap(pages+1,13);buddy_init_memmap(pages+20,7);
    invariant();assert(nr_free==20);
    struct Page *leaves[64];size_t used=0;
    while ((leaves[used]=buddy_alloc_pages(1))!=NULL) { used++;invariant(); }
    assert(used==20 && nr_free==0);
    while (used) buddy_free_pages(leaves[--used],1);
    invariant();assert(nr_free==20);
    printf("PASS: unaligned ranges, gaps, exhaustion and recovery\n");

    setup(16);buddy_init_memmap(pages+8,8);buddy_init_memmap(pages,8);
    whole=buddy_alloc_pages(16);assert(whole==pages);buddy_free_pages(whole,16);
    invariant();printf("PASS: adjacent registrations coalesce\n");

    setup(4);buddy_init_memmap(pages,4);
    for(i=0;i<4;i++)leaves[i]=buddy_alloc_pages(1);
    struct Page *at[4];for(i=0;i<4;i++)at[leaves[i]-pages]=leaves[i];
    buddy_free_pages(at[1],1);buddy_free_pages(at[2],1);
    assert(nr_free==2 && buddy_alloc_pages(2)==NULL);
    invariant();buddy_free_pages(at[0],1);buddy_free_pages(at[3],1);
    whole=buddy_alloc_pages(4);assert(whole==pages);buddy_free_pages(whole,4);
    invariant();printf("PASS: adjacent non-buddies do not merge\n");

    setup(1024);buddy_init_memmap(pages+3,997);
    struct Page *active[128]={0};size_t requests[128]={0};unsigned rng=1234567;
    for(i=0;i<10000;i++) {
        rng=rng*1664525U+1013904223U;unsigned slot=(rng>>16)%128;
        if(active[slot]) {buddy_free_pages(active[slot],requests[slot]);active[slot]=NULL;}
        else {rng=rng*1664525U+1013904223U;size_t n=(rng>>16)%65+1;
            size_t before=nr_free;active[slot]=buddy_alloc_pages(n);requests[slot]=n;
            if(!active[slot])assert(before==nr_free);}
        invariant();
    }
    for(i=0;i<128;i++)if(active[i])buddy_free_pages(active[i],requests[i]);
    invariant();assert(nr_free==997);
    printf("PASS: 10000 deterministic randomized operations\n");
    setup(8);buddy_init_memmap(NULL,0);buddy_check();invariant();
    printf("ALL HOST POSITIVE TESTS PASSED\n");return 0;
}
