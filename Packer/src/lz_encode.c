#include "woody.h"
#include <limits.h>
#include <stdlib.h>

#define HASH_SIZE 4096
#define WINDOW 4096
#define MAX_MATCH 18
#define SEARCH_LIMIT 128

static unsigned int hash(const unsigned char *s)
{
    return ((unsigned int)s[0] * 251 * 251 + s[1] * 251 + s[2])
        & (HASH_SIZE - 1);
}

/* One flag byte describes eight tokens. A token is a literal byte, or a
   two-byte (12-bit distance, 4-bit length) back-reference. */
unsigned char *lz_encode(const unsigned char *src, size_t size, size_t *out_size)
{
    int32_t head[HASH_SIZE];
    int32_t *previous;
    unsigned char *out;
    size_t position = 0;
    size_t used = 0;
    size_t flag;
    size_t best;
    size_t back;
    size_t length;
    size_t end;
    unsigned int bit;
    unsigned int h;
    unsigned int tries;
    unsigned int token;
    int32_t candidate;

    if (!size || size > INT_MAX || size > SIZE_MAX / sizeof(*previous))
        return NULL;
    out = malloc(size + (size + 7) / 8);
    previous = malloc(size * sizeof(*previous));
    if (!out || !previous) {
        free(out);
        free(previous);
        return NULL;
    }
    for (h = 0; h < HASH_SIZE; ++h)
        head[h] = -1;
    while (position < size) {
        flag = used++;
        out[flag] = 0;
        for (bit = 0; bit < 8 && position < size; ++bit) {
            best = 0;
            back = 0;
            if (size - position >= 3) {
                candidate = head[hash(src + position)];
                tries = 0;
                while (candidate >= 0 && position - (size_t)candidate <= WINDOW
                    && tries++ < SEARCH_LIMIT) {
                    length = 0;
                    while (length < MAX_MATCH && length < size - position
                        && src[candidate + length] == src[position + length])
                        ++length;
                    if (length > best) {
                        best = length;
                        back = position - (size_t)candidate;
                    }
                    if (best == MAX_MATCH)
                        break;
                    candidate = previous[candidate];
                }
            }
            if (best >= 3) {
                out[flag] |= (unsigned char)(1U << bit);
                token = (unsigned int)((back - 1) << 4) | (unsigned int)(best - 3);
                out[used++] = (unsigned char)token;
                out[used++] = (unsigned char)(token >> 8);
            } else {
                best = 1;
                out[used++] = src[position];
            }
            end = position + best;
            while (position < end) {
                if (size - position >= 3) {
                    h = hash(src + position);
                    previous[position] = head[h];
                    head[h] = (int32_t)position;
                }
                ++position;
            }
        }
    }
    free(previous);
    *out_size = used;
    return out;
}
