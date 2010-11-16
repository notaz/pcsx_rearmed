#import "NamedSlider.h"

@implementation NamedSlider

- (void)dealloc
{
	[strings release];
	[super dealloc];
}

- (void)setStrings:(NSArray *)theStrings
{
	[strings release];
	strings = [theStrings retain];
}

- (NSString *)stringValue
{
	int index = [self intValue];
	
	if (index >= 0 && index < [strings count])
		return [strings objectAtIndex:index];
	
	return @"(Unknown)";
}

- (void)setIntValue:(int)value
{
	[super setIntValue:value];
	[self sendAction:[self action] to:[self target]];
}

@end
