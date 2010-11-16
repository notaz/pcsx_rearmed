#import <Cocoa/Cocoa.h>
#import "PcsxController.h"
#import "ConfigurationController.h"
#import "EmuThread.h"
#include "psxcommon.h"
#include "plugins.h"
#include "misc.h"
#include "ExtendedKeys.h"

NSDictionary *prefStringKeys;
NSDictionary *prefByteKeys;
NSMutableArray *biosList;
NSString *saveStatePath;

@implementation PcsxController

- (IBAction)ejectCD:(id)sender
{
	NSMutableString *deviceName;
	NSTask *ejectTask;
	NSRange rdiskRange;

	BOOL wasPaused = [EmuThread pauseSafe];

	/* close connection to current cd */
	if ([EmuThread active])
		CDR_close();

	// switch to another ISO if using internal image reader, otherwise eject the CD
	if (UsingIso()) {
		NSOpenPanel* openDlg = [NSOpenPanel openPanel];

		[openDlg setCanChooseFiles:YES];
		[openDlg setCanChooseDirectories:NO];

		if ([openDlg runModal] == NSOKButton) {
			NSArray* files = [openDlg filenames];
			SetCdOpenCaseTime(time(NULL) + 2);
			SetIsoFile((const char *)[[files objectAtIndex:0] fileSystemRepresentation]);
		}
	} else {
		if (CDR_getDriveLetter() != nil) {
			deviceName = [NSMutableString stringWithCString:CDR_getDriveLetter()];

			// delete the 'r' in 'rdisk'
			rdiskRange = [deviceName rangeOfString:@"rdisk"];
			if (rdiskRange.length != 0) {
				rdiskRange.length = 1;
				[deviceName deleteCharactersInRange:rdiskRange];
			}
			// execute hdiutil to eject the device
			ejectTask = [NSTask launchedTaskWithLaunchPath:@"/usr/bin/hdiutil" arguments:[NSArray arrayWithObjects:@"eject", deviceName, nil]];
			[ejectTask waitUntilExit];
		}
	}

	/* and open new cd */
	if ([EmuThread active])
		CDR_open();

	if (!wasPaused) {
		[EmuThread resume];
	}
}

- (IBAction)pause:(id)sender
{
    if ([EmuThread isPaused]) {
        //[sender setState:NSOffState];
        [EmuThread resume];
    }
    else {
        //[sender setState:NSOnState];
        [EmuThread pause];
    }
}

- (IBAction)preferences:(id)sender
{
	/* load the nib if it hasn't yet */
	if (preferenceWindow == nil) {
		if (preferencesController == nil) {
			preferencesController = [[ConfigurationController alloc] initWithWindowNibName:@"Configuration"];
		}
		preferenceWindow = [preferencesController window];
	}

	/* show the window */
	[preferenceWindow makeKeyAndOrderFront:self];
	[preferencesController showWindow:self];
}

- (IBAction)reset:(id)sender
{
    [EmuThread reset];
}

- (IBAction)runCD:(id)sender
{
	SetIsoFile(NULL);
	[EmuThread run];
}

- (IBAction)runIso:(id)sender
{
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];

	[openDlg setCanChooseFiles:YES];
	[openDlg setCanChooseDirectories:NO];

	if ([openDlg runModalForDirectory:nil file:nil] == NSOKButton) {
		NSArray* files = [openDlg filenames];
		SetIsoFile((const char *)[[files objectAtIndex:0] fileSystemRepresentation]);
		[EmuThread run];
    }
}

- (IBAction)runBios:(id)sender
{
	SetIsoFile(NULL);
	[EmuThread runBios];
}

- (IBAction)freeze:(id)sender
{
	int num = [sender tag];
	NSString *path = [NSString stringWithFormat:@"%@/%s-%3.3d.pcsxstate", saveStatePath, CdromId, num];

	[EmuThread freezeAt:path which:num-1];
}

