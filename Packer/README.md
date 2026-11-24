## Build and use

Requires the normal Linux C/assembly toolchain: `cc`, GNU `ld`, and `make`.
No additional package, NASM, or `objcopy` is needed.

```sh
make
cp /bin/ls ./ls
./woody ls -o ls
./ls -l
```

The same input/output name is supported. A separate output also works:

```sh
./woody ./program -o ./program.small
./program.small
```

`-o` may appear before or after the input. Without `-o`, the output is
`<input>.packed`. Quote paths containing spaces. Use `--` before an input
filename beginning with `-`. Input names refer to files, not commands in `PATH`.

Progress shows the original size, metadata removal, compressed code size, and
final percentage saved. Masking is automatic for compressed outputs. There is
no startup banner.

`make clean`, `make fclean`, and `make re` are available. `fclean` removes the
`woody` tool and build files, not the files it has processed.

## How it works

1. Validate the ELF header and program headers.
2. Remove a trailing suffix when it consists entirely of non-loaded section
   data, section headers, or zero padding. Clear the section-table fields.
   Unknown appended nonzero data prevents this trimming step.
3. Compress the largest suitable read/execute load segment with LZSS. The
   encoder uses a 4 KiB window, matches of 3 to 18 bytes, and a bounded hash-chain
   search. One flag byte describes eight literal/back-reference tokens. Mask the
   resulting bytes using 16 random bytes obtained through Linux `getrandom`.
4. Reduce file offsets by whole aligned pages and reuse an available note/null
   program-header entry for a small embedded unpacker. Loaded data and dynamic
   linker information stay available to the normal ELF loader.
5. On execution, that unpacker unmasks each byte as it decodes the stream and
   restores the code in anonymous temporary memory,
   checks its checksum, copies it back to its original virtual address, restores
   execute permission, and jumps to the original entry point. The assembly
   bridge preserves the initial registers, flags, and stack.

The output is one ELF executable. It runs in the same process: there is no
second executable, `memfd` execution, or external launcher. The temporary decode
buffer is memory, not an unpacked file on disk. PIE load bias is handled at
runtime.

The exact runtime decoder also verifies the compressed stream during packing.
The checksum detects corruption; it is not a security or encryption feature.

## Source files

| File | Purpose |
| --- | --- |
| `src/main.c` | Arguments, progress, file handling, atomic replacement |
| `src/elf.c` | ELF validation and internal metadata trimming |
| `src/pack.c` | Layout checks, packing, program-header updates |
| `src/lz_encode.c` | LZSS compressor |
| `src/lz_decode.c` | Shared masked/unmasked decoder and checksum |
| `src/unpack.c` | In-memory runtime restoration using Linux system calls |
| `src/entry.S` | Register/stack bridge to the original entry point |
| `src/stub.ld`, `src/embed.S` | Build and embed the unpacker |
| `include/woody.h` | Shared structures and declarations |