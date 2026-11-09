/* Keen for iOS: the UIKit side of the touch controls (KeenTouch.m). */
#ifndef KEEN_TOUCH_VIEW_H
#define KEEN_TOUCH_VIEW_H
#ifdef __cplusplus
extern "C" {
#endif
/* Once a frame, from CInput::pollEvents (the main thread, after SDL has
   pumped UIKit's events): puts the overlay on screen if it isn't yet, tells
   it the game's mode, finishes holds, redraws, and returns the game
   commands the touches hold or pulse (KT_CMD_*). */
unsigned int KeenTouchView_Frame(int mode);
/* A short line shown over the game for a few seconds */
void KeenTouchView_Message(const char *text);
#ifdef __cplusplus
}
#endif
#endif