- (IBAction)defrost:(id)sender
{
	NSString *path = [NSString stringWithFormat:@"%@/%s-%3.3d.pcsxstate", saveStatePath, CdromId, [sender tag]];
	[EmuThread defrostAt:path];
}

- (IBAction)fullscreen:(id)sender
{
	GPU_keypressed(GPU_FULLSCREEN_KEY);
}

- (BOOL)validateMenuItem:(id <NSMenuItem>)menuItem
{
	if ([menuItem action] == @selector(pause:)) {
		[menuItem setState:([EmuThread isPaused] ? NSOnState : NSOffState)];
	}

	if ([menuItem action] == @selector(pause:) || [menuItem action] == @selector(fullscreen:))
		return [EmuThread active];

	if ([menuItem action] == @selector(reset:) || [menuItem action] == @selector(ejectCD:) ||
		 [menuItem action] == @selector(freeze:))
		return [EmuThread active] && ![EmuThread isRunBios];

	if ([menuItem action] == @selector(runCD:) || [menuItem action] == @selector(runIso:) ||
		 [menuItem action] == @selector(runBios:)) {
		if (preferenceWindow != nil)
			if ([preferenceWindow isVisible])
				return NO;

		if ([menuItem action] == @selector(runBios:) && strcmp(Config.Bios, "HLE") == 0)
			return NO;

		return ![EmuThread active];
	}

	if ([menuItem action] == @selector(defrost:)) {
		if (![EmuThread active] || [EmuThread isRunBios])
			return NO;

		NSString *path = [NSString stringWithFormat:@"%@/%s-%3.3d.pcsxstate", saveStatePath, CdromId, [menuItem tag]];
		return (CheckState((char *)[path fileSystemRepresentation]) == 0);
	}

	if ([menuItem action] == @selector(preferences:))
		return ![EmuThread active];

	return YES;
}

- (void)applicationWillResignActive:(NSNotification *)aNotification
{
	wasPausedBeforeBGSwitch = [EmuThread isPaused];

	if (sleepInBackground) {
		 [EmuThread pause];
	}
}

- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
	if (sleepInBackground && !wasPausedBeforeBGSwitch) {
		[EmuThread resume];
	}
}

- (void)awakeFromNib
{
	pluginList = [[PluginList alloc] init];
	if (![pluginList configured] /*!Config.Gpu[0] || !Config.Spu[0] || !Config.Pad1[0] || !Config.Cdr[0]*/) {
		// configure plugins
		[self preferences:nil];

		NSRunCriticalAlertPanel(NSLocalizedString(@"Missing plugins!", nil),
				NSLocalizedString(@"Pcsx is missing one or more critical plugins. You will need to install these in order to play games.", nil), 
				nil, nil, nil);
	}

	if (![PcsxController biosAvailable]) {
		NSRunInformationalAlertPanel(NSLocalizedString(@"Missing BIOS!", nil),
				NSLocalizedString(@"Pcsx wasn't able to locate any Playstation BIOS ROM files. This means that it will run in BIOS simulation mode which is less stable and compatible than using a real Playstation BIOS.\n"
										@"If you have a BIOS available, please copy it to\n~/Library/Application Support/Pcsx/Bios/", nil), 
				nil, nil, nil);
	}

	sleepInBackground = YES;
}

- (void)dealloc
{
	[pluginList release];
	[super dealloc];
}

