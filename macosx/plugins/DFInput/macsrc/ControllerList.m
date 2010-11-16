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

#import "ControllerList.h"
#include "pad.h"
#include "cfg.h"

static int currentController;

@implementation ControllerList

- (id)initWithConfig
{
	if (!(self = [super init])) return nil;
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

/* sets current controller data returned by data source */
+ (void)setCurrentController:(int)which
{
	currentController = which;
}

+ (int)currentController
{
	return currentController;
}

/* NSDataSource */
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return DKEY_TOTAL + (g.cfg.PadDef[currentController].Type == PSE_PAD_TYPE_ANALOGPAD ? 8 : -2);
}

static const NSString *LabelText[DKEY_TOTAL + 8] = {
	@"D-Pad Up",
	@"D-Pad Down",
	@"D-Pad Left",
	@"D-Pad Right",
	@"Cross",
	@"Circle",
	@"Square",
	@"Triangle",
	@"L1",
	@"R1",
	@"L2",
	@"R2",
	@"Select",
	@"Start",
	@"L3",
	@"R3",
	@"L-Stick Right",
	@"L-Stick Left",
	@"L-Stick Down",
	@"L-Stick Up",
	@"R-Stick Right",
	@"R-Stick Left",
	@"R-Stick Down",
	@"R-Stick Up"
};

static const int DPad[DKEY_TOTAL] = {
	DKEY_UP,
	DKEY_DOWN,
	DKEY_LEFT,
	DKEY_RIGHT,
	DKEY_CROSS,
	DKEY_CIRCLE,
	DKEY_SQUARE,
	DKEY_TRIANGLE,
	DKEY_L1,
	DKEY_R1,
	DKEY_L2,
	DKEY_R2,
	DKEY_SELECT,
	DKEY_START,
	DKEY_L3,
	DKEY_R3
};

+ (int)getButtonOfRow:(int)row
{
	return DPad[row];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn 
		row:(int)rowIndex
{
	char buf[256];

	if ([((NSString *)[aTableColumn identifier]) isEqualToString:@"key"])
		return LabelText[rowIndex];
	else {
		// actual keys
		if (rowIndex < DKEY_TOTAL) {
			GetKeyDescription(buf, currentController, DPad[rowIndex]);
		} else {
			rowIndex -= DKEY_TOTAL;
			GetAnalogDescription(buf, currentController, rowIndex / 4, rowIndex % 4);
		}

		return [NSString stringWithUTF8String:buf];
	}
}

- (void)deleteRow:(int)which
{
	if (which < DKEY_TOTAL) {
		g.cfg.PadDef[currentController].KeyDef[DPad[which]].Key = 0;
		g.cfg.PadDef[currentController].KeyDef[DPad[which]].JoyEvType = NONE;
		g.cfg.PadDef[currentController].KeyDef[DPad[which]].J.d = 0;
	} else {
		which -= DKEY_TOTAL;
		g.cfg.PadDef[currentController].AnalogDef[which / 4][which % 4].Key = 0;
		g.cfg.PadDef[currentController].AnalogDef[which / 4][which % 4].JoyEvType = NONE;
		g.cfg.PadDef[currentController].AnalogDef[which / 4][which % 4].J.d = 0;
	}
}

@end
