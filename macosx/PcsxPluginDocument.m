//
//  PcsxPluginDocument.m
//  Pcsx
//
//  Created by Gil Pedersen on Thu Jul 01 2004.
//  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
//

#import "PcsxPluginDocument.h"


@implementation PcsxPluginDocument

- (BOOL)showAddPluginSheet:(NSWindow *)window forName:(NSString *)name
// User has asked to see the custom display. Display it.
{
	if (!addPluginSheet)
		[NSBundle loadNibNamed:@"AddPluginSheet" owner:self];
	
	[pluginName setObjectValue:name];
	
	[NSApp beginSheet:addPluginSheet
			modalForWindow:window
			modalDelegate:nil
			didEndSelector:nil
			contextInfo:nil];
	[NSApp runModalForWindow:addPluginSheet];
	// Sheet is up here.
	[NSApp endSheet:addPluginSheet];
	[addPluginSheet orderOut:self];
	
	return moveOK;
}

- (IBAction)closeAddPluginSheet:(id)sender
{
	if ([[sender keyEquivalent] isEqualToString:@"\r"]) {
		moveOK = YES;
	} else {
		moveOK = NO;
	}
	[NSApp stopModal];
}

- (BOOL)loadDataRepresentation:(NSData *)docData ofType:(NSString *)docType
{
	//NSLog(@"loadDataRepresentation");
	return NO;
}

- (BOOL)loadFileWrapperRepresentation:(NSFileWrapper *)wrapper ofType:(NSString *)docType
{
	if ([self showAddPluginSheet:nil forName:[wrapper filename]]) {
		NSString *dst = [NSString stringWithFormat:@"%@/%@", 
				[[NSBundle mainBundle] builtInPlugInsPath],
				[wrapper filename]];
		
		if ([wrapper writeToFile:dst atomically:NO updateFilenames:NO]) {
			[[NSWorkspace sharedWorkspace] noteFileSystemChanged:[[NSBundle mainBundle] builtInPlugInsPath]];
			NSRunInformationalAlertPanel(NSLocalizedString(@"Installation Succesfull", nil),
					NSLocalizedString(@"The installation of the specified plugin was succesfull. In order to use it, please restart the application.", nil), 
					nil, nil, nil);
		} else {
			NSRunAlertPanel(NSLocalizedString(@"Installation Failed!", nil),
					NSLocalizedString(@"The installation of the specified plugin failed. Please try again, or make a manual install.", nil), 
					nil, nil, nil);
		}
	}
	
	// Tell the NSDocument that we can't handle the file, since we are already done with it
	return NO;
}

- (id)openDocumentWithContentsOfFile:(NSString *)fileName display:(BOOL)flag
{
    
    return nil;
}

- (NSString *)windowNibName {
    // Implement this to return a nib to load OR implement -makeWindowControllers to manually create your controllers.
    return @"PcsxPluginDocument";
}

- (NSData *)dataRepresentationOfType:(NSString *)type {
    // Implement to provide a persistent data representation of your document OR remove this and implement the file-wrapper or file path based save methods.
    return nil;
}
/*
- (BOOL)loadDataRepresentation:(NSData *)data ofType:(NSString *)type {
    // Implement to load a persistent data representation of your document OR remove this and implement the file-wrapper or file path based load methods.
    return YES;
}*/

@end
