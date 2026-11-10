/* Keen for iOS: on-screen touch controls.

   The house standard (as in the Jedi Knight and Serious Sam ports), adapted
   for a 2D platformer:
   - a move stick under the left thumb (left / right walk, up enters doors
     and looks up, down looks down),
   - JUMP, POGO and FIRE in the right thumb's cluster,
   - STATUS at the top left; QUICK SAVE, QUICK LOAD (0.3 s holds with a ring)
     and MENU (tap: the game's menu; 0.45 s hold: the tray) at the top right,
   - MENU's tray: FPS and the keyboard (for the sithewok cheat), with the
     app's build stamp under it.

   KeenTouchCore.c is plain C (no UIKit) so the Linux test drives the very
   same code; KeenTouch.m is the thin UIKit view that feeds it touches and
   draws its state; KeenTouchGame.cpp hands its output to the engine. */

#ifndef KEEN_TOUCH_H
#define KEEN_TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* What the game is showing; the engine reports it every frame it is in one
   (KeenTouch_SeenMode). Anything else (menus, the title, cut scenes) is
   KT_MODE_NONE: the overlay hides and every touch goes to the game as a
   mouse click, as without it. */
enum { KT_MODE_NONE = 0, KT_MODE_LEVEL = 1, KT_MODE_MAP = 2,
       /* Keen Dreams: no pogo stick (FIRE throws flower power, labelled
          THROW), and the engine has no quick save or quick load there */
       KT_MODE_DREAMS_LEVEL = 3, KT_MODE_DREAMS_MAP = 4 };

enum {
  KT_BTN_FIRE, KT_BTN_JUMP, KT_BTN_POGO,
  KT_BTN_STATUS, KT_BTN_QUICKSAVE, KT_BTN_QUICKLOAD, KT_BTN_MENU,
  KT_BTN_TRAYFPS, KT_BTN_TRAYKEYS, /* MENU's tray, left to right */
  KT_BTN_COUNT
};

/* Game commands the overlay holds or pulses (KT_Output) */
enum {
  KT_CMD_LEFT = 1 << 0, KT_CMD_RIGHT = 1 << 1, KT_CMD_UP = 1 << 2, KT_CMD_DOWN = 1 << 3,
  KT_CMD_JUMP = 1 << 4, KT_CMD_POGO = 1 << 5, KT_CMD_FIRE = 1 << 6,
  KT_CMD_STATUS = 1 << 7, KT_CMD_QUICKSAVE = 1 << 8, KT_CMD_QUICKLOAD = 1 << 9,
  KT_CMD_MENU = 1 << 10
};

#define KT_MAX_TOUCHES 10

typedef struct {
  const char *label;
  float radius;     /* points */
  float x, y;       /* centre, set by KT_Layout */
  int shown;        /* in this mode (and the tray: while open) */
} KT_Button;

typedef struct {
  long id;          /* 0: free */
  int owner;        /* -1 stick, else KT_BTN_*; -2 nothing (ignored) */
  float x0, y0, x, y;
  double t0;
  int on;           /* still on its button (a slide off cancels holds for good) */
  int fired;        /* a hold already did its thing */
} KT_Touch;

typedef struct {
  /* layout */
  float W, H, left, right, top, bottom;
  KT_Button btn[KT_BTN_COUNT];
  float fpsX, fpsY, versionY;
  /* state */
  int mode;
  int trayOpen;
  int showFps;
  KT_Touch touch[KT_MAX_TOUCHES];
  int stickTouch;   /* index into touch[], -1: none */
  float stickX, stickY;  /* where the stick appeared */
  unsigned int held;     /* commands held this frame */
  unsigned int pulses;   /* commands to send once (taps, holds), until read */
  int wantKeyboard;      /* the tray's keyboard was tapped */
  int dirty;             /* needs a redraw */
} KT_State;

/* Shared numbers (house standard) */
#define KT_IDLE_ALPHA 0.65
#define KT_TOUCH_SLOP 6.0f
#define KT_STICK_ZONE 0.42f
#define KT_STICK_RADIUS 60.0f
#define KT_STICK_DEADZONE 0.30f
#define KT_QUICKSAVE_HOLD 0.3
#define KT_QUICKLOAD_HOLD 0.3
#define KT_MENU_HOLD 0.45
#define KT_TRAY_STEP 50.0f
#define KT_CLUSTER_ARC 115.0f
#define KT_FPS_W 54.0f
#define KT_FPS_H 18.0f
#define KT_FPS_PERIOD 0.5

void KT_Init(KT_State *s);
/* Screen size and safe-area insets, in points (landscape) */
void KT_Layout(KT_State *s, float W, float H, float inLeft, float inTop, float inRight, float inBottom);
void KT_SetMode(KT_State *s, int mode);
/* Would the overlay take a touch that starts here? (Otherwise it goes to the game.) */
int KT_Wants(const KT_State *s, float x, float y);
void KT_TouchBegan(KT_State *s, long id, float x, float y, double t);
void KT_TouchMoved(KT_State *s, long id, float x, float y, double t);
void KT_TouchEnded(KT_State *s, long id, float x, float y, double t, int cancelled);
/* Once a frame: finishes the holds that have run their time */
void KT_Tick(KT_State *s, double t);
/* How far (0..1) a hold has run, for its ring; -1: no ring */
float KT_HoldProgress(const KT_State *s, int button, double t);
int KT_ButtonDown(const KT_State *s, int button);
/* Commands held now, plus the pulses not read yet (which this clears) */
unsigned int KT_TakeCommands(KT_State *s);

/* The engine side (KeenTouchGame.cpp) */
void KeenTouch_SeenMode(int mode);   /* called by the engine while in a level or on a map */
void KeenTouch_ApplyInput(void);     /* called by CInput::pollEvents */
int KeenTouch_CheatTyped(const char *text); /* the keyboard's line; 1 if it was a cheat */

#ifdef __cplusplus
}
#endif

#endif
