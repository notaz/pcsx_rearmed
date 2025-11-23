PCSX-ReARMed - yet another PCSX fork
====================================

![CI (Linux)](https://github.com/notaz/pcsx_rearmed/workflows/CI%20(Linux)/badge.svg)

PCSX ReARMed is yet another PCSX fork based on the PCSX-Reloaded project,
which itself contains code from PCSX, PCSX-df and PCSX-Revolution. This
version was originally ARM architecture oriented (hence the name) with
its MIPS->ARM dynamic recompilation and assembly optimizations, but more
recently it targets other architectures too. A fork of this emulator was
used in [PS Classic](https://en.wikipedia.org/wiki/PlayStation_Classic)
(without any coordination or even notification).

## Features
* ARM/ARM64 dynamic recompiler by Ari64
* [lightrec](https://github.com/pcercuei/lightrec/) dynamic recompiler for other architectures
* NEON GPU by Exophase for ARM NEON and x86 SSE2+
* PCSX4ALL GPU by Una-i/senquack for other architectures
* heavily modified P.E.Op.S. SPU
* BIOS HLE emulation (most games run without proprietary BIOS)
* libretro support

