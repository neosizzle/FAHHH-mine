; syscall definitions
%define SYS_WRITE       1
%define SYS_EXIT        60
%define SYS_OPEN        2
%define SYS_CLOSE       3
%define SYS_GETDENTS64  217
%define SYS_MPROTECT    10

; SYS_OPEN
%define O_RDONLY        0x0
%define O_WRONLY        0x1
%define O_RDWR          0x2
%define O_DIRECTORY     0x10000 

%define PROT_NONE       0x0
%define PROT_READ       0x1
%define PROT_WRITE      0x2
%define PROT_EXEC       0x4

; dirent constants
%define DT_REG			0x8
%define DT_DIR			0x4

; structs
struc	dirent
.d_ino:			resq	1	; 64-bit inode number
.d_off:			resq	1	; 64-bit offset to next structure
.d_reclen		resw	1	; Size of this dirent
.d_name			resb	1	; Filename (null-terminated)
endstruc


; statics
%define DIRENT_BUF_SZ   128
