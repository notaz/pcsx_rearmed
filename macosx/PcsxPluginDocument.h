//
//  PcsxPluginDocument.h
//  Pcsx
//
//  Created by Gil Pedersen on Thu Jul 01 2004.
//  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>


@interface PcsxPluginDocument : NSDocument {
	IBOutlet NSWindow *addPluginSheet;
	IBOutlet NSTextField *pluginName;
	
	BOOL moveOK;
}
- (IBAction)closeAddPluginSheet:(id)sender;

@end
