/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: HIDInput by Gil Pedersen.
 * Copyright (c) 2004, Gil Pedersen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */

#import <Cocoa/Cocoa.h>
#import "PadController.h"
#include "pad.h"

static NSWindow *padWindow;
static PadController *padController;

#define APP_ID @"net.pcsx.DFInputPlugin"

void DoAbout() {
	// Get parent application instance
	NSApplication *app = [NSApplication sharedApplication];
	NSBundle *bundle = [NSBundle bundleWithIdentifier:APP_ID];

	// Get Credits.rtf
	NSString *path = [bundle pathForResource:@"Credits" ofType:@"rtf"];
	NSAttributedString *credits;
	if (path) {
		credits = [[[NSAttributedString alloc] initWithPath: path
				documentAttributes:NULL] autorelease];
	} else {
		credits = [[[NSAttributedString alloc] initWithString:@""] autorelease];
	}

	// Get Application Icon
	NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[bundle bundlePath]];
	NSSize size = NSMakeSize(64, 64);
	[icon setSize:size];

	[app orderFrontStandardAboutPanelWithOptions:[NSDictionary dictionaryWithObjectsAndKeys:
		[bundle objectForInfoDictionaryKey:@"CFBundleName"], @"ApplicationName",
		icon, @"ApplicationIcon",
		[bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"], @"ApplicationVersion",
		[bundle objectForInfoDictionaryKey:@"CFBundleVersion"], @"Version",
		[bundle objectForInfoDictionaryKey:@"NSHumanReadableCopyright"], @"Copyright",
		credits, @"Credits", nil]];
}

long DoConfiguration() {
	SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE);
	LoadPADConfig();

	if (padWindow == nil) {
		if (padController == nil) {
			padController = [[PadController alloc] initWithWindowNibName:@"NetPcsxHIDInputPluginMain"];
		}
		padWindow = [padController window];
	}

	[padWindow center];
	[padWindow makeKeyAndOrderFront:nil];

	return 0;
}

@implementation PadController

- (IBAction)cancel:(id)sender
{
	SDL_Quit();
	[self close];
}

- (IBAction)ok:(id)sender
{
	SavePADConfig();
	SDL_Quit();
	[self close];
}

- (void)awakeFromNib
{
	[[NSNotificationCenter defaultCenter] addObserver:self
    selector:@selector(windowWillClose:)
    name:NSWindowWillCloseNotification object:[self window]];

	[controllerView1 addSubview: controllerView];
	[controllerView setController:0];
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
	PadView *newView = nil;
	if ([[tabViewItem identifier] isEqualToString:@"pad1"])
		newView = controllerView1;
	else if ([[tabViewItem identifier] isEqualToString:@"pad2"])
		newView = controllerView2;

	if (nil != newView) {
		[controllerView removeFromSuperviewWithoutNeedingDisplay];
		[newView addSubview: controllerView];
		[controllerView setFrame:[newView frame]];
		[controllerView setController:[newView isEqual:controllerView1] ? 0 : 1];
	}
}

- (void)windowBecameKey:(NSNotification *)notification
{
	if ([[controllerView1 subviews] count] > 0)
		[controllerView setController:0];
	else if ([[controllerView2 subviews] count] > 0)
		[controllerView setController:1];

	[[NSNotificationCenter defaultCenter] removeObserver:self
    name:NSWindowDidBecomeKeyNotification object:[self window]];
}

- (void)windowWillClose:(NSNotification *)aNotification
{
	if ([aNotification object] == [self window]) {
		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(windowBecameKey:)
		 name:NSWindowDidBecomeKeyNotification object:[self window]];
	}
}

@end