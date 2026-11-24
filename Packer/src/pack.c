#include "woody.h"
#include <elf.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

extern const unsigned char stub_blob[];
extern const unsigned char stub_blob_end[];

static int inside(size_t size, uint64_t offset, uint64_t length)
{
    return offset <= size && length <= size - offset;
}

int already_packed(const unsigned char *data)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *p = (const Elf64_Phdr *)(data + h->e_phoff);
    t_info info;
    size_t i;

    for (i = 0; i < h->e_phnum; ++i) {
        if (p[i].p_type != PT_LOAD || p[i].p_filesz < 56)
            continue;
        /* The first 56 bytes are shared with the original unmasked format. */
        memcpy(&info, data + p[i].p_offset, 56);
        if (info.self == p[i].p_vaddr && info.magic == WOODY_MAGIC
            && p[i].p_filesz >= sizeof(info)
            && h->e_entry == info.self + sizeof(info))
            return 2;
        if (info.self == p[i].p_vaddr && info.magic == WOODY_LEGACY_MAGIC
            && h->e_entry == info.self + 56)
            return 1;
    }
    return 0;
}

static int random_mask(unsigned char mask[MASK_SIZE])
{
    size_t done = 0;
    ssize_t n;

    while (done < MASK_SIZE) {
        n = getrandom(mask + done, MASK_SIZE - done, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        done += (size_t)n;
    }
    return 0;
}

/* Dynamic IFUNC resolvers, text relocations and preinit functions can execute
   before the new entry point. Keep those binaries uncompressed. */
int needs_early_code(const unsigned char *data, size_t size)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *p = (const Elf64_Phdr *)(data + h->e_phoff);
    const Elf64_Shdr *s;
    Elf64_Dyn d;
    Elf64_Sym symbol;
    Elf64_Rela relocation;
    uint64_t offset;
    size_t i;
    int dynamic = 0;

    for (i = 0; i < h->e_phnum; ++i)
        if (p[i].p_type == PT_INTERP)
            dynamic = 1;
    if (!dynamic)
        return 0;
    for (i = 0; i < h->e_phnum; ++i) {
        if (p[i].p_type != PT_DYNAMIC)
            continue;
        if (p[i].p_filesz % sizeof(d))
            return 1;
        for (offset = 0; offset < p[i].p_filesz; offset += sizeof(d)) {
            memcpy(&d, data + p[i].p_offset + offset, sizeof(d));
            if (d.d_tag == DT_NULL)
                break;
            if (d.d_tag == DT_TEXTREL
                || (d.d_tag == DT_FLAGS && (d.d_un.d_val & DF_TEXTREL))
                || (d.d_tag == DT_PREINIT_ARRAYSZ && d.d_un.d_val))
                return 1;
        }
    }
    if (!h->e_shnum || h->e_shoff % 8 || h->e_shentsize != sizeof(*s)
        || !inside(size, h->e_shoff, (uint64_t)h->e_shnum * sizeof(*s)))
        return 1;
    s = (const Elf64_Shdr *)(data + h->e_shoff);
    for (i = 0; i < h->e_shnum; ++i) {
        if (s[i].sh_type != SHT_DYNSYM && s[i].sh_type != SHT_RELA)
            continue;
        if (!inside(size, s[i].sh_offset, s[i].sh_size))
            return 1;
        if (s[i].sh_type == SHT_DYNSYM) {
            if (s[i].sh_entsize != sizeof(symbol) || s[i].sh_size % sizeof(symbol))
                return 1;
            for (offset = 0; offset < s[i].sh_size; offset += sizeof(symbol)) {
                memcpy(&symbol, data + s[i].sh_offset + offset, sizeof(symbol));
                if (ELF64_ST_TYPE(symbol.st_info) == STT_GNU_IFUNC
                    && symbol.st_shndx != SHN_UNDEF)
                    return 1;
            }
        } else {
            if (s[i].sh_entsize != sizeof(relocation)
                || s[i].sh_size % sizeof(relocation))
                return 1;
            for (offset = 0; offset < s[i].sh_size; offset += sizeof(relocation)) {
                memcpy(&relocation, data + s[i].sh_offset + offset,
                       sizeof(relocation));
                if (ELF64_R_TYPE(relocation.r_info) == R_X86_64_IRELATIVE)
                    return 1;
            }
        }
    }
    return 0;
}

