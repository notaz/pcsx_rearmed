/* PcsxController */

#import <Cocoa/Cocoa.h>
#import "EmuThread.h"
#import "PluginList.h"

@interface PcsxController : NSObject
{
    NSWindowController *preferencesController;
    NSWindowController *memCardController;
	 PluginList *pluginList;
	 
    IBOutlet NSWindow *memCardWindow;
    IBOutlet NSWindow *preferenceWindow;
	 
	 BOOL sleepInBackground;
	 BOOL wasPausedBeforeBGSwitch;
}
- (IBAction)ejectCD:(id)sender;
- (IBAction)pause:(id)sender;
- (IBAction)preferences:(id)sender;
- (IBAction)reset:(id)sender;
- (IBAction)runCD:(id)sender;
- (IBAction)runIso:(id)sender;
- (IBAction)runBios:(id)sender;
- (IBAction)freeze:(id)sender;
- (IBAction)defrost:(id)sender;
- (IBAction)fullscreen:(id)sender;

+ (void)setConfigFromDefaults;
+ (void)setDefaultFromConfig:(NSString *)defaultKey;
+ (BOOL)biosAvailable;

@end
