/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on: Cdrom for Psemu Pro like Emulators
 * By: linuzappz <linuzappz@hotmail.com>
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

#import "PluginConfigController.h"
#include "cdr.h"

#define APP_ID @"net.pcsx.DFCdrom"
#define PrefsKey APP_ID @" Settings"

static PluginConfigController *windowController;

void AboutDlgProc()
{
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
			credits, @"Credits",
			nil]];
}

void ConfDlgProc()
{
	NSWindow *window;

	if (windowController == nil) {
		windowController = [[PluginConfigController alloc] initWithWindowNibName:@"DFCdromPluginConfig"];
	}
	window = [windowController window];

	[windowController loadValues];

	[window center];
	[window makeKeyAndOrderFront:nil];
}

void ReadConfig()
{
	NSDictionary *keyValues;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
		[[NSMutableDictionary alloc] initWithObjectsAndKeys:
			[NSNumber numberWithBool:YES], @"Threaded",
			[NSNumber numberWithInt:64], @"Cache Size",
			[NSNumber numberWithInt:0], @"Speed",
			nil], PrefsKey, nil]];

	keyValues = [defaults dictionaryForKey:PrefsKey];

	ReadMode = ([[keyValues objectForKey:@"Threaded"] boolValue] ? THREADED : NORMAL);
	CacheSize = [[keyValues objectForKey:@"Cache Size"] intValue];
	CdrSpeed = [[keyValues objectForKey:@"Speed"] intValue];
}

@implementation PluginConfigController

- (IBAction)cancel:(id)sender
{
	[self close];
}

- (IBAction)ok:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

	NSMutableDictionary *writeDic = [NSMutableDictionary dictionaryWithDictionary:keyValues];

	[writeDic setObject:[NSNumber numberWithInt:[Cached intValue]] forKey:@"Threaded"];
	[writeDic setObject:[NSNumber numberWithInt:[CacheSize intValue]] forKey:@"Cache Size"];

	switch ([CdSpeed indexOfSelectedItem]) {
		case 1: [writeDic setObject:[NSNumber numberWithInt:1] forKey:@"Speed"]; break;
		case 2: [writeDic setObject:[NSNumber numberWithInt:2] forKey:@"Speed"]; break;
		case 3: [writeDic setObject:[NSNumber numberWithInt:4] forKey:@"Speed"]; break;
		case 4: [writeDic setObject:[NSNumber numberWithInt:8] forKey:@"Speed"]; break;
		case 5: [writeDic setObject:[NSNumber numberWithInt:16] forKey:@"Speed"]; break;
		case 6: [writeDic setObject:[NSNumber numberWithInt:32] forKey:@"Speed"]; break;
		default: [writeDic setObject:[NSNumber numberWithInt:0] forKey:@"Speed"]; break;
	}

	// write to defaults
	[defaults setObject:writeDic forKey:PrefsKey];
	[defaults synchronize];

	// and set global values accordingly
	ReadConfig();

	[self close];
}

- (void)loadValues
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

	ReadConfig();

	// load from preferences
	[keyValues release];
	keyValues = [[defaults dictionaryForKey:PrefsKey] retain];

	[Cached setIntValue:[[keyValues objectForKey:@"Threaded"] intValue]];
	[CacheSize setIntValue:[[keyValues objectForKey:@"Cache Size"] intValue]];

	switch ([[keyValues objectForKey:@"Speed"] intValue]) {
		case 1: [CdSpeed selectItemAtIndex:1]; break;
		case 2: [CdSpeed selectItemAtIndex:2]; break;
		case 4: [CdSpeed selectItemAtIndex:3]; break;
		case 8: [CdSpeed selectItemAtIndex:4]; break;
		case 16: [CdSpeed selectItemAtIndex:5]; break;
		case 32: [CdSpeed selectItemAtIndex:6]; break;
		default: [CdSpeed selectItemAtIndex:0]; break;
	}
}

- (void)awakeFromNib
{
}

@end
