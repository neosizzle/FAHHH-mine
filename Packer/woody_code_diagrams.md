# Woody code diagrams

These diagrams describe the current C/ASM version with LZSS compression and
per-file XOR masking. Build-time work, packing, and packed-program startup are
separate phases. No external programs are invoked by the packer.

## 1. Build and embedding

The packer sources are `main.c`, `elf.c`, `pack.c`, `lz_encode.c`, and
`lz_decode.c`. The decoder is compiled once for the host and once for the
freestanding unpacker.

```mermaid
flowchart TD
    H["woody.h: shared types and declarations"] -.-> C["Packer C sources"]
    H -.-> U["unpack.c and lz_decode.c"]
    C --> HC["Compile host objects"]
    E["entry.S"] --> O["Compile unpacker objects"]
    U --> O
    O --> S["ld with stub.ld: stub.elf"]
    S --> A["embed.S: include stub bytes"]
    A --> B["Compile embed.o"]
    HC --> L["Link woody with libc"]
    B --> L
```

`stub.ld` places the 72-byte metadata block before `loader_entry`, followed by
code and constants. It rejects unpackers requiring writable global data.

## 2. Packing command and file handling

This is the main control flow in `main.c`, with the ELF helpers in `elf.c` and
packing helpers in `pack.c`.

```mermaid
flowchart TD
    A["arguments(): input file and optional -o"] --> AV{"Arguments valid?"}
    AV -->|No| ERR["Report error and release resources"]
    AV -->|Yes| IO["Open input, fstat, output_ok, mmap"]
    IO --> VALID{"File checks and validate_elf pass?"}
    VALID -->|No| ERR
    VALID -->|Yes| VERSION{"already_packed(): existing format?"}
    VERSION -->|V1 or V2| KEEP["Keep bytes; explain legacy masking if needed"]
    VERSION -->|Unpacked| TRIM["trim_sections(): stripped candidate or original"]
    TRIM --> EARLY{"needs_early_code()?"}
    EARLY -->|Yes| BASE["Choose stripped candidate or original"]
    EARLY -->|No| PACK["pack_elf(): create packed candidate"]
    PACK --> RESULT{"Packed candidate returned?"}
    RESULT -->|No| BASE
    RESULT -->|Yes| CHOSEN["Choose smaller masked LZSS image"]
    KEEP --> SAVE["save(): temporary file, write, chmod, fsync"]
    BASE --> SAVE
    CHOSEN --> SAVE
    SAVE --> COMMIT{"Write and atomic rename succeed?"}
    COMMIT -->|No| TEMP["Remove temporary file; report error"]
    TEMP --> ERR
    COMMIT -->|Yes| DONE["Print sizes; free, munmap, close; return 0"]
```

Validation checks ELF64/x86-64 format, program-header bounds, segment sizes and
alignment, interpreter termination, and an executable entry point. Metadata
trimming requires the removed suffix to contain only non-loaded section data,
section headers, or zeros. Unknown appended nonzero data prevents trimming.

`needs_early_code()` checks dynamic preinit, text relocations, IFUNC resolvers,
and required dynamic metadata. Such inputs use the stripped/original candidate.
Already-packed V1 inputs are retained without adding masking; masking them
requires the original, unpacked ELF.

## 3. Packing internals

Every fallback below returns `NULL` to `process()`, which retains its base
candidate. Packing failures do not publish a partially packed file.

```mermaid
flowchart TD
    P["pack_elf()"] --> L{"layout(): suitable code and header slot?"}
    L -->|No| F["Return NULL with fallback reason"]
    L -->|Yes| C["lz_encode(): compress selected RX segment"]
    C --> M["random_mask(): getrandom; XOR compressed bytes"]
    C -->|Allocation failure| F
    M -->|Random generation failure| F
    M --> V["lz_decode_masked(): verify with memcmp"]
    V --> OK{"Decoded bytes match original?"}
    OK -->|No| F
    OK -->|Yes| SIZE{"Smaller after alignment and stub overhead?"}
    SIZE -->|No| F
    SIZE -->|Yes| IMAGE["Allocate output; copy payload and shifted tail"]
    IMAGE -->|Allocation failure| F
    IMAGE --> META["Fill t_info: addresses, sizes, checksum, mask"]
    META --> ELF["Update entry and offsets; clear section table"]
    ELF --> LOAD["Replace note/null header with unpacker PT_LOAD"]
    LOAD --> RET["Return packed bytes and output size"]
```

