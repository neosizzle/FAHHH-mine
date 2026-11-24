#include "woody.h"

static inline long call(long number, long a, long b, long c,
                        long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long result;

    __asm__ volatile ("syscall" : "=a" (result)
        : "a" (number), "D" (a), "S" (b), "d" (c),
          "r" (r10), "r" (r8), "r" (r9) : "rcx", "r11", "memory", "cc");
    return result;
}

static void fail(void)
{
    static const char message[] = "woody: cannot unpack executable\n";

    call(1, 2, (long)message, sizeof(message) - 1, 0, 0, 0);
    call(231, 126, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}

uintptr_t unpack(const t_info *info)
{
    uintptr_t bias = (uintptr_t)info - info->self;
    unsigned char *target = (unsigned char *)(bias + info->target);
    unsigned char *temporary;
    uintptr_t page = (uintptr_t)target & ~(PAGE_SIZE - 1);
    size_t length;
    size_t i;
    long result;

    if (info->magic != WOODY_MAGIC || !info->raw_size
        || info->raw_size > 0x7fffffffUL || !info->packed_size
        || info->packed_size >= info->raw_size)
        fail();
    length = (((uintptr_t)target + info->raw_size + PAGE_SIZE - 1)
              & ~(PAGE_SIZE - 1)) - page;
    /* mmap temporary writable memory, with no execute permission. */
    result = call(9, 0, info->raw_size, 3, 0x22, -1, 0);
    if ((unsigned long)result >= (unsigned long)-4095)
        fail();
    temporary = (unsigned char *)result;
    if (lz_decode_masked(target, info->packed_size, temporary, info->raw_size,
                         info->mask)
        || checksum(temporary, info->raw_size) != info->checksum)
        fail();
    /* The unpacker runs in a different segment, so code can be made RW here. */
    if (call(10, page, length, 3, 0, 0, 0) < 0)
        fail();
    for (i = 0; i < info->raw_size; ++i)
        target[i] = temporary[i];
    if (call(10, page, length, info->prot, 0, 0, 0) < 0)
        fail();
    call(11, (long)temporary, info->raw_size, 0, 0, 0, 0);
    return bias + info->entry;
}
