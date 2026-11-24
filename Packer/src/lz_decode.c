#include "woody.h"

static unsigned int byte(const unsigned char *src, size_t position,
                          const unsigned char *mask)
{
    return src[position] ^ (mask ? mask[position % MASK_SIZE] : 0);
}

int lz_decode_masked(const unsigned char *src, size_t src_size,
                      unsigned char *dst, size_t dst_size,
                      const unsigned char mask[MASK_SIZE])
{
    size_t input = 0;
    size_t output = 0;
    unsigned int flags;
    unsigned int bit;
    unsigned int token;
    size_t back;
    size_t length;

    while (output < dst_size) {
        if (input == src_size)
            return -1;
        flags = byte(src, input++, mask);
        for (bit = 0; bit < 8 && output < dst_size; ++bit) {
            if (!(flags & (1U << bit))) {
                if (input == src_size)
                    return -1;
                dst[output++] = (unsigned char)byte(src, input++, mask);
            } else {
                if (src_size - input < 2)
                    return -1;
                token = byte(src, input, mask) | (byte(src, input + 1, mask) << 8);
                input += 2;
                back = (token >> 4) + 1;
                length = (token & 15) + 3;
                if (back > output || length > dst_size - output)
                    return -1;
                while (length--) {
                    dst[output] = dst[output - back];
                    ++output;
                }
            }
        }
    }
    return input == src_size ? 0 : -1;
}

int lz_decode(const unsigned char *src, size_t src_size,
               unsigned char *dst, size_t dst_size)
{
    return lz_decode_masked(src, src_size, dst, dst_size, NULL);
}

/* FNV-1a: a small corruption check, not encryption. */
uint32_t checksum(const unsigned char *src, size_t size)
{
    uint32_t value = UINT32_C(2166136261);

    while (size--)
        value = (value ^ *src++) * UINT32_C(16777619);
    return value;
}
