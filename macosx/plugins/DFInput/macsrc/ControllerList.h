/*
 * Copyright (c) 2010, Wei Mingzhi <whistler@openoffice.org>.
 * All Rights Reserved.
 *
 * Based on HIDInput by Gil Pedersen.
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

#define ControllerList NetPcsxHIDInputPluginControllerList

#import <Foundation/Foundation.h>
#import <AppKit/NSTableView.h>
#include "cfg.h"

@class KeyConfig;

@interface ControllerList : NSObject {
}

- (id)initWithConfig;

+ (void)setCurrentController:(int)which;
+ (int)currentController;
+ (int)getButtonOfRow:(int)row;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (void)deleteRow:(int)which;

@end
