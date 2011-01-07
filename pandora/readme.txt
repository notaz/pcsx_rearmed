
PCSX-ReARMed - yet another PCSX fork


About
-----

PCSX ReARMed is yet another PCSX fork based on the PCSX-Reloaded project,
which itself contains code from PCSX, PCSX-df and PCSX-Revolution. This
version is ARM architecture oriented and features MIPS->ARM recompiler by
Ari64. It was created for Pandora handheld, but should be usable on other
devices after some code adjustments.

PCSX ReARMed features GPU and SPU plugins from PCSX4ALL project.


Usage
-----

This version features a framebuffer driven menu that can be used to run
games and configure the emulator.

Supportd CD image formats:
- .bin/.cue
- .bin/.toc
- .img/.ccd/.sub
- .mdf/.mds
- .Z/.Z.table

CDDA (CD audio) support requires .cue/.bin format.


Plugins
-------

GPU (graphics) and SPU (sound) plugins can be selected in
[BIOS/Plugins] menu:

builtin_gpu    - the P.E.Op.S. GPU plugin, most accurate but slow.
gpuPCSX4ALL.so - plugin from PCSX4ALL project. Faster but has some glitches.
gpuGLES.so     - experimental port of P.E.Op.S. MesaGL plugin to OpenGL ES.
                 Occasionally faster but has lots of glitches and seems to
                 be rather unstable (may crash the system).
builtin_spu    - P.E.Op.S. SPU plugin, most accurate but slow.
spunull.so     - NULL plugin, i.e. no sound emulation.


Changelog
---------

r3 (2011-01-05):
+ added Pickle's port of gpu-gles from psx4m project
+ added PCSX4ALL gpu as a plugin
* improved gpu plugin support
+ added savestate preview
* various frontend fixes

r2 (2010-12-29):
* fixed memcard paths
* fixed a keybind copy-paste bug
* properly implemented pad handling
  (inputs no longer control both emulated pads at once)
* fixed a crash caused by framebuffer out of range access
* fixed SWL/SWR handling (usually resulted in graphic glitches)
* fixed BxxZAL (Medal of Honor)
* fixed alignment crash in color space conversion code (Lunar)
* fixed SWC2 occasional use of wrong address register (Parasite Eve)
* fixed firstfile() handling in HLE BIOS (broken memory cards in some games)
+ added per-game configs (controls still not saved though)
+ added simple plugin select interface to the menu

r1 (2010-12-25):
* initial release


Credits / License
-----------------

Emulator core:

(C) 1999-2003 PCSX Team
	(c) 1998 Vision Thing
	Linuzappz     <linuzappz@pcsx.net>
	Shadow        <shadow@pcsx.net>
	Pete Bernett  <psswitch@online.de>
	NoComp        <NoComp@mailcity.com>
	Nik3d
	Akumax        <akumax@pcsx.net>

(C) 2005-2009 PCSX-df Team
	(c) Ryan Schultz <schultz.ryan@gmail.com>
	(c) Andrew Burton <adb@iinet.net.au>
	(c) Stephen Chao <schao@myrealbox.com>
	(c) Marcus Comstedt <marcus@mc.pp.se>
	Stefan Sikora <hoshy@schrauberstube.de>

(C) 2009-2010 PCSX-Reloaded Team
	edgbla (Root counters, various core/plugin fixes)
	Firnis (GTE code from PCSX-Revolution Project)
	Gabriele Gorla (MDEC decoder)
	Peter Collingbourne (Various core/psxbios fixes)
	Dario, NeToU, siveritas (Various bugfixes)
	shalma (GTE Divider, various core fixes)
	Wei Mingzhi (Maintainer, input plugin, iso/cheat support, misc stuff)

PCSX4ALL GPU plugin:
	(C) 2010 PCSX4ALL Team
	(C) 2010 Unai
	Franxis <franxism@gmail.com>
	Chui <sdl_gp32@yahoo.es>

GLES plugin (psx4m project):
	(C) 1999-2009 by Pete Bernert
	EQ
	Olli Hinkka
	Proger
	Pickle

builtin GPU/SPU plugins:
	Pete Bernert and the P.E.Op.S. team

MIPS->ARM recompiler:
	(C) 2009-2010 Ari64

integration, optimization and frontend:
	(C) 2010-2011 notaz

Source code is released under GNU GPL license, version 2 or later.
See COPYING included in the archive (.pnd can be extracted using unsquashfs).
The source code is available in a GIT repository at:

git://notaz.gp2x.de/~notaz/pcsx_rearmed.git

