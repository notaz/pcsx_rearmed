# PCSX ReARMed RSX backend

Build the PSL1GHT core with `make -f Makefile.libretro platform=psl1ght DYNAREC=lightrec BUILTIN_GPU=rsx`.
`BUILTIN_GPU=peops` keeps the existing software-only build. Other platforms keep their existing defaults.
The shader build uses Python 3 and PSL1GHT's `cgcomp` with its Cg runtime.
Generated shader binaries stay in the ignored `generated/` directory.

The experimental **GPU Renderer (Restart)** core option selects RSX or Software.
RSX requires a frontend implementing `libretro_rsx.h` version 1; failed negotiation uses software.
The core uses the frontend's command FIFO and its unused host mapping, with no frontend source include paths or core-specific frontend flags.
The frontend owns the host mapping. The core borrows 2 MiB streaming pages from its unused portion and reuses them only after GPU completion. Local-memory overflow pages are allocated only when existing pages remain busy. This mapping is separate from core RAM, the Lightrec code cache and local RSX images.
RetroArch's PSL1GHT build accepts `RSX_HOST_SIZE` in bytes (default 33554432); the current console test package uses `RSX_HOST_SIZE=16777216`. GPU resources are retired through the interface after queued rendering completes.

Polygons, sprites, lines, polylines, indexed textures, blending, masks, VRAM copies and 24-bit scanout run on the RSX.
Uploads and texture snapshots use the 3D engine to preserve command ordering. Contiguous snapshot tiles share one draw. CPU VRAM reads and save states reconstruct the required state from a bounded command journal using P.E.Op.S., without waiting for GPU readback.
Commands overwritten by later opaque operations are pruned while retaining texture and palette dependencies.
The renderer is experimental. Hardware edge and interpolation rules can differ from the reference. Software remains selectable for affected games.

`command.c` decodes GP0 primitives; `renderer.c` integrates the reference parser and the VRAM journal; `rsx.c` owns RSX resources and drawing.
`peops.c` wraps the existing P.E.Op.S. implementation without modifying its rasterizer.
`shaders.py` builds immutable programs with full precision and exact binary factors for packed texels. Per-draw parameters use vertex constants.

`PS3_RSX_DIAGNOSTICS=1` enables counters accessible to ProDG. It does not log per-frame activity and is off in normal builds.
The GPU remains on the main thread; the PSL1GHT platform disables the asynchronous GPU worker.

Semi-transparent lines and polygons use a bounded group of at most 64 primitives.
`batch.h` permits a shared pre-group VRAM snapshot only when no new primitive reads
an earlier primitive's write bounds. Destination, texture page and CLUT reads all
participate, including linear X carry and Y wrapping. Draw order and RGB5/STP/mask
shader calculations are preserved. Uploads, copies, readbacks, output, context
changes and dither changes flush the group. The storage is fixed and private to
the core; the frontend interface and host allocation are unchanged.

`make -C plugins/gpu_rsx/tests check` checks dependency boundaries and compares
2,000 ordered RGB5/STP/mask operations against sequential rendering. Set
`SANITIZER=address,undefined` for the Linux sanitizer build; native Windows builds
can use MinGW. Console fixtures additionally compare canonical VRAM with the
ungrouped backend and check exact textured, semi-transparent and RGB888 pixels.

The native blending path handles addition, reverse subtraction and quarter-source
addition for triangles and quads without overlapping triangles. It preserves
RGB5 saturation by storing each color channel as `8*n + residue` in RGBA8 VRAM
and decoding the integer byte before dividing by eight. Saturation can leave a
residue of seven; subsequent native operations and shader reads preserve the
same RGB5 result. Only scanout expands channels to display RGB. No extra images
or frontend protocol changes are needed.

Mixed STP textures use source alpha for additive blending. Subtraction and forced
mask writes split opaque and STP texels into disjoint passes. Texture and CLUT
dependencies still refresh the snapshot. Average blending, destination mask
checks, lines and overlapping quads retain the existing snapshot path.
`blend.h` checks eligibility; `tests/native_blend_test.c` covers the geometry and
exhaustively checks byte residues across native blending operations.

CPU VRAM journal dependencies use conservative 4x4 cells, stored in a persistent
4 KiB bitmap. Completely overwritten cells are retired even when a framebuffer
clear starts at a row that is not aligned to 16 pixels. This prevents unnecessary
capacity checkpoints and software reconstruction of obsolete draws. Hardware
snapshot tiles remain 16x16. `tests/journal_test.c` compares pruning with full
replay and a pixel dependency oracle, including partial clears and wrapped reads.
