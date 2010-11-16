/* NetSfPeopsSPUPluginNamedSlider */

#import <Cocoa/Cocoa.h>

#define NamedSlider NetSfPeopsSPUPluginNamedSlider

@interface NamedSlider : NSSlider
{
	NSArray *strings;
}

- (void)setStrings:(NSArray *)theStrings;
@end
