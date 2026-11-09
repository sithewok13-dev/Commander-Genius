// Keen for iOS: the touch controls' engine side (see KeenTouch.h).
// Tells the overlay which mode the game is in, hands the overlay's buttons
// to the engine as its own input commands (the same ones the keyboard and a
// game controller press), and runs the sithewok cheat.

#include "KeenTouch.h"
#include "KeenTouchView.h"

#include <base/CInput.h>
#include <engine/core/CBehaviorEngine.h>
#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
int sSeenMode = KT_MODE_NONE;
Uint32 sSeenTicks = 0;
unsigned int sLastCommands = 0;

// How long the game may go without reporting its mode before the overlay
// hides (a menu has opened, or a cut scene started)
constexpr Uint32 kModeTimeoutMs = 250;

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
    sSeenTicks = SDL_GetTicks();
}

extern "C" void KeenTouch_ApplyInput(void)
{
    const int mode = (SDL_GetTicks() - sSeenTicks < kModeTimeoutMs) ? sSeenMode : KT_MODE_NONE;
    const unsigned int now = KeenTouchView_Frame(mode);

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
// through walls (on a map: anywhere, to any level), "sithewok jump" jumps
// without limit, "sithewok off" ends invincibility, noclip and jump (the
// items are kept).
extern "C" int KeenTouch_CheatTyped(const char *text)
{
    std::string s(text ? text : "");
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return std::tolower(ch); });
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);

    auto &cheat = gBehaviorEngine.mCheatmode;
    if (s == "sithewok")
    {
        cheat.god = true;
        cheat.items = true;
        KeenTouchView_Message("sithewok: everything, and invincible");
    }
    else if (s == "sithewok noclip")
    {
        cheat.noclipping = !cheat.noclipping;
        KeenTouchView_Message(cheat.noclipping ? "sithewok: walk through walls ON" : "sithewok: walk through walls OFF");
    }
    else if (s == "sithewok jump")
    {
        cheat.jump = !cheat.jump;
        KeenTouchView_Message(cheat.jump ? "sithewok: endless jumping ON" : "sithewok: endless jumping OFF");
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
