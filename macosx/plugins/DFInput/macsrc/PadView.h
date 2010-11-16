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

#define PadView NetPcsxHIDInputPluginPadView

#import <Cocoa/Cocoa.h>
#import "ControllerList.h"

@class ControllerList;

@interface PadView : NSView
{
	IBOutlet NSTableView *tableView;
	IBOutlet NSPopUpButton *typeMenu;
	IBOutlet NSPopUpButton *deviceMenu;

	ControllerList *controller;
}
- (IBAction)setType:(id)sender;
- (IBAction)setDevice:(id)sender;

- (void)setController:(int)which;

@end
