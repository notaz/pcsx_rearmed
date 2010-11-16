//
//  PcsxPlugin.h
//  Pcsx
//
//  Created by Gil Pedersen on Fri Oct 03 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface PcsxPlugin : NSObject {
    CFBundleRef pluginRef;
    
    NSString *path;
	 NSDate *modDate;
    NSString *name;
    long version;
    int type;
	 int active;
}

+ (NSString *)getPrefixForType:(int)type;
+ (NSString *)getDefaultKeyForType:(int)type;
+ (char **)getConfigEntriesForType:(int)type;

- (id)initWithPath:(NSString *)aPath;

- (NSString *)getDisplayVersion;
- (int)getType;
- (NSString *)path;
- (NSString *)description;
- (BOOL)hasAboutAs:(int)type;
- (BOOL)hasConfigureAs:(int)type;
- (long)initAs:(int)aType;
- (long)shutdownAs:(int)aType;
- (void)aboutAs:(int)type;
- (void)configureAs:(int)type;
- (BOOL)verifyOK;

@end
