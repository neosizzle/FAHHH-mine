
#!/usr/bin/env python3
# This file is used to generate bad5
import struct

src = "/tmp/test/good-gcc"
dst = "/tmp/test/bad5"

with open(src, "rb") as f:
    data = bytearray(f.read())

# Verify 64-bit little-endian ELF
assert data[:4] == b"\x7fELF", "Not an ELF file"
assert data[4] == 2 and data[5] == 1, "Not a 64-bit little-endian ELF"

# ELF64 header fields
e_entry = struct.unpack_from("<Q", data, 24)[0]
e_phoff = struct.unpack_from("<Q", data, 32)[0]
e_phentsize = struct.unpack_from("<H", data, 54)[0]
e_phnum = struct.unpack_from("<H", data, 56)[0]

# Translate entry virtual address to file offset
entry_offset = None

for i in range(e_phnum):
    off = e_phoff + i * e_phentsize
    p_type, p_flags = struct.unpack_from("<II", data, off)
    p_offset, p_vaddr = struct.unpack_from("<QQ", data, off + 8)
    p_filesz = struct.unpack_from("<Q", data, off + 32)[0]

    if p_type == 1 and p_vaddr <= e_entry < p_vaddr + p_filesz:
        entry_offset = p_offset + (e_entry - p_vaddr)
        break

if entry_offset is None:
    raise RuntimeError("Could not map ELF entry point to a file offset")

print(f"Entry virtual address: 0x{e_entry:x}")
print(f"Entry file offset:     0x{entry_offset:x}")

payload = b"Famine version 1.0 (c)oded by <jng>-<gdaryl>"
insert_offset = entry_offset - len(payload)

if insert_offset < 0:
    raise RuntimeError("Entry point offset is too small")

data[insert_offset:insert_offset] = payload

with open(dst, "wb") as f:
    f.write(data)

print(f"Inserted 'hello' at file offset: 0x{insert_offset:x}")
print(f"Wrote {dst}")
