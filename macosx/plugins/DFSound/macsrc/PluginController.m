#import "PluginController.h"
#include "stdafx.h"
#include "externals.h"

#define APP_ID @"net.sf.peops.SPUPlugin"
#define PrefsKey APP_ID @" Settings"

static PluginController *pluginController;
char * pConfigFile=NULL;

void DoAbout()
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


long DoConfiguration()
{
	NSWindow *window;
	
	if (pluginController == nil) {
		pluginController = [[PluginController alloc] initWithWindowNibName:@"NetSfPeopsSpuPluginMain"];
	}
	window = [pluginController window];
	
	/* load values */
	[pluginController loadValues];
	
	[window center];
	[window makeKeyAndOrderFront:nil];
	
	return 0;
}

void ReadConfig(void)
{
	NSDictionary *keyValues;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
			[[NSMutableDictionary alloc] initWithObjectsAndKeys:
					[NSNumber numberWithBool:YES], @"High Compatibility Mode",
					[NSNumber numberWithBool:YES], @"SPU IRQ Wait",
					[NSNumber numberWithBool:NO], @"XA Pitch",
					[NSNumber numberWithInt:0], @"Interpolation Quality",
					[NSNumber numberWithInt:1], @"Reverb Quality",
					nil], PrefsKey,
			nil]];

	keyValues = [defaults dictionaryForKey:PrefsKey];

	iUseTimer = [[keyValues objectForKey:@"High Compatibility Mode"] boolValue] ? 2 : 0;
	iSPUIRQWait = [[keyValues objectForKey:@"SPU IRQ Wait"] boolValue];
	iDisStereo = [[keyValues objectForKey:@"Mono Sound Output"] boolValue];
	iXAPitch = [[keyValues objectForKey:@"XA Pitch"] boolValue];

	iUseInterpolation = [[keyValues objectForKey:@"Interpolation Quality"] intValue];
	iUseReverb = [[keyValues objectForKey:@"Reverb Quality"] intValue];

	iVolume=1; 
}

@implementation PluginController

- (IBAction)cancel:(id)sender
{
	[self close];
}

- (IBAction)ok:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	NSMutableDictionary *writeDic = [NSMutableDictionary dictionaryWithDictionary:keyValues];
	[writeDic setObject:[NSNumber numberWithInt:[hiCompBox intValue]] forKey:@"High Compatibility Mode"];
	[writeDic setObject:[NSNumber numberWithInt:[irqWaitBox intValue]] forKey:@"SPU IRQ Wait"];
	[writeDic setObject:[NSNumber numberWithInt:[monoSoundBox intValue]] forKey:@"Mono Sound Output"];
	[writeDic setObject:[NSNumber numberWithInt:[xaSpeedBox intValue]] forKey:@"XA Pitch"];

	[writeDic setObject:[NSNumber numberWithInt:[interpolValue intValue]] forKey:@"Interpolation Quality"];
	[writeDic setObject:[NSNumber numberWithInt:[reverbValue intValue]] forKey:@"Reverb Quality"];
	
	// write to defaults
	[defaults setObject:writeDic forKey:PrefsKey];
	[defaults synchronize];
	
	// and set global values accordingly
	ReadConfig();
	
	[self close];
}

- (IBAction)reset:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults removeObjectForKey:PrefsKey];
	[self loadValues];
}

- (void)loadValues
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	ReadConfig();
	
	/* load from preferences */
	[keyValues release];
	keyValues = [[defaults dictionaryForKey:PrefsKey] retain];
	
	[hiCompBox setIntValue:[[keyValues objectForKey:@"High Compatibility Mode"] intValue]];
	[irqWaitBox setIntValue:[[keyValues objectForKey:@"SPU IRQ Wait"] intValue]];
	[monoSoundBox setIntValue:[[keyValues objectForKey:@"Mono Sound Output"] intValue]];
	[xaSpeedBox setIntValue:[[keyValues objectForKey:@"XA Pitch"] intValue]];

	[interpolValue setIntValue:[[keyValues objectForKey:@"Interpolation Quality"] intValue]];
	[reverbValue setIntValue:[[keyValues objectForKey:@"Reverb Quality"] intValue]];
}

- (void)awakeFromNib
{
	[interpolValue setStrings:[NSArray arrayWithObjects:
		@"(No Interpolation)",
		@"(Simple Interpolation)",
		@"(Gaussian Interpolation)",
		@"(Cubic Interpolation)",
		nil]];

	[reverbValue setStrings:[NSArray arrayWithObjects:
		@"(No Reverb)",
		@"(Simple Reverb)",
		@"(PSX Reverb)",
		nil]];
}

@end
