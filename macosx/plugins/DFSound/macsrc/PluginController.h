/* NetSfPeopsSPUPluginController */

#import <Cocoa/Cocoa.h>
#import "NamedSlider.h"

void DoAbout();
long DoConfiguration();
void LoadConfiguration();

#define PluginController NetSfPeopsSPUPluginController

@interface PluginController : NSWindowController
{
    IBOutlet NSControl *hiCompBox;
    IBOutlet NetSfPeopsSPUPluginNamedSlider *interpolValue;
    IBOutlet NSControl *irqWaitBox;
    IBOutlet NSControl *monoSoundBox;
    IBOutlet NetSfPeopsSPUPluginNamedSlider *reverbValue;
    IBOutlet NSControl *xaEnableBox;
    IBOutlet NSControl *xaSpeedBox;
	 
	 NSMutableDictionary *keyValues;
}
- (IBAction)cancel:(id)sender;
- (IBAction)ok:(id)sender;
- (IBAction)reset:(id)sender;

- (void)loadValues;
@end
