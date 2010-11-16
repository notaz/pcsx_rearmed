//
//  EmuThread.m
//  Pcsx
//
//  Created by Gil Pedersen on Sun Sep 21 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <ExceptionHandling/NSExceptionHandler.h>
#import <AppKit/NSApplication.h>
#include <pthread.h>
#include <setjmp.h>
#import "EmuThread.h"
#include "psxcommon.h"
#include "plugins.h"
#include "misc.h"

EmuThread *emuThread;
NSString *defrostPath = nil;
static int safeEvent;
static int paused;
static int runbios;

static pthread_cond_t eventCond;
static pthread_mutex_t eventMutex;

#define EMUEVENT_NONE		0
#define EMUEVENT_PAUSE		(1<<0)
#define EMUEVENT_RESET		(1<<1)
#define EMUEVENT_STOP		(1<<2)

@implementation EmuThread

- (void)EmuThreadRun:(id)anObject
{
	pool = [[NSAutoreleasePool alloc] init];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowDidClose:)
        name:@"emuWindowDidClose" object:nil];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowWantPause:)
        name:@"emuWindowWantPause" object:nil];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowWantResume:)
        name:@"emuWindowWantResume" object:nil];

	// we shouldn't change the priority, since we might depend on subthreads
	//[NSThread setThreadPriority:1.0-((1.0-[NSThread threadPriority])/4.0)];

	// Do processing here
	if (OpenPlugins() == -1)
		goto done;

	setjmp(restartJmp);

	EmuReset();

	int res = CheckCdrom();
	if (res == -1) {
		ClosePlugins();
		SysMessage(_("Could not check CD-ROM!\n"));
		goto done;
	}

	LoadCdrom();

	if (defrostPath) {
		LoadState([defrostPath fileSystemRepresentation]);
		[defrostPath release]; defrostPath = nil;
	}

	psxCpu->Execute();

done:
	[pool release]; pool = nil;
	emuThread = nil;

	return;
}

- (void)EmuThreadRunBios:(id)anObject
{
	pool = [[NSAutoreleasePool alloc] init];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowDidClose:)
        name:@"emuWindowDidClose" object:nil];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowWantPause:)
        name:@"emuWindowWantPause" object:nil];

	[[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(emuWindowWantResume:)
        name:@"emuWindowWantResume" object:nil];

	// we shouldn't change the priority, since we might depend on subthreads
	//[NSThread setThreadPriority:1.0-((1.0-[NSThread threadPriority])/4.0)];

	// Do processing here
	if (OpenPlugins() == -1)
		goto done;

	EmuReset();

	psxCpu->Execute();

done:
	[pool release]; pool = nil;
	emuThread = nil;
	
	return;
}

- (void)dealloc
{
	// remove all registered observers
	[[NSNotificationCenter defaultCenter] removeObserver:self name:nil object:nil];

	if (pool)
		[pool release];

	[super dealloc];
}

- (void)emuWindowDidClose:(NSNotification *)aNotification
{
	[EmuThread stop];
}

- (void)emuWindowWantPause:(NSNotification *)aNotification
{
	wasPaused = [EmuThread pause];
}

- (void)emuWindowWantResume:(NSNotification *)aNotification
{
	if (!wasPaused) {
		[EmuThread resume];
	}
	wasPaused = NO;
}

/* called periodically from the emulation thread */
- (void)handleEvents
{
	/* only do a trylock here, since we're not interested in blocking, 
	   and we can just handle events next time round */
	if (pthread_mutex_trylock(&eventMutex) == 0) {
		while (safeEvent) {
			if (safeEvent & EMUEVENT_STOP) {
				/* signify that the emulation has stopped */
				[emuThread autorelease];
				emuThread = nil;
				paused = NO;
				
				/* better unlock the mutex before killing ourself */
				pthread_mutex_unlock(&eventMutex);

				ClosePlugins();
				SysClose();
				
				//[[NSThread currentThread] autorelease];
				[NSThread exit];
				return;
			}

			if (safeEvent & EMUEVENT_RESET) {
#if 0
				/* signify that the emulation has stopped */
				[emuThread autorelease];
				emuThread = nil;

				/* better unlock the mutex before killing ourself */
				pthread_mutex_unlock(&eventMutex);

				ClosePlugins();

				// start a new emulation thread
				[EmuThread run];

				//[[NSThread currentThread] autorelease];
				[NSThread exit];
				return;
#else
				safeEvent &= ~EMUEVENT_RESET;
				pthread_mutex_unlock(&eventMutex);

				longjmp(restartJmp, 0);
#endif
			}
			
			if (safeEvent & EMUEVENT_PAUSE) {
				paused = 2;
				/* wait until we're signalled */
				pthread_cond_wait(&eventCond, &eventMutex);
			}
		}
		pthread_mutex_unlock(&eventMutex);
	}
}