static int note_slot(const Elf64_Ehdr *h, const Elf64_Phdr *p, size_t i)
{
    size_t j;

    if (p[i].p_type == PT_NULL)
        return 1;
    if (p[i].p_type != PT_NOTE)
        return 0;
    for (j = 0; j < h->e_phnum; ++j)
        if (p[j].p_type == PT_GNU_PROPERTY
            && p[i].p_offset < p[j].p_offset + p[j].p_filesz
            && p[j].p_offset < p[i].p_offset + p[i].p_filesz)
            return 0;
    return 1;
}

static int layout(const unsigned char *data, size_t *code,
                   size_t *slot, uint64_t *high, uint64_t *alignment)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *p = (const Elf64_Phdr *)(data + h->e_phoff);
    size_t last_load = 0;
    size_t i;
    uint64_t start;
    uint64_t end;
    uint64_t page_start;
    uint64_t page_end;

    *code = SIZE_MAX;
    *slot = SIZE_MAX;
    *high = 0;
    *alignment = PAGE_SIZE;
    for (i = 0; i < h->e_phnum; ++i) {
        if (p[i].p_type != PT_LOAD)
            continue;
        last_load = i;
        if (p[i].p_vaddr + p[i].p_memsz > UINT64_MAX - PAGE_SIZE)
            return 1;
        if (p[i].p_vaddr + p[i].p_memsz > *high)
            *high = p[i].p_vaddr + p[i].p_memsz;
        if (p[i].p_align > *alignment)
            *alignment = p[i].p_align;
        if ((p[i].p_flags & (PF_R | PF_W | PF_X)) == (PF_R | PF_X)
            && (*code == SIZE_MAX || p[i].p_filesz > p[*code].p_filesz))
            *code = i;
    }
    for (i = last_load + 1; i < h->e_phnum; ++i)
        if (note_slot(h, p, i))
            *slot = i;
    if (*code == SIZE_MAX || *slot == SIZE_MAX)
        return 1;
    start = p[*code].p_offset;
    end = start + p[*code].p_filesz;
    if (start < sizeof(*h) || h->e_phoff + h->e_phnum * sizeof(*p) > start)
        return 1;
    page_start = p[*code].p_vaddr & ~(PAGE_SIZE - 1);
    page_end = (p[*code].p_vaddr + p[*code].p_memsz + PAGE_SIZE - 1)
        & ~(PAGE_SIZE - 1);
    for (i = 0; i < h->e_phnum; ++i) {
        if (i == *code)
            continue;
        if (p[i].p_filesz && p[i].p_offset < end
            && p[i].p_offset + p[i].p_filesz > start)
            return 1;
        if (p[i].p_type == PT_LOAD && p[i].p_memsz
            && (p[i].p_vaddr & ~(PAGE_SIZE - 1)) < page_end
            && p[i].p_vaddr + p[i].p_memsz > page_start)
            return 1;
    }
    return 0;
}