`layout()` chooses the largest suitable readable/executable, non-writable load
segment, checks page/file overlaps, and finds a replaceable note/null header
after the existing load headers. `note_slot()` preserves GNU property headers.
Offset reductions preserve the load segments' required alignment.

## 4. LZSS encoder and shared decoder

The encoder uses a 4 KiB window, a maximum match length of 18 bytes, and up to
128 hash-chain candidates. Each group has one flag byte and up to eight tokens.
A literal uses one byte; a back-reference uses two bytes for distance and length.

```mermaid
flowchart TD
    subgraph ENC["lz_encode.c: encoder"]
        E0["Next token; reserve group flag when needed"] --> E1["hash(): search previous matching positions"]
        E1 --> E2{"Match length at least 3?"}
        E2 -->|Yes| E3["Flag 1; emit distance and length"]
        E2 -->|No| E4["Flag 0; emit literal byte"]
        E3 --> E5["Update hash chains and input position"]
        E4 --> E5
        E5 --> E6{"Input complete?"}
        E6 -->|No| E0
        E6 -->|Yes| STREAM["Return LZSS stream"]
    end
    STREAM --> MASK["pack.c: XOR with random 16-byte mask"]
    MASK --> D0
    subgraph DEC["lz_decode.c: decoder"]
        D0["Next token; unmask group flag when needed"] --> D1{"Flag bit set?"}
        D1 -->|No| D2["Unmask one literal byte"]
        D1 -->|Yes| D3["Unmask reference; derive distance and length"]
        D2 --> D4{"Input and output bounds valid?"}
        D3 --> D4
        D4 -->|No| BAD["Return -1"]
        D4 -->|Yes| D5["Append literal or overlapping back-reference"]
        D5 --> D6{"Expected output complete?"}
        D6 -->|No| D0
        D6 -->|Yes| D7{"All input consumed?"}
        D7 -->|No| BAD
        D7 -->|Yes| GOOD["Return 0"]
    end
```

`lz_decode()` is the unmasked wrapper: it calls `lz_decode_masked()` with a null
mask. `byte()` applies the XOR mask when present. `checksum()` implements FNV-1a
over the original code; the runtime uses it to detect corrupted decoded bytes.

## 5. Startup of a newly packed executable

This runs when the output binary is executed, rather than when `woody` packs it.
The original register state and stack are restored before the final jump.

```mermaid
flowchart TD
    K["Linux loads packed ELF at its new entry"] --> ASM["entry.S: save registers and flags; align stack"]
    ASM --> U["unpack(): compute PIE load bias and target"]
    U --> HEADER{"Metadata magic and sizes valid?"}
    HEADER -->|No| FAIL["fail(): write error; exit_group 126"]
    HEADER -->|Yes| MMAP["mmap temporary RW decode buffer"]
    MMAP -->|Failure| FAIL
    MMAP -->|Success| DEC["lz_decode_masked(): unmask and decompress"]
    DEC --> VERIFY{"Decode succeeded and checksum matches?"}
    VERIFY -->|No| FAIL
    VERIFY -->|Yes| RW["mprotect original code pages to RW"]
    RW -->|Failure| FAIL
    RW -->|Success| COPY["Copy restored instructions to original address"]
    COPY --> RX["mprotect code pages back to RX"]
    RX -->|Failure| FAIL
    RX -->|Success| FREE["munmap temporary buffer; return original entry"]
    FREE --> RESTORE["entry.S: restore registers, flags, and stack"]
    RESTORE --> RUN["Jump to original program entry"]
```

`call()` is the inline-assembly Linux syscall bridge. Runtime restoration uses
`mmap`, `mprotect`, and `munmap`; failure reporting uses `write` and `exit_group`.
The `munmap` return value is ignored in the current implementation. No second
process or unpacked temporary executable is created.

## Shared structures

`woody.h` provides declarations, constants, and these structures:

| Structure / field | Purpose |
| --- | --- |
| `t_info.magic` | Identify the packed format |
| `t_info.self` | Unpacker's link-time address, used to calculate load bias |
| `t_info.entry` | Original program entry address |
| `t_info.target` | Original code segment address |
| `t_info.raw_size` | Restored code length |
| `t_info.packed_size` | Masked compressed payload length |
| `t_info.checksum` | FNV-1a value of original code |
| `t_info.prot` | Restored code permissions, RX |
| `t_info.mask` | Per-file 16-byte XOR mask |
| `t_report.raw_size`, `compressed_size` | Progress counters |
| `t_report.reason` | Packing result or fallback explanation |

`t_info` occupies 72 bytes. The old unmasked format has a 56-byte common prefix.
Masking is reversible obfuscation: its recovery information ships in the ELF.