+ (void)setConfigFromDefaults
{
	NSEnumerator *enumerator;
	const char *str;
	NSString *key;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

	/*
	enumerator = [prefStringKeys keyEnumerator];
	while ((key = [enumerator nextObject])) {
		str = [[defaults stringForKey:key] fileSystemRepresentation];
		char *dst = (char *)[[prefStringKeys objectForKey:key] pointerValue];
		if (str != nil && dst != nil) strncpy(dst, str, 255);
	}*/

	enumerator = [prefByteKeys keyEnumerator];
	while ((key = [enumerator nextObject])) {
		u8 *dst = (u8 *)[[prefByteKeys objectForKey:key] pointerValue];
		if (dst != nil) *dst = [defaults integerForKey:key];
	}

	// special cases
	//str = [[defaults stringForKey:@"PluginPAD"] fileSystemRepresentation];
	//if (str != nil) strncpy(Config.Pad2, str, 255);

	str = [[defaults stringForKey:@"Bios"] fileSystemRepresentation];
	if (str) {
		NSString *path = [defaults stringForKey:@"Bios"];
		int index = [biosList indexOfObject:path];

		if (-1 == index) {
			[biosList insertObject:path atIndex:0];
		} else if (0 < index) {
			[biosList exchangeObjectAtIndex:index withObjectAtIndex:0];
		}
	}

	str = [[defaults stringForKey:@"Mcd1"] fileSystemRepresentation];
	if (str) strncpy(Config.Mcd1, str, MAXPATHLEN);

	str = [[defaults stringForKey:@"Mcd2"] fileSystemRepresentation];
	if (str) strncpy(Config.Mcd2, str, MAXPATHLEN);

	if ([defaults boolForKey:@"UseHLE"] || 0 == [biosList count]) {
		strcpy(Config.Bios, "HLE");
	} else {
		str = [(NSString *)[biosList objectAtIndex:0] fileSystemRepresentation];
		if (str != nil) strncpy(Config.Bios, str, MAXPATHLEN);
		else strcpy(Config.Bios, "HLE");
	}

	// FIXME: hack
	strcpy(Config.Net, "Disabled");
}

+ (void)setDefaultFromConfig:(NSString *)defaultKey
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

	char *str = (char *)[[prefStringKeys objectForKey:defaultKey] pointerValue];
	if (str) {
		[defaults setObject:[NSString stringWithCString:str] forKey:defaultKey];
		return;
	}

	u8 *val = (u8 *)[[prefByteKeys objectForKey:defaultKey] pointerValue];
	if (val) {
		[defaults setInteger:*val forKey:defaultKey];
		return;
	}
}

+ (BOOL)biosAvailable
{
	return ([biosList count] > 0);
}

