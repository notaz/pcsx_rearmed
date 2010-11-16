/* NetSfPeopsSoftGPUPluginConfigController */

#define PluginConfigController NetSfPeopsSoftGPUPluginConfigController

#import <Cocoa/Cocoa.h>

@interface PluginConfigController : NSWindowController
{
    IBOutlet NSControl *autoFullScreen;
    IBOutlet NSPopUpButton *ditherMode;
    IBOutlet NSControl *fpsCounter;
    IBOutlet NSControl *frameSkipping;
    IBOutlet NSControl *hackEnable;
    IBOutlet NSView *hacksView;
    IBOutlet NSControl *vSync;
	 
	 NSMutableDictionary *keyValues;
}
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
- (IBAction)reset:(id)sender;
- (IBAction)hackToggle:(id)sender;

- (void)loadValues;

@end
