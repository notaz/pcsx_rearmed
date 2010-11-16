/* ConfigurationController */

#import <Cocoa/Cocoa.h>
#import "PluginController.h"
#import "PluginList.h"

@interface ConfigurationController : NSWindowController
{
    IBOutlet PluginController *cdromPlugin;
    IBOutlet PluginController *graphicsPlugin;
    IBOutlet PluginController *padPlugin;
    IBOutlet PluginController *soundPlugin;

	IBOutlet id noXaAudioCell;
	IBOutlet id sioIrqAlwaysCell;
	IBOutlet id bwMdecCell;
	IBOutlet id autoVTypeCell;
	IBOutlet id vTypePALCell;
	IBOutlet id noCDAudioCell;
	IBOutlet id usesHleCell;
	IBOutlet id usesDynarecCell;
	IBOutlet id consoleOutputCell;
	IBOutlet id spuIrqAlwaysCell;
	IBOutlet id rCountFixCell;
	IBOutlet id vSyncWAFixCell;
	IBOutlet id noFastBootCell;

	IBOutlet NSTextField *mcd1Label;
	IBOutlet NSTextField *mcd2Label;

	NSMutableDictionary *checkBoxDefaults;
}
- (IBAction)setCheckbox:(id)sender;
- (IBAction)setCheckboxInverse:(id)sender;
- (IBAction)setVideoType:(id)sender;
- (IBAction)mcdChangeClicked:(id)sender;
- (IBAction)mcdNewClicked:(id)sender;

- (NSString *)keyForSender:(id)sender;

@end