// called when class is initialized
+ (void)initialize
{
	NSString *path;
	const char *str;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
		@"Disabled", @"PluginNET",
		[NSNumber numberWithInt:1], @"NoDynarec",
		[NSNumber numberWithInt:1], @"AutoDetectVideoType",
		[NSNumber numberWithInt:0], @"UseHLE",
		nil];

	[defaults registerDefaults:appDefaults];

	prefStringKeys = [[NSDictionary alloc] initWithObjectsAndKeys:
		[NSValue valueWithPointer:Config.Gpu], @"PluginGPU",
		[NSValue valueWithPointer:Config.Spu], @"PluginSPU",
		[NSValue valueWithPointer:Config.Pad1], @"PluginPAD",
		[NSValue valueWithPointer:Config.Cdr], @"PluginCDR",
		[NSValue valueWithPointer:Config.Net], @"PluginNET",
		[NSValue valueWithPointer:Config.Mcd1], @"Mcd1",
		[NSValue valueWithPointer:Config.Mcd2], @"Mcd2",
		nil];

	prefByteKeys = [[NSDictionary alloc] initWithObjectsAndKeys:
		[NSValue valueWithPointer:&Config.Xa], @"NoXaAudio",
		[NSValue valueWithPointer:&Config.Sio], @"SioIrqAlways",
		[NSValue valueWithPointer:&Config.Mdec], @"BlackAndWhiteMDECVideo",
		[NSValue valueWithPointer:&Config.PsxAuto], @"AutoDetectVideoType",
		[NSValue valueWithPointer:&Config.PsxType], @"VideoTypePAL",
		[NSValue valueWithPointer:&Config.Cdda], @"NoCDAudio",
		[NSValue valueWithPointer:&Config.Cpu], @"NoDynarec",
		[NSValue valueWithPointer:&Config.PsxOut], @"ConsoleOutput",
		[NSValue valueWithPointer:&Config.SpuIrq], @"SpuIrqAlways",
		[NSValue valueWithPointer:&Config.RCntFix], @"RootCounterFix",
		[NSValue valueWithPointer:&Config.VSyncWA], @"VideoSyncWAFix",
		nil];

	// setup application support paths
	NSArray *libPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
	if ([libPaths count] > 0) {
		NSString *path;
		BOOL dir;

		// create them if needed
		NSFileManager *dfm = [NSFileManager defaultManager];
		NSString *supportPath = [NSString stringWithFormat:@"%@/Application Support", [libPaths objectAtIndex:0]];
		if (![dfm fileExistsAtPath:supportPath isDirectory:&dir])
			[dfm createDirectoryAtPath:supportPath attributes:nil];

		path = [NSString stringWithFormat:@"%@/Pcsx", supportPath];
		if (![dfm fileExistsAtPath:path isDirectory:&dir])
			[dfm createDirectoryAtPath:path attributes:nil];

		path = [NSString stringWithFormat:@"%@/Pcsx/Bios", supportPath];
		if (![dfm fileExistsAtPath:path isDirectory:&dir])
			[dfm createDirectoryAtPath:path attributes:nil];

		path = [NSString stringWithFormat:@"%@/Pcsx/Memory Cards", supportPath];
		if (![dfm fileExistsAtPath:path isDirectory:&dir])
			[dfm createDirectoryAtPath:path attributes:nil];

		path = [NSString stringWithFormat:@"%@/Pcsx/Patches", supportPath];
		if (![dfm fileExistsAtPath:path isDirectory:&dir])
			[dfm createDirectoryAtPath:path attributes:nil];

		saveStatePath = [[NSString stringWithFormat:@"%@/Pcsx/Save States", supportPath] retain];
		if (![dfm fileExistsAtPath:saveStatePath isDirectory:&dir])
			[dfm createDirectoryAtPath:saveStatePath attributes:nil];

		path = [NSString stringWithFormat:@"%@/Pcsx/Memory Cards/Mcd001.mcr", supportPath];
		str = [path fileSystemRepresentation];
		if (str != nil) strncpy(Config.Mcd1, str, 255);

		path = [NSString stringWithFormat:@"%@/Pcsx/Memory Cards/Mcd002.mcr", supportPath];
		str = [path fileSystemRepresentation];
		if (str != nil) strncpy(Config.Mcd2, str, 255);

		path = [NSString stringWithFormat:@"%@/Pcsx/Bios/", supportPath];
		str = [path fileSystemRepresentation];
		if (str != nil) strncpy(Config.BiosDir, str, 255);

		path = [NSString stringWithFormat:@"%@/Pcsx/Patches/", supportPath];
		str = [path fileSystemRepresentation];
		if (str != nil) strncpy(Config.PatchesDir, str, 255);
	} else {
		strcpy(Config.BiosDir, "Bios/");
		strcpy(Config.PatchesDir, "Patches/");

		saveStatePath = @"sstates";
		[saveStatePath retain];
	}

	// set plugin path
	path = [[[NSBundle mainBundle] builtInPlugInsPath] stringByAppendingString:@"/"];
	str = [path fileSystemRepresentation];
	if (str != nil) strncpy(Config.PluginsDir, str, 255);

	// locate a bios
	biosList = [[NSMutableArray alloc] init];
	NSFileManager *manager = [NSFileManager defaultManager];
	NSArray *bioses = [manager directoryContentsAtPath:[NSString stringWithCString:Config.BiosDir]];
	if (bioses) {
		int i;
		for (i = 0; i < [bioses count]; i++) {
			NSString *file = [bioses objectAtIndex:i];
			NSDictionary *attrib = [manager fileAttributesAtPath:[NSString stringWithFormat:@"%s%@", Config.BiosDir, file] traverseLink:YES];

			if ([[attrib fileType] isEqualToString:NSFileTypeRegular]) {
				unsigned long long size = [attrib fileSize];
				if (([attrib fileSize] % (256 * 1024)) == 0 && size > 0) {
					[biosList addObject:file];
				}
			}
		}
	}

	[PcsxController setConfigFromDefaults];
}


@end
