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

#import "MappingCell.h"
#import "ControllerList.h"
#import "cfg.h"

@implementation MappingCell

- (id)initTextCell:(NSString *)aString {
	self = [super initTextCell:aString];
	[self setEditable:NO];
	return self;
}

- (void)selectWithFrame:(NSRect)aRect inView:(NSView *)controlView editor:(NSText *)textObj delegate:(id)anObject start:(int)selStart length:(int)selLength
{
	[super selectWithFrame:aRect inView:controlView editor:textObj delegate:anObject start:selStart length:selLength];

	int whichPad = [ControllerList currentController];
	NSTableView *tableView = (NSTableView *)[self controlView];
	int i, changed = 0, row;
	NSEvent *endEvent;
	NSPoint where = {0.0, 0.0};

	/* start a modal session */
	NSModalSession session = [NSApp beginModalSessionForWindow:[tableView window]];
	[NSApp runModalSession:session];

	/* delay for a little while to allow user to release the button pressed to activate the element */
	[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.15]];

	InitAxisPos(whichPad);

	/* wait for 10 seconds for user to press a key */
	for (i = 0; i < 10; i++) {
		[NSApp runModalSession:session];
		row = [tableView selectedRow];
		if (row < DKEY_TOTAL) {
			changed = ReadDKeyEvent(whichPad, [ControllerList getButtonOfRow:row]);
		} else {
			row -= DKEY_TOTAL;
			changed = ReadAnalogEvent(whichPad, row / 4, row % 4);
		}

		if (changed) break;
	}

	[NSApp endModalSession:session];

	/* move selection to the next list element */
	[self endEditing:textObj];
	if (changed == 1) {
		int nextRow = [tableView selectedRow] + 1;
		if (nextRow >= [tableView numberOfRows]) {
			[tableView deselectAll:self];
			return;
		}
		[tableView selectRow:nextRow byExtendingSelection:NO];

		/* discard any events we have received while waiting for the button press */
		endEvent = [NSEvent otherEventWithType:NSApplicationDefined location:where 
									modifierFlags:0 timestamp:(NSTimeInterval)0
									windowNumber:0 context:[NSGraphicsContext currentContext] subtype:0 data1:0 data2:0];
		[NSApp postEvent:endEvent atStart:NO];
		[NSApp discardEventsMatchingMask:NSAnyEventMask beforeEvent:endEvent];
	}
	[[tableView window] makeFirstResponder:tableView];
}

@end
