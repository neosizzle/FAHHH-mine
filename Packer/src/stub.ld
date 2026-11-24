ENTRY(loader_entry)
PHDRS { image PT_LOAD FLAGS(5); }
SECTIONS
{
    . = 0;
    .payload : {
        KEEP(*(.payload.header))
        KEEP(*(.payload.entry))
        *(.text*)
        *(.rodata*)
    } :image
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
    .unexpected : { *(.data*) *(.bss*) *(.got*) *(.plt*) } :image
    ASSERT(SIZEOF(.unexpected) == 0, "unpacker must be self-contained")
    ASSERT(loader_entry == 72, "unpacker metadata layout changed")
}
