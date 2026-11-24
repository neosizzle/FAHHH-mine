.section .rodata
.global stub_blob
.global stub_blob_end
.hidden stub_blob
.hidden stub_blob_end
.balign 8
stub_blob:
    .incbin "build/stub.elf"
stub_blob_end:
.section .note.GNU-stack,"",@progbits
