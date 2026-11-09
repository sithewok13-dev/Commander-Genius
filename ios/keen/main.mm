// Keen for iOS: app entry point.
// Starts UIKit through SDL, points Commander Genius at the app's Documents
// folder (Files > On My iPhone > Keen) and makes the "games" folder there,
// so it shows up in the Files app before any game is copied in.

#import <Foundation/Foundation.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <string>
#include "CGeniusEntry.h"

extern std::string gIOSSandboxSharedFilesDir;

static int KeenMain(int argc, char *argv[])
{
    @autoreleasepool {
        NSString *docs = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
        NSString *games = [docs stringByAppendingPathComponent:@"games"];
        [[NSFileManager defaultManager] createDirectoryAtPath:games
                                  withIntermediateDirectories:YES attributes:nil error:nil];
        gIOSSandboxSharedFilesDir = [docs UTF8String];
        // Relative paths and the log land in Documents too
        chdir([docs fileSystemRepresentation]);
    }
    return CommanderGenius_Run(argc, argv);
}

int main(int argc, char *argv[])
{
    return SDL_UIKitRunApp(argc, argv, KeenMain);
}
