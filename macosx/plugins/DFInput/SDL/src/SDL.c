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

#include "SDL_config.h"

/* Initialization code for SDL */

#include "SDL.h"
#include "haptic/SDL_haptic_c.h"
#include "joystick/SDL_joystick_c.h"

/* The initialized subsystems */
static Uint32 SDL_initialized = 0;

int
SDL_InitSubSystem(Uint32 flags)
{
    /* Initialize the joystick subsystem */
    if ((flags & SDL_INIT_JOYSTICK) && !(SDL_initialized & SDL_INIT_JOYSTICK)) {
        if (SDL_JoystickInit() < 0) {
            return (-1);
        }
        SDL_initialized |= SDL_INIT_JOYSTICK;
    }

    /* Initialize the haptic subsystem */
    if ((flags & SDL_INIT_HAPTIC) && !(SDL_initialized & SDL_INIT_HAPTIC)) {
        if (SDL_HapticInit() < 0) {
            return (-1);
        }
        SDL_initialized |= SDL_INIT_HAPTIC;
    }

    return (0);
}

int
SDL_Init(Uint32 flags)
{
    /* Clear the error message */
    SDL_ClearError();

    /* Initialize the desired subsystems */
    if (SDL_InitSubSystem(flags) < 0) {
        return (-1);
    }

    return (0);
}

void
SDL_QuitSubSystem(Uint32 flags)
{
    /* Shut down requested initialized subsystems */
    if ((flags & SDL_initialized & SDL_INIT_JOYSTICK)) {
        SDL_JoystickQuit();
        SDL_initialized &= ~SDL_INIT_JOYSTICK;
    }

    if ((flags & SDL_initialized & SDL_INIT_HAPTIC)) {
        SDL_HapticQuit();
        SDL_initialized &= ~SDL_INIT_HAPTIC;
    }
}

Uint32
SDL_WasInit(Uint32 flags)
{
    if (!flags) {
        flags = SDL_INIT_EVERYTHING;
    }
    return (SDL_initialized & flags);
}

void
SDL_Quit(void)
{
    /* Quit all subsystems */
    SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
}