+ (void)run
{
	int err;

	if (emuThread) {
		[EmuThread resume];
		return;
	}

	if (pthread_mutex_lock(&eventMutex) != 0) {
		err = pthread_cond_init(&eventCond, NULL);
		if (err) return;

		err = pthread_mutex_init(&eventMutex, NULL);
		if (err) return;

		pthread_mutex_lock(&eventMutex);
	}

    safeEvent = EMUEVENT_NONE;
    paused = NO;
    runbios = NO;

	if (SysInit() != 0) {
		pthread_mutex_unlock(&eventMutex);
		return;
	}

	emuThread = [[EmuThread alloc] init];

    [NSThread detachNewThreadSelector:@selector(EmuThreadRun:) 
                toTarget:emuThread withObject:nil];

	pthread_mutex_unlock(&eventMutex);
}

+ (void)runBios
{
	int err;

	if (emuThread) {
		[EmuThread resume];
		return;
	}

	if (pthread_mutex_lock(&eventMutex) != 0) {
		err = pthread_cond_init(&eventCond, NULL);
		if (err) return;

		err = pthread_mutex_init(&eventMutex, NULL);
		if (err) return;

		pthread_mutex_lock(&eventMutex);
	}

    safeEvent = EMUEVENT_NONE;
    paused = NO;
    runbios = YES;

	if (SysInit() != 0) {
		pthread_mutex_unlock(&eventMutex);
		return;
	}

	emuThread = [[EmuThread alloc] init];

    [NSThread detachNewThreadSelector:@selector(EmuThreadRunBios:) 
                toTarget:emuThread withObject:nil];

	pthread_mutex_unlock(&eventMutex);
}

+ (void)stop
{
	pthread_mutex_lock(&eventMutex);
	safeEvent = EMUEVENT_STOP;
	pthread_mutex_unlock(&eventMutex);
	
	// wake it if it's sleeping
	pthread_cond_broadcast(&eventCond);
}

+ (BOOL)pause
{
	if (paused || ![EmuThread active])
        return YES;
    
	pthread_mutex_lock(&eventMutex);
	safeEvent |= EMUEVENT_PAUSE;
	paused = 1;
	pthread_mutex_unlock(&eventMutex);

	pthread_cond_broadcast(&eventCond);
	
	return NO;
}

+ (BOOL)pauseSafe
{
	if ((paused == 2) || ![EmuThread active])
        return YES;

	[EmuThread pause];
	while ([EmuThread isPaused] != 2) [NSThread sleepUntilDate:[[NSDate date] addTimeInterval:0.05]];
	
	return NO;
}

+ (void)resume
{
	if (!paused || ![EmuThread active])
		return;
	
	pthread_mutex_lock(&eventMutex);
	
	safeEvent &= ~EMUEVENT_PAUSE;
	paused = NO;
	pthread_mutex_unlock(&eventMutex);

	pthread_cond_broadcast(&eventCond);
}

+ (void)reset
{
	pthread_mutex_lock(&eventMutex);
	safeEvent = EMUEVENT_RESET;
	pthread_mutex_unlock(&eventMutex);

	pthread_cond_broadcast(&eventCond);
}

// must only be called from within the emulation thread!!!
+ (void)resetNow
{
	/* signify that the emulation has stopped */
	[emuThread autorelease];
	emuThread = nil;

	ClosePlugins();

	// start a new emulation thread
	[EmuThread run];

	//[[NSThread currentThread] autorelease];
	[NSThread exit];
	return;
}

+ (BOOL)isPaused
{
    return paused;
}

+ (BOOL)isRunBios
{
    return runbios;
}

+ (BOOL)active
{
	return emuThread ? YES : NO;
}

+ (void)freezeAt:(NSString *)path which:(int)num
{
	BOOL emuWasPaused = [EmuThread pauseSafe];
	char Text[256];

	GPU_freeze(2, (GPUFreeze_t *)&num);
	int ret = SaveState([path fileSystemRepresentation]);
	if (ret == 0) sprintf (Text, _("*PCSX*: Saved State %d"), num+1);
	else sprintf (Text, _("*PCSX*: Error Saving State %d"), num+1);
	GPU_displayText(Text);

	if (!emuWasPaused) {
		[EmuThread resume];
	}
}

+ (BOOL)defrostAt:(NSString *)path
{
	const char *cPath = [path fileSystemRepresentation];
	if (CheckState(cPath) != 0)
		return NO;

	defrostPath = [path retain];
	[EmuThread reset];

	GPU_displayText(_("*PCSX*: Loaded State"));
	return YES;
}

@end
