#ifndef WOODY_H
# define WOODY_H

# include <stddef.h>
# include <stdint.h>

# define WOODY_LEGACY_MAGIC UINT64_C(0x315a4c59444f4f57)
# define WOODY_MAGIC UINT64_C(0x325a4c59444f4f57)
# define MASK_SIZE 16
# define PAGE_SIZE 4096UL

typedef struct s_info {
    uint64_t magic;
    uint64_t self;
    uint64_t entry;
    uint64_t target;
    uint64_t raw_size;
    uint64_t packed_size;
    uint32_t checksum;
    uint32_t prot;
    unsigned char mask[MASK_SIZE];
} t_info;

typedef struct s_report {
    size_t raw_size;
    size_t compressed_size;
    const char *reason;
} t_report;

_Static_assert(sizeof(t_info) == 72, "entry.S header must match t_info");

unsigned char *lz_encode(const unsigned char *src, size_t size, size_t *out_size);
int lz_decode(const unsigned char *src, size_t src_size,
               unsigned char *dst, size_t dst_size);
int lz_decode_masked(const unsigned char *src, size_t src_size,
                      unsigned char *dst, size_t dst_size,
                      const unsigned char mask[MASK_SIZE]);
uint32_t checksum(const unsigned char *src, size_t size);
int validate_elf(const unsigned char *data, size_t size);
unsigned char *trim_sections(const unsigned char *data, size_t size,
                             size_t *trimmed_size);
int already_packed(const unsigned char *data);
int needs_early_code(const unsigned char *data, size_t size);
unsigned char *pack_elf(const unsigned char *data, size_t size,
                        size_t *out_size, t_report *report);
int error(const char *message);

#endif
