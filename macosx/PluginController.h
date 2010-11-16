/* PluginController */

#import <Cocoa/Cocoa.h>
#import "PluginList.h"

@interface PluginController : NSObject
{
    IBOutlet NSButton *aboutButton;
    IBOutlet NSButton *configureButton;
    IBOutlet NSPopUpButton *pluginMenu;
	 
	 int pluginType;
	 NSArray *plugins;
	 NSString *defaultKey;
}
- (IBAction)doAbout:(id)sender;
- (IBAction)doConfigure:(id)sender;
- (IBAction)selectPlugin:(id)sender;

- (void)setPluginsTo:(NSArray *)list withType:(int)type;

@end
