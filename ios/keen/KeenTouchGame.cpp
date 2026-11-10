// Keen for iOS: the touch controls' engine side (see KeenTouch.h).
// Tells the overlay which mode the game is in, hands the overlay's buttons
// to the engine as its own input commands (the same ones the keyboard and a
// game controller press), and runs the sithewok cheat.

#include "KeenTouch.h"
#include "KeenTouchView.h"

#include <base/CInput.h>
#include <base/GsTimer.h>
#include <engine/core/CBehaviorEngine.h>
#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
int sSeenMode = KT_MODE_NONE;
int sPollsUnseen = 1000;
unsigned int sLastCommands = 0;
unsigned int sPendingPulses = 0;
SDL_atomic_t sAway;        // the app isn't active (another app, the home screen, a call)
bool sWatching = false;

// The app going away and coming back (SDL sends these from UIKit's callbacks)
int SDLCALL AppEvents(void *, SDL_Event *e)
{
    switch (e->type)
    {
    case SDL_APP_WILLENTERBACKGROUND:
        SDL_AtomicSet(&sAway, 1);
        break;
    case SDL_APP_DIDENTERFOREGROUND:
        SDL_AtomicSet(&sAway, 0);
        break;
    default:
        break;
    }
    return 1;
}

// How many polls the game may go without reporting its mode before the
// overlay hides (a menu has opened, or a cut scene started). Counted in polls,
// not time: every poll is followed by one game step (GsApp.cpp), so a slow
// load or a hitch doesn't hide the controls.
constexpr int kModeTimeoutPolls = 8;

struct CommandMap { unsigned int bit; int command; };
const CommandMap kCommands[] = {
    { KT_CMD_LEFT, IC_LEFT },           { KT_CMD_RIGHT, IC_RIGHT },
    { KT_CMD_UP, IC_UP },               { KT_CMD_DOWN, IC_DOWN },
    { KT_CMD_JUMP, IC_JUMP },           { KT_CMD_POGO, IC_POGO },
    { KT_CMD_FIRE, IC_FIRE },           { KT_CMD_STATUS, IC_STATUS },
    { KT_CMD_QUICKSAVE, IC_QUICKSAVE }, { KT_CMD_QUICKLOAD, IC_QUICKLOAD },
    { KT_CMD_MENU, IC_BACK },
};
}

extern "C" void KeenTouch_SeenMode(int mode)
{
    sSeenMode = mode;
    sPollsUnseen = 0;
}

extern "C" void KeenTouch_ApplyInput(void)
{
    if (!sWatching)
    {
        SDL_AddEventWatch(AppEvents, nullptr);
        sWatching = true;
    }

    int mode = (sPollsUnseen < kModeTimeoutPolls) ? sSeenMode : KT_MODE_NONE;
    sPollsUnseen++;

    // While the app is in the background the game stands still: nothing
    // held, nothing drawn, the game's clock not running. Back in a level or
    // on a map, it comes back to the game's menu (paused), not straight into
    // the action.
    if (SDL_AtomicGet(&sAway))
    {
        for (const auto &c : kCommands)
        {
            if (sLastCommands & c.bit)
                gInput.setCommand(0, c.command, false);
        }
        sLastCommands = 0;
        KeenTouchView_Frame(KT_MODE_NONE); // lets go of every touch
        while (SDL_AtomicGet(&sAway))
        {
            SDL_PumpEvents();
            SDL_Delay(50);
        }
        gTimer.setLogicReset(true);
        if (mode != KT_MODE_NONE)
            sPendingPulses |= KT_CMD_MENU;
        mode = KT_MODE_NONE;
    }

    const unsigned int now = KeenTouchView_Frame(mode) | sPendingPulses;
    sPendingPulses = 0;

    // Only what changed is written, so the keyboard and a game controller
    // keep working alongside the touches. A pulse (a tap, a finished hold)
    // is pressed for one poll and released on the next.
    const unsigned int changed = now ^ sLastCommands;
    for (const auto &c : kCommands)
    {
        if (changed & c.bit)
            gInput.setCommand(0, c.command, (now & c.bit) != 0);
    }
    sLastCommands = now;
}

// sithewok: everything at once (all items, keys and ammo; in Keen 1-3 also
// the pogo stick and 5 lives) and invincibility. "sithewok noclip" walks
// through walls (on a map: anywhere, to any level; in Keen 1-3 the engine
// does this on the map only), "sithewok jump" jumps without limit (Keen 1-3:
// on the pogo), "sithewok off" ends invincibility, noclip and jump (the
// items are kept).
extern "C" int KeenTouch_CheatTyped(const char *text)
{
    std::string s(text ? text : "");
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return std::tolower(ch); });
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);

    auto &cheat = gBehaviorEngine.mCheatmode;
    // Keen Dreams runs on different code (refkeen) that has none of these
    if (s.rfind("sithewok", 0) == 0 &&
        (sSeenMode == KT_MODE_DREAMS_LEVEL || sSeenMode == KT_MODE_DREAMS_MAP))
    {
        KeenTouchView_Message("sithewok doesn't work in Keen Dreams yet");
        return 1;
    }
    if (s == "sithewok")
    {
        cheat.god = true;
        cheat.items = true;
        KeenTouchView_Message("sithewok: everything, and invincible");
    }
    else if (s == "sithewok noclip")
    {
        cheat.noclipping = !cheat.noclipping;
        KeenTouchView_Message(cheat.noclipping ? "sithewok: walk through walls ON (Keen 1-3: on the map only)" : "sithewok: walk through walls OFF");
    }
    else if (s == "sithewok jump")
    {
        cheat.jump = !cheat.jump;
        KeenTouchView_Message(cheat.jump ? "sithewok: endless jumping ON (Keen 1-3: on the pogo)" : "sithewok: endless jumping OFF");
    }
    else if (s == "sithewok off")
    {
        cheat.god = false;
        cheat.noclipping = false;
        cheat.jump = false;
        KeenTouchView_Message("sithewok off (you keep the items)");
    }
    else
    {
        return 0;
    }
    return 1;
}
