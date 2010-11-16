#import "PluginConfigController.h"
#include "gpu.h"
#include "cfg.h"
#include "menu.h"
#include "externals.h"

#define APP_ID @"net.sf.peops.SoftGpuGLPlugin"
#define PrefsKey APP_ID @" Settings"

static PluginConfigController *windowController;
char * pConfigFile=NULL;

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


void SoftDlgProc()
{
	NSWindow *window;
	
	if (windowController == nil) {
		windowController = [[PluginConfigController alloc] initWithWindowNibName:@"NetSfPeopsSoftGPUConfig"];
	}
	window = [windowController window];
	
	/* load values */
	[windowController loadValues];
	
	[window center];
	[window makeKeyAndOrderFront:nil];
}

void ReadConfig(void)
{
	NSDictionary *keyValues;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
			[[NSMutableDictionary alloc] initWithObjectsAndKeys:
					[NSNumber numberWithBool:NO], @"FPS Counter",
					[NSNumber numberWithBool:NO], @"Auto Full Screen",
					[NSNumber numberWithBool:NO], @"Frame Skipping",
					[NSNumber numberWithBool:YES], @"Frame Limit",
					[NSNumber numberWithBool:NO], @"VSync",
					[NSNumber numberWithBool:NO], @"Enable Hacks",
					[NSNumber numberWithInt:1], @"Dither Mode",
					[NSNumber numberWithLong:0], @"Hacks",
					nil], PrefsKey,
			nil]];
	
	keyValues = [defaults dictionaryForKey:PrefsKey];

	iShowFPS = [[keyValues objectForKey:@"FPS Counter"] boolValue];
	iWindowMode = [[keyValues objectForKey:@"Auto Full Screen"] boolValue] ? 0 : 1;
	UseFrameSkip = [[keyValues objectForKey:@"Frame Skipping"] boolValue];
	UseFrameLimit = [[keyValues objectForKey:@"Frame Limit"] boolValue];
	//??? = [[keyValues objectForKey:@"VSync"] boolValue];
	iUseFixes = [[keyValues objectForKey:@"Enable Hacks"] boolValue];

	iUseDither = [[keyValues objectForKey:@"Dither Mode"] intValue];
	dwCfgFixes = [[keyValues objectForKey:@"Hacks"] longValue];
	
	iResX = 640;
	iResY = 480;
	iUseNoStretchBlt = 1;

	fFrameRate = 60;
	iFrameLimit = 2;
	
	if (iShowFPS)
		ulKeybits|=KEY_SHOWFPS;
	else
		ulKeybits&=~KEY_SHOWFPS;

 // additional checks
 if(!iColDepth)       iColDepth=32;
 if(iUseFixes)        dwActFixes=dwCfgFixes;
 else						 dwActFixes=0;
 SetFixes();
 
 if(iFrameLimit==2) SetAutoFrameCap();
 bSkipNextFrame = FALSE;
 
 szDispBuf[0]=0;
 BuildDispMenu(0);
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
	[writeDic setObject:[NSNumber numberWithInt:[fpsCounter intValue]] forKey:@"FPS Counter"];
	[writeDic setObject:[NSNumber numberWithInt:[autoFullScreen intValue]] forKey:@"Auto Full Screen"];
	[writeDic setObject:[NSNumber numberWithInt:[frameSkipping intValue]] forKey:@"Frame Skipping"];
	//[writeDic setObject:[NSNumber numberWithInt:[frameLimit intValue]] forKey:@"Frame Limit"];
	[writeDic setObject:[NSNumber numberWithInt:[vSync intValue]] forKey:@"VSync"];
	[writeDic setObject:[NSNumber numberWithInt:[hackEnable intValue]] forKey:@"Enable Hacks"];

	[writeDic setObject:[NSNumber numberWithInt:[ditherMode indexOfSelectedItem]] forKey:@"Dither Mode"];
	
	unsigned long hackValues = 0;
	int i;
	NSArray *views = [hacksView subviews];
	for (i=0; i<[views count]; i++) {
	   NSView *control = [views objectAtIndex:i];
		if ([control isKindOfClass:[NSButton class]]) {
			hackValues |= [(NSControl *)control intValue] << ([control tag] - 1);
		}
	}
	
	[writeDic setObject:[NSNumber numberWithLong:hackValues] forKey:@"Hacks"];
	
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

- (IBAction)hackToggle:(id)sender
{
	BOOL enable = [sender intValue] ? YES : NO;
	int i;
	NSArray *views = [hacksView subviews];

	for (i=0; i<[views count]; i++) {
	   NSView *control = [views objectAtIndex:i];
		if ([control isKindOfClass:[NSButton class]]) {
			[(NSControl *)control setEnabled:enable];
		}
	}
}

- (void)loadValues
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	ReadConfig();
	
	/* load from preferences */
	[keyValues release];
	keyValues = [[defaults dictionaryForKey:PrefsKey] retain];
	
	[fpsCounter setIntValue:[[keyValues objectForKey:@"FPS Counter"] intValue]];
	[autoFullScreen setIntValue:[[keyValues objectForKey:@"Auto Full Screen"] intValue]];
	[frameSkipping setIntValue:[[keyValues objectForKey:@"Frame Skipping"] intValue]];
	[vSync setIntValue:[[keyValues objectForKey:@"VSync"] intValue]];
	[hackEnable setIntValue:[[keyValues objectForKey:@"Enable Hacks"] intValue]];

	[ditherMode selectItemAtIndex:[[keyValues objectForKey:@"Dither Mode"] intValue]];

	unsigned long hackValues = [[keyValues objectForKey:@"Hacks"] longValue];
	
	int i;
	NSArray *views = [hacksView subviews];
	for (i=0; i<[views count]; i++) {
	   NSView *control = [views objectAtIndex:i];
		if ([control isKindOfClass:[NSButton class]]) {
			[(NSControl *)control setIntValue:(hackValues >> ([control tag] - 1)) & 1];
		}
	}
	
	[self hackToggle:hackEnable];
}

- (void)awakeFromNib
{
	hacksView = [[hacksView subviews] objectAtIndex:0];
}

@end
