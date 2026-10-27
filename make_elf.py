#!/usr/bin/env python3
"""
Build a minimal, *valid* static-PIE x86-64 Linux ELF (ET_DYN, no interpreter)
with the exact program-header layout requested:

    PT_LOAD #1 : vaddr 0x1000, size 0x1100, flags R+X   <- code
    PT_LOAD #2 : vaddr 0x2000, size 0x1000, flags R     <- "hello world" string

The code prints the string, which lives in the SECOND segment, reached with a
RIP-relative LEA (required for PIE, since the kernel chooses the load bias at
runtime and there are no relocations).

Note the overlap, the point of this whole exercise:

    seg1 covers [0x1000, 0x1000+0x1100) = [0x1000, 0x2100)
    seg2 covers [0x2000, 0x2000+0x1000) = [0x2000, 0x3000)

So page 0x2000 is claimed by BOTH segments. The loader maps seg1 first (R+X),
then seg2 (R) over page 0x2000 with MAP_FIXED, so that page ends up READ-ONLY
with seg2's permissions. It still holds the correct string because both
segments reference the same file offset (0x2000) for that page.

No assembler or linker: every byte is emitted by hand.
"""

import os
import struct
import sys

PAGE      = 0x1000
EHDR_SIZE = 64
PHDR_SIZE = 56
PHNUM     = 2

# --- requested segment layout --------------------------------------------
SEG1_VADDR, SEG1_OFF, SEG1_SIZE = 0x1000, 0x1000, 0x2200   # R+X, code
SEG2_VADDR, SEG2_OFF, SEG2_SIZE = 0x2000, 0x2000, 0x1000   # R,   string

MSG       = b"hello world\n"
msg_vaddr = SEG2_VADDR          # string placed at the very start of segment 2

# --- machine code (lives in segment 1, at vaddr 0x1000) ------------------
#   mov eax, 1              ; __NR_write
#   mov edi, 1             ; fd = stdout
#   lea rsi, [rip + msg]   ; &string, in segment 2 (position independent)
#   mov edx, len
#   syscall
#   mov eax, 60            ; __NR_exit
#   xor edi, edi
#   syscall
p1 = b"\xb8\x01\x00\x00\x00"                 # mov eax, 1          (5)
p2 = b"\xbf\x01\x00\x00\x00"                 # mov edi, 1          (5)
# lea rsi,[rip+disp32] is "48 8d 35 <disp32>" (7 bytes); disp is relative
# to the address of the *next* instruction.
lea_addr  = SEG1_VADDR + len(p1) + len(p2)
lea_next  = lea_addr + 7
disp      = msg_vaddr - lea_next
assert lea_next + disp == msg_vaddr
p3 = b"\x48\x8d\x35" + struct.pack("<i", disp)   # lea rsi,[rip+disp]  (7)
p4 = b"\xba" + struct.pack("<I", len(MSG))       # mov edx, len        (5)
p5 = b"\x0f\x05"                                  # syscall            (2)
p6 = b"\xb8\x3c\x00\x00\x00"                      # mov eax, 60        (5)
p7 = b"\x31\xff"                                  # xor edi, edi       (2)
p8 = b"\x0f\x05"                                  # syscall            (2)
code = p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8

entry = SEG1_VADDR   # relative to load bias

# --- ELF64 header --------------------------------------------------------
e_ident  = b"\x7fELF\x02\x01\x01\x00" + b"\x00" * 8
ehdr = e_ident + struct.pack(
    "<HHIQQQIHHHHHH",
    3,          # e_type    = ET_DYN  (PIE)
    0x3e,       # e_machine = EM_X86_64
    1,          # e_version
    entry,      # e_entry   (bias-relative)
    EHDR_SIZE,  # e_phoff
    0,          # e_shoff
    0,          # e_flags
    EHDR_SIZE,  # e_ehsize
    PHDR_SIZE,  # e_phentsize
    PHNUM,      # e_phnum
    0, 0, 0,    # e_shentsize, e_shnum, e_shstrndx
)

# --- program headers -----------------------------------------------------
PT_LOAD = 1
PF_X, PF_W, PF_R = 1, 2, 4

def phdr(flags, off, vaddr, size):
    return struct.pack(
        "<IIQQQQQQ",
        PT_LOAD, flags, off, vaddr, vaddr,   # p_paddr == p_vaddr
        size, size, PAGE,                    # p_filesz == p_memsz
    )

ph1 = phdr(PF_R | PF_X, SEG1_OFF, SEG1_VADDR, SEG1_SIZE)   # R+X
ph2 = phdr(PF_R,        SEG2_OFF, SEG2_VADDR, SEG2_SIZE)   # R

# --- assemble the file ---------------------------------------------------
# file offset == vaddr for both segments, so the image spans up to seg2.
file_len = SEG2_OFF + len(MSG)
blob = bytearray(file_len + 1024)
blob[0:len(ehdr)]                 = ehdr
blob[EHDR_SIZE:EHDR_SIZE+PHDR_SIZE]                 = ph1
blob[EHDR_SIZE+PHDR_SIZE:EHDR_SIZE+2*PHDR_SIZE]     = ph2
blob[SEG1_OFF:SEG1_OFF+len(code)] = code                   # code at file 0x1000

gap_start = SEG1_OFF + len(code)
blob[gap_start:SEG2_OFF] = b"\x01" * (SEG2_OFF - gap_start)

blob[SEG2_OFF:SEG2_OFF+len(MSG)]  = MSG                    # string at file 0x2000
blob[SEG2_OFF+len(MSG):] = b"\x02" * (1024)                 # code at file 0x1000


out = sys.argv[1] if len(sys.argv) > 1 else "hello_pie_overlap"
with open(out, "wb") as f:
    f.write(blob)
os.chmod(out, 0o755)

print(f"wrote {out} ({file_len} bytes)")
print(f"  type       = ET_DYN (PIE)")
print(f"  entry      = {entry:#x}  (bias-relative)")
print(f"  lea disp   = {disp:#x}  -> &msg = {msg_vaddr:#x}")
print(f"  PT_LOAD #1 = [{SEG1_VADDR:#x}, {SEG1_VADDR+SEG1_SIZE:#x})  R+X  (code)")
print(f"  PT_LOAD #2 = [{SEG2_VADDR:#x}, {SEG2_VADDR+SEG2_SIZE:#x})  R    (string)")
print(f"  overlap    = [{SEG2_VADDR:#x}, {SEG1_VADDR+SEG1_SIZE:#x})  (page {SEG2_VADDR:#x}, seg2 wins -> R)")