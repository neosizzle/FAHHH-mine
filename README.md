# Famine
A simple virus program that recursively infects the files in a given root directory by patching binary files; making them run a duplication routine that re-runs the virus logic before jumping back to the main program.

## Overview
This is built for fun, for a Ubuntu 37 machine with kernel 6.14.0. It currently does not do anything really maliclous, just does self replication on the systems binary only. Hence, it is unstable and there will be a risk of system failure when running this on root, such as when this process interferes with system processes who are using the binaries.

## Features
- Evasion routine
- Recursive Root file infection (unstable as system processes might use the programs mid-infection)
- Packer for static analysis prevention
- Launch infection routine as background process
- Multi infection prevention chech via signatures
- Re-infection on executing infected binaries

## Installation and usage
The following are required to build the program
- Cmake version 3.23
- NASM version 2

To build, create the build files
```
cmake -S . -B build <options>=ON/OFF
```

Current supported options are:
- DROOT_MODE - Root mode, which infects all binaries in the system in /usr/bin. Default OFF
- DISPLAY_INFECT - Enable infection logging. Default ON

And then run the build command
```
cmake --build build
```

Run the virus
```
./build/famine/Famine.packed # sudo is needed for build mode
```

## How it works
- elaborate on design and limitations
- elaborate on codecave construction: step by step
- elaborate on common pitfalls
- elaborate on tools and tips incl. util bash scripts

## Research pipeline
- gdaryl mission