unsigned char *pack_elf(const unsigned char *data, size_t size,
                        size_t *out_size, t_report *report)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *p = (const Elf64_Phdr *)(data + h->e_phoff);
    const Elf64_Ehdr *stub_h = (const Elf64_Ehdr *)stub_blob;
    const Elf64_Phdr *stub_p = (const Elf64_Phdr *)(stub_blob + stub_h->e_phoff);
    size_t blob_size = (uintptr_t)stub_blob_end - (uintptr_t)stub_blob;
    size_t stub_size = stub_p->p_filesz;
    size_t code;
    size_t slot;
    size_t cut;
    size_t removed;
    size_t stub_offset;
    size_t i;
    uint64_t high;
    uint64_t alignment;
    unsigned char *compressed;
    unsigned char *verify;
    unsigned char *out;
    Elf64_Ehdr *new_h;
    Elf64_Phdr *new_p;
    t_info info;

    report->reason = "layout needs stripping only (no safe code/note slot or early code)";
    if (layout(data, &code, &slot, &high, &alignment))
        return NULL;
    report->raw_size = p[code].p_filesz;
    report->reason = "cannot allocate compression buffers";
    compressed = lz_encode(data + p[code].p_offset, report->raw_size,
                           &report->compressed_size);
    if (!compressed)
        return NULL;
    report->reason = "cannot generate the payload mask";
    if (random_mask(info.mask)) {
        free(compressed);
        return NULL;
    }
    for (i = 0; i < report->compressed_size; ++i)
        compressed[i] ^= info.mask[i % MASK_SIZE];
    verify = malloc(report->raw_size);
    if (!verify || lz_decode_masked(compressed, report->compressed_size,
                                    verify, report->raw_size, info.mask)
        || memcmp(verify, data + p[code].p_offset, report->raw_size)) {
        free(verify);
        free(compressed);
        report->reason = "compression verification failed";
        return NULL;
    }
    free(verify);
    report->reason = "compression plus unpacker would not reduce the file size";
    if (report->compressed_size >= report->raw_size) {
        free(compressed);
        return NULL;
    }
    removed = (report->raw_size - report->compressed_size) / alignment * alignment;
    cut = p[code].p_offset + p[code].p_filesz;
    stub_offset = (size - removed + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (stub_h->e_phnum != 1 || stub_size < sizeof(info)
        || !inside(blob_size, stub_p->p_offset, stub_size)
        || stub_size > SIZE_MAX - stub_offset || stub_offset + stub_size >= size) {
        free(compressed);
        return NULL;
    }
    *out_size = stub_offset + stub_size;
    out = calloc(*out_size, 1);
    if (!out) {
        free(compressed);
        report->reason = "cannot allocate packed output";
        return NULL;
    }
    memcpy(out, data, p[code].p_offset);
    memcpy(out + p[code].p_offset, compressed, report->compressed_size);
    memcpy(out + cut - removed, data + cut, size - cut);
    memcpy(out + stub_offset, stub_blob + stub_p->p_offset, stub_size);
    free(compressed);
    info.magic = WOODY_MAGIC;
    info.self = (high + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    info.entry = h->e_entry;
    info.target = p[code].p_vaddr;
    info.raw_size = report->raw_size;
    info.packed_size = report->compressed_size;
    info.checksum = checksum(data + p[code].p_offset, report->raw_size);
    info.prot = 5; /* PROT_READ | PROT_EXEC */
    memcpy(out + stub_offset, &info, sizeof(info));
    new_h = (Elf64_Ehdr *)out;
    new_h->e_entry = info.self + sizeof(info);
    new_h->e_shoff = 0;
    new_h->e_shnum = 0;
    new_h->e_shstrndx = SHN_UNDEF;
    new_p = (Elf64_Phdr *)(out + h->e_phoff);
    for (i = 0; i < h->e_phnum; ++i)
        if (new_p[i].p_offset >= cut)
            new_p[i].p_offset -= removed;
    new_p[code].p_filesz = report->compressed_size;
    new_p[slot].p_type = PT_LOAD;
    new_p[slot].p_flags = PF_R | PF_X;
    new_p[slot].p_offset = stub_offset;
    new_p[slot].p_vaddr = info.self;
    new_p[slot].p_paddr = info.self;
    new_p[slot].p_filesz = stub_size;
    new_p[slot].p_memsz = stub_size;
    new_p[slot].p_align = PAGE_SIZE;
    report->reason = "masked LZSS code segment + embedded unpacker";
    return out;
}
