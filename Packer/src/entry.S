.section .payload.header,"a"
.global loader_info
.hidden loader_info
loader_info:
    .zero 72

.section .payload.entry,"ax"
.global loader_entry
.hidden loader_entry
loader_entry:
    endbr64
    lea -8(%rsp), %rsp
    pushfq
    push %rax
    push %rcx
    push %rdx
    push %rbx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, %r12
    and $-16, %rsp
    cld
    lea loader_info(%rip), %rdi
    call unpack
    mov %rax, 128(%r12)
    mov %r12, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rbx
    pop %rdx
    pop %rcx
    pop %rax
    popfq
    lea 8(%rsp), %rsp
    jmp *-8(%rsp)
.section .note.GNU-stack,"",@progbits
