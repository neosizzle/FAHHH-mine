#include "woody.h"
#include <elf.h>
#include <stdlib.h>
#include <string.h>

static int inside(size_t size, uint64_t offset, uint64_t length)
{
    return offset <= size && length <= size - offset;
}

int validate_elf(const unsigned char *data, size_t size)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *p;
    size_t i;
    int entry = 0;

    if (size < sizeof(*h) || memcmp(h->e_ident, ELFMAG, SELFMAG))
        return error("input must be an ELF executable");
    if (h->e_ident[EI_CLASS] != ELFCLASS64
        || h->e_ident[EI_DATA] != ELFDATA2LSB || h->e_machine != EM_X86_64)
        return error("only little-endian x86-64 ELF files are supported");
    if (h->e_ident[EI_VERSION] != EV_CURRENT || h->e_version != EV_CURRENT
        || h->e_ehsize != sizeof(*h)
        || (h->e_type != ET_EXEC && h->e_type != ET_DYN)
        || h->e_phentsize != sizeof(*p) || h->e_phoff % 8
        || !h->e_phnum || h->e_phnum == PN_XNUM
        || !inside(size, h->e_phoff, (uint64_t)h->e_phnum * sizeof(*p)))
        return error("invalid or unsupported ELF headers");
    p = (const Elf64_Phdr *)(data + h->e_phoff);
    for (i = 0; i < h->e_phnum; ++i) {
        if (!inside(size, p[i].p_offset, p[i].p_filesz))
            return error("invalid segment bounds");
        if (p[i].p_type == PT_INTERP
            && (!p[i].p_filesz
                || !memchr(data + p[i].p_offset, 0, p[i].p_filesz)))
            return error("invalid ELF interpreter");
        if (p[i].p_type != PT_LOAD)
            continue;
        if (p[i].p_filesz > p[i].p_memsz
            || p[i].p_memsz > UINT64_MAX - p[i].p_vaddr
            || p[i].p_offset % 4096 != p[i].p_vaddr % 4096
            || (p[i].p_align > 1 && ((p[i].p_align & (p[i].p_align - 1))
                || p[i].p_offset % p[i].p_align != p[i].p_vaddr % p[i].p_align)))
            return error("invalid load segment");
        if ((p[i].p_flags & PF_X) && h->e_entry >= p[i].p_vaddr
            && h->e_entry - p[i].p_vaddr < p[i].p_filesz)
            entry = 1;
    }
    return entry ? 0 : error("no executable entry point");
}

/* Trim only when the entire suffix is section data, the section table, or zero
   padding. Unknown appended nonzero data causes the original image to be kept. */
unsigned char *trim_sections(const unsigned char *data, size_t size,
                             size_t *trimmed_size)
{
    const Elf64_Ehdr *h = (const Elf64_Ehdr *)data;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)(data + h->e_phoff);
    const Elf64_Shdr *sh;
    unsigned char *owned;
    unsigned char *out;
    size_t cut = h->e_phoff + h->e_phnum * sizeof(*ph);
    size_t i;
    size_t start;
    size_t end;

    *trimmed_size = size;
    for (i = 0; i < h->e_phnum; ++i)
        if (ph[i].p_filesz && ph[i].p_offset + ph[i].p_filesz > cut)
            cut = ph[i].p_offset + ph[i].p_filesz;
    if (cut < sizeof(*h))
        cut = sizeof(*h);
    if (cut >= size || !h->e_shnum || h->e_shoff % 8
        || h->e_shentsize != sizeof(*sh)
        || !inside(size, h->e_shoff, (uint64_t)h->e_shnum * sizeof(*sh)))
        return NULL;
    owned = calloc(size - cut, 1);
    if (!owned)
        return NULL;
    start = h->e_shoff > cut ? h->e_shoff : cut;
    end = h->e_shoff + h->e_shnum * sizeof(*sh);
    if (end > start)
        memset(owned + start - cut, 1, end - start);
    sh = (const Elf64_Shdr *)(data + h->e_shoff);
    for (i = 0; i < h->e_shnum; ++i) {
        if (sh[i].sh_type == SHT_NOBITS)
            continue;
        if (!inside(size, sh[i].sh_offset, sh[i].sh_size))
            break;
        start = sh[i].sh_offset > cut ? sh[i].sh_offset : cut;
        end = sh[i].sh_offset + sh[i].sh_size;
        if (end <= start)
            continue;
        if (sh[i].sh_flags & SHF_ALLOC)
            break;
        memset(owned + start - cut, 1, end - start);
    }
    if (i == h->e_shnum) {
        for (i = cut; i < size; ++i)
            if (data[i] && !owned[i - cut])
                break;
    } else
        i = 0;
    free(owned);
    if (i != size)
        return NULL;
    out = malloc(cut);
    if (!out)
        return NULL;
    memcpy(out, data, cut);
    ((Elf64_Ehdr *)out)->e_shoff = 0;
    ((Elf64_Ehdr *)out)->e_shnum = 0;
    ((Elf64_Ehdr *)out)->e_shstrndx = SHN_UNDEF;
    *trimmed_size = cut;
    return out;
}
