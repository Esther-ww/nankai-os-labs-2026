/* Host unit tests: real trapframe layout and the verbatim delivered handler. */
#include <trap.h>
#include <stdarg.h>
extern int printf(const char *, ...);
extern int memcmp(const void *, const void *, size_t);

static int dump_count, panic_count, output_count, failures;
static const char *last_format;
static void *last_address;
static struct trapframe *last_dump;

int cprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    last_address = va_arg(ap, void *);
    va_end(ap);
    last_format = fmt;
    output_count++;
    return 0;
}
void print_trapframe(struct trapframe *tf) {
    dump_count++;
    last_dump = tf;
}
static void test_panic(const char *message) {
    (void)message;
    panic_count++;
}
/* Real kernel panic stops execution; this stub records it for assertions. */
#define panic test_panic
#include "handler_under_test.inc"

static int same_text(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void run_case(const char *name, uintptr_t cause, uintptr_t pc,
                     unsigned advance, int expect_dump, int expect_panic,
                     const char *format) {
    struct trapframe tf, expected;
    unsigned char *bytes = (unsigned char *)&tf;
    unsigned i;
    for (i = 0; i < sizeof(tf); i++) bytes[i] = (unsigned char)(i * 17 + 3);
    tf.cause = cause;
    tf.epc = pc;
    expected = tf;
    expected.epc += advance;
    dump_count = panic_count = output_count = 0;
    last_format = 0;
    last_address = 0;
    last_dump = 0;
    exception_handler(&tf);
    int ok = memcmp(&tf, &expected, sizeof(tf)) == 0 &&
             dump_count == expect_dump && panic_count == expect_panic &&
             output_count == (format != 0);
    if (format) ok = ok && same_text(format, last_format) && last_address == (void *)pc;
    if (expect_dump) ok = ok && last_dump == &tf;
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

int main(void) {
    /* 32-bit EBREAK deliberately starts at a 2-byte, not 4-byte offset. */
    uint16_t compressed[] = {0x9002, 0};
    uint16_t regular[] __attribute__((aligned(4))) = {0, 0x0073, 0x0010, 0};
    uint16_t illegal16[] = {0x0000, 0};
    uint16_t illegal32[] = {0x007b, 0x0000, 0};
    uint16_t long48[] = {0x001f, 0, 0};
    uint16_t long64[] = {0x003f, 0, 0, 0};
    const char *bp = "Breakpoint at %p\n";
    const char *ill = "Illegal instruction at %p\n";
    if (sizeof(void *) != 8 || sizeof(struct trapframe) != 36 * 8 ||
        ((uintptr_t)&regular[1] & 3) != 2) return 2;
    run_case("C.EBREAK advances EPC by 2", 3, (uintptr_t)compressed, 2, 0, 0, bp);
    run_case("EBREAK advances EPC by 4 at halfword alignment", 3, (uintptr_t)&regular[1], 4, 0, 0, bp);
    run_case("16-bit illegal encoding advances EPC by 2", 2, (uintptr_t)illegal16, 2, 0, 0, ill);
    run_case("teacher 0x0000007b advances EPC by 4", 2, (uintptr_t)illegal32, 4, 0, 0, ill);
    run_case("teacher fallback cause 20 with unreadable EPC", 20, 0, 0, 1, 0, 0);
    run_case("instruction page fault is not silently skipped", 12, 0, 0, 1, 0, 0);
    run_case("load page fault is not silently skipped", 13, 1, 0, 1, 0, 0);
    run_case("store page fault is not silently skipped", 15, 0, 0, 1, 0, 0);
    run_case("48-bit encoding stops without changing EPC", 2, (uintptr_t)long48, 0, 0, 1, ill);
    run_case("64-bit encoding stops without changing EPC", 2, (uintptr_t)long64, 0, 0, 1, ill);
    run_case("NULL known-exception EPC rejected", 2, 0, 0, 0, 1, ill);
    run_case("odd known-exception EPC rejected", 3, 1, 0, 0, 1, bp);
    printf("%s\n", failures ? "HOST TESTS FAILED" : "ALL 12 HOST TESTS PASSED (not a QEMU/grade result)");
    return failures != 0;
}
