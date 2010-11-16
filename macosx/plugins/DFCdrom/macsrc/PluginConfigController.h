#define PluginConfigController DFCdromPluginConfigController

#import <Cocoa/Cocoa.h>

@interface PluginConfigController : NSWindowController
{
	IBOutlet NSControl *Cached;
	IBOutlet NSSlider *CacheSize;
	IBOutlet NSPopUpButton *CdSpeed;

	NSMutableDictionary *keyValues;
}
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;

- (void)loadValues;

@end
