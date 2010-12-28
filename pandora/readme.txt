
PCSX-ReARMed - yet another PCSX fork


About
-----

PCSX ReARMed is yet another PCSX fork based on the PCSX-Reloaded project,
which itself contains code from PCSX, PCSX-df and PCSX-Revolution. This
version is ARM architecture oriented and features MIPS->ARM recompiler by
Ari64. It was created for Pandora handheld, but should be usable on other
devices after some code adjustments.

It does not, however, have anything in common with similar psx4all and
pcsx4all projects, except that all of these are PCSX ports.


Usage
-----

This version features a framebuffer driven menu that can be used to run
games and configure the emulator.

Supportd CD image formats:
- .cue/.bin
- .toc/.bin
- .img
- .mds
- .Z/.Z.table

CDDA (CD audio) support requires .cue/.bin format.


Credits / License
-----------------

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
	avlex (Help on xcode project)
	Dario (Various bugfixes)
	edgbla (Root counters, various core/plugin fixes)
	Firnis (GTE code from PCSX-Revolution Project)
	Gabriele Gorla (MDEC decoder)
	maggix (Snow Leopard compile fix)
	NeToU (Bugfix)
	Peter Collingbourne (Various core/psxbios fixes)
	siveritas (Bugfix)
	shalma (GTE Divider, various core fixes)
	Tristin Celestin (PulseAudio support)
	Wei Mingzhi (Maintainer, input plugin, iso/cheat support, misc stuff)

GPU and SPU code by Pete Bernert and the P.E.Op.S. team
ARM recompiler (C) 2009-2010 Ari64

integration, optimization and frontend (C) 2010 notaz

Source code is released under GNU GPL license, version 2 or later.
See COPYING included in the archive (.pnd can be extracted using unsquashfs).
The source code is available in a GIT repository at:

git://notaz.gp2x.de/~notaz/pcsx_rearmed.git

