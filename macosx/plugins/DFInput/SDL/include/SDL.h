/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
// 7/31/2010 Wei Mingzhi
// Removed everything unrated to Mac OS X Joystick support

/**
 *  \file SDL.h
 *  
 *  Main include header for the SDL library
 */

/**
 *  \mainpage Simple DirectMedia Layer (SDL)
 *  
 *  http://www.libsdl.org/
 *  
 *  \section intro_sec Introduction
 *  
 *  This is the Simple DirectMedia Layer, a general API that provides low
 *  level access to audio, keyboard, mouse, joystick, 3D hardware via OpenGL,
 *  and 2D framebuffer across multiple platforms.
 *  
 *  The current version supports Windows, Windows CE, Mac OS X, Linux,
 *  FreeBSD, NetBSD, OpenBSD, BSD/OS, Solaris, and QNX. The code contains
 *  support for other operating systems but those are not officially supported. 
 *  
 *  SDL is written in C, but works with C++ natively, and has bindings to
 *  several other languages, including Ada, C#, Eiffel, Erlang, Euphoria,
 *  Guile, Haskell, Java, Lisp, Lua, ML, Objective C, Pascal, Perl, PHP,
 *  Pike, Pliant, Python, Ruby, and Smalltalk.
 *  
 *  This library is distributed under GNU LGPL version 2, which can be
 *  found in the file  "COPYING".  This license allows you to use SDL
 *  freely in commercial programs as long as you link with the dynamic
 *  library.
 *  
 *  The best way to learn how to use SDL is to check out the header files in
 *  the "include" subdirectory and the programs in the "test" subdirectory.
 *  The header files and test programs are well commented and always up to date.
 *  More documentation is available in HTML format in "docs/index.html", and
 *  a documentation wiki is available online at:
 *  	http://www.libsdl.org/cgi/docwiki.cgi
 *  
 *  The test programs in the "test" subdirectory are in the public domain.
 *  
 *  Frequently asked questions are answered online:
 *  	http://www.libsdl.org/faq.php
 *  
 *  If you need help with the library, or just want to discuss SDL related
 *  issues, you can join the developers mailing list:
 *  	http://www.libsdl.org/mailing-list.php
 *  
 *  Enjoy!
 *  	Sam Lantinga				(slouken@libsdl.org)
 */

#ifndef _SDL_H
#define _SDL_H

#include "SDL_main.h"
#include "SDL_stdinc.h"
#include "SDL_endian.h"
#include "SDL_error.h"

#ifndef SDL_IGNORE
#define SDL_IGNORE 0
#endif

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* As of version 0.5, SDL is loaded dynamically into the application */

/**
 *  \name SDL_INIT_*
 *  
 *  These are the flags which may be passed to SDL_Init().  You should
 *  specify the subsystems which you will be using in your application.
 */
/*@{*/
#define SDL_INIT_JOYSTICK       0x00000200
#define SDL_INIT_HAPTIC         0x00001000
#define SDL_INIT_NOPARACHUTE    0x00100000      /**< Don't catch fatal signals */
#define SDL_INIT_EVERYTHING     0x0000FFFF
/*@}*/

/**
 *  This function loads the SDL dynamically linked library and initializes 
 *  the subsystems specified by \c flags (and those satisfying dependencies).
 *  Unless the ::SDL_INIT_NOPARACHUTE flag is set, it will install cleanup
 *  signal handlers for some commonly ignored fatal signals (like SIGSEGV).
 */
extern DECLSPEC int SDLCALL SDL_Init(Uint32 flags);

/**
 *  This function initializes specific SDL subsystems
 */
extern DECLSPEC int SDLCALL SDL_InitSubSystem(Uint32 flags);

/**
 *  This function cleans up specific SDL subsystems
 */
extern DECLSPEC void SDLCALL SDL_QuitSubSystem(Uint32 flags);

/**
 *  This function returns mask of the specified subsystems which have
 *  been initialized.
 *  
 *  If \c flags is 0, it returns a mask of all initialized subsystems.
 */
extern DECLSPEC Uint32 SDLCALL SDL_WasInit(Uint32 flags);

/**
 *  This function cleans up all initialized subsystems and unloads the
 *  dynamically linked library.  You should call it upon all exit conditions.
 */
extern DECLSPEC void SDLCALL SDL_Quit(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_H */
