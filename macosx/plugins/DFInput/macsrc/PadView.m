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

#import "PadView.h"
#include "pad.h"

@implementation PadView

- (id)initWithFrame:(NSRect)frameRect
{
	if ((self = [super initWithFrame:frameRect]) != nil) {
		controller = [[ControllerList alloc] initWithConfig];
		[self setController:0];
	}
	return self;
}

- (void)dealloc
{
	[controller release];
	[super dealloc];
}

- (void)drawRect:(NSRect)rect
{
}

- (IBAction)setType:(id)sender
{
	g.cfg.PadDef[[ControllerList currentController]].Type =
		([sender indexOfSelectedItem] > 0 ? PSE_PAD_TYPE_ANALOGPAD : PSE_PAD_TYPE_STANDARD);

	[tableView reloadData];
}

- (IBAction)setDevice:(id)sender
{
	g.cfg.PadDef[[ControllerList currentController]].DevNum = (int)[sender indexOfSelectedItem] - 1;
}

- (void)setController:(int)which
{
	int i;

	[ControllerList setCurrentController:which];
	[tableView setDataSource:controller];

	[deviceMenu removeAllItems];
	[deviceMenu addItemWithTitle:@"(Keyboard only)"];

	for (i = 0; i < SDL_NumJoysticks(); i++) {
		[deviceMenu addItemWithTitle:[NSString stringWithUTF8String:SDL_JoystickName(i)]];
	}

	if (g.cfg.PadDef[which].DevNum >= SDL_NumJoysticks()) {
		g.cfg.PadDef[which].DevNum = -1;
	}

	[deviceMenu selectItemAtIndex:g.cfg.PadDef[which].DevNum + 1];
	[typeMenu selectItemAtIndex:(g.cfg.PadDef[which].Type == PSE_PAD_TYPE_ANALOGPAD ? 1 : 0)];

	[tableView reloadData];
}


- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
	return false;
}

/* handles key events on the pad list */
- (void)keyDown:(NSEvent *)theEvent
{
	int key = [theEvent keyCode];

	if ([[theEvent window] firstResponder] == tableView) {
		if (key == 51 || key == 117) {
			// delete keys - remove the mappings for the selected item
			[controller deleteRow:[tableView selectedRow]];
			[tableView reloadData];
			return;
		} else if (key == 36) {
			// return key - configure the selected item
			[tableView editColumn:[tableView columnWithIdentifier:@"button"] row:[tableView selectedRow] withEvent:nil select:YES];
			return;
		}
	}

	[super keyDown:theEvent];
}

@end
