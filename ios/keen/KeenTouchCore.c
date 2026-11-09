/* Keen for iOS: the touch controls' logic, plain C (see KeenTouch.h).
   No UIKit here: KeenTouch.m feeds it touches and draws it, and the Linux
   test (not in the repository) drives this same file. */

#include "KeenTouch.h"
#include <math.h>
#include <string.h>

static const KT_Button kButtons[KT_BTN_COUNT] = {
  [KT_BTN_FIRE]      = { "FIRE",        42.0f },
  /* JUMP is as big as FIRE: in a platformer it is the button pressed most */
  [KT_BTN_JUMP]      = { "JUMP",        42.0f },
  [KT_BTN_POGO]      = { "POGO",        31.0f },
  [KT_BTN_STATUS]    = { "STATUS",      22.0f },
  [KT_BTN_QUICKSAVE] = { "QUICK\nSAVE", 22.0f },
  [KT_BTN_QUICKLOAD] = { "QUICK\nLOAD", 22.0f },
  [KT_BTN_MENU]      = { "MENU",        22.0f },
  [KT_BTN_TRAYFPS]   = { "FPS",         20.0f },
  [KT_BTN_TRAYKEYS]  = { "",            20.0f }, /* drawn as a keyboard */
};

static int IsTray(int b) { return b == KT_BTN_TRAYFPS || b == KT_BTN_TRAYKEYS; }
static int IsCluster(int b) { return b == KT_BTN_FIRE || b == KT_BTN_JUMP || b == KT_BTN_POGO; }
static unsigned int ClusterCmd(int b) { return b == KT_BTN_FIRE ? KT_CMD_FIRE : b == KT_BTN_JUMP ? KT_CMD_JUMP : KT_CMD_POGO; }

static void UpdateShown(KT_State *s)
{
  const int level = s->mode == KT_MODE_LEVEL, map = s->mode == KT_MODE_MAP, any = level || map;
  s->btn[KT_BTN_FIRE].shown = level;
  s->btn[KT_BTN_POGO].shown = level;
  /* on a map JUMP enters the level Keen stands on */
  s->btn[KT_BTN_JUMP].shown = any;
  s->btn[KT_BTN_STATUS].shown = any;
  s->btn[KT_BTN_QUICKSAVE].shown = any;
  s->btn[KT_BTN_QUICKLOAD].shown = any;
  s->btn[KT_BTN_MENU].shown = any;
  s->btn[KT_BTN_TRAYFPS].shown = any && s->trayOpen;
  s->btn[KT_BTN_TRAYKEYS].shown = any && s->trayOpen;
}

void KT_Init(KT_State *s)
{
  memset(s, 0, sizeof(*s));
  memcpy(s->btn, kButtons, sizeof(kButtons));
  s->stickTouch = -1;
  KT_Layout(s, 844, 390, 47, 0, 47, 21); /* until the view reports the real screen */
}

void KT_Layout(KT_State *s, float W, float H, float inLeft, float inTop, float inRight, float inBottom)
{
  s->W = W;
  s->H = H;
  s->left = fmaxf(inLeft, 8.0f);
  s->right = W - fmaxf(inRight, 8.0f);
  s->top = fmaxf(inTop, 8.0f);
  s->bottom = H - fmaxf(inBottom, 8.0f);

  /* The cluster: FIRE where the standard puts it, JUMP left of it and POGO
     on the arc between them, all KT_CLUSTER_ARC from FIRE's centre */
  const float fx = s->right - (inRight < 20.0f ? 100.0f : 76.0f), fy = s->bottom - 58.0f;
  s->btn[KT_BTN_FIRE].x = fx;
  s->btn[KT_BTN_FIRE].y = fy;
  s->btn[KT_BTN_JUMP].x = fx - KT_CLUSTER_ARC;
  s->btn[KT_BTN_JUMP].y = fy;
  s->btn[KT_BTN_POGO].x = fx + KT_CLUSTER_ARC * cosf((float)M_PI * 120.0f / 180.0f);
  s->btn[KT_BTN_POGO].y = fy - KT_CLUSTER_ARC * sinf((float)M_PI * 120.0f / 180.0f);

  /* Top row: STATUS on the left; QUICK SAVE, QUICK LOAD, MENU on the right */
  const float rowY = s->top + 26.0f;
  s->btn[KT_BTN_STATUS].x = s->left + 24.0f;
  s->btn[KT_BTN_STATUS].y = rowY;
  s->btn[KT_BTN_QUICKSAVE].x = s->right - 156.0f;
  s->btn[KT_BTN_QUICKLOAD].x = s->right - 90.0f;
  s->btn[KT_BTN_MENU].x = s->right - 24.0f;
  s->btn[KT_BTN_QUICKSAVE].y = s->btn[KT_BTN_QUICKLOAD].y = s->btn[KT_BTN_MENU].y = rowY;
  s->fpsX = s->right - 216.0f;
  s->fpsY = rowY;

  /* MENU's tray: the keyboard right under MENU, FPS to its left; the version
     label under the row */
  const float ty = rowY + 52.0f;
  s->btn[KT_BTN_TRAYKEYS].x = s->btn[KT_BTN_MENU].x;
  s->btn[KT_BTN_TRAYFPS].x = s->btn[KT_BTN_MENU].x - KT_TRAY_STEP;
  s->btn[KT_BTN_TRAYKEYS].y = s->btn[KT_BTN_TRAYFPS].y = ty;
  s->versionY = ty + 20.0f + 12.0f;

  UpdateShown(s);
  s->dirty = 1;
}

static void ReleaseAll(KT_State *s)
{
  for (int i = 0; i < KT_MAX_TOUCHES; i++)
    if (s->touch[i].id) s->touch[i].owner = -2; /* the rest of these touches does nothing */
  s->stickTouch = -1;
  s->held = 0;
  s->trayOpen = 0;
}

void KT_SetMode(KT_State *s, int mode)
{
  if (mode == s->mode) return;
  /* Anything held stops: a new mode starts with nothing pressed */
  ReleaseAll(s);
  s->pulses = 0;
  s->mode = mode;
  UpdateShown(s);
  s->dirty = 1;
}

/* The shown button nearest (x, y), within its radius plus KT_TOUCH_SLOP; -1: none */
static int ButtonAt(const KT_State *s, float x, float y, int trayOnly)
{
  int best = -1;
  float bestD = 1e9f;
  for (int b = 0; b < KT_BTN_COUNT; b++) {
    const KT_Button *p = &s->btn[b];
    if (!p->shown || (trayOnly && !IsTray(b))) continue;
    const float d = hypotf(x - p->x, y - p->y);
    if (d <= p->radius + KT_TOUCH_SLOP && d - p->radius < bestD) {
      bestD = d - p->radius;
      best = b;
    }
  }
  return best;
}

static int InStickZone(const KT_State *s, float x, float y)
{
  (void)y;
  return x < s->W * KT_STICK_ZONE;
}

int KT_Wants(const KT_State *s, float x, float y)
{
  if (s->mode == KT_MODE_NONE) return 0;
  if (s->trayOpen) return 1; /* a touch anywhere else closes it */
  return ButtonAt(s, x, y, 0) >= 0 || InStickZone(s, x, y);
}

static KT_Touch *FindTouch(KT_State *s, long id)
{
  for (int i = 0; i < KT_MAX_TOUCHES; i++)
    if (s->touch[i].id == id) return &s->touch[i];
  return 0;
}

static int OwnedByOther(const KT_State *s, int b, const KT_Touch *self)
{
  for (int i = 0; i < KT_MAX_TOUCHES; i++)
    if (s->touch[i].id && &s->touch[i] != self && s->touch[i].owner == b) return 1;
  return 0;
}

static void UpdateHeld(KT_State *s)
{
  unsigned int held = 0;
  if (s->stickTouch >= 0) {
    const KT_Touch *t = &s->touch[s->stickTouch];
    const float dx = t->x - s->stickX, dy = s->stickY - t->y; /* + up */
    const float mag = hypotf(dx, dy) / KT_STICK_RADIUS;
    if (mag >= KT_STICK_DEADZONE) {
      float deg = atan2f(dy, dx) * 180.0f / (float)M_PI; /* 0 right, 90 up */
      if (deg < 0) deg += 360.0f;
      if (s->mode == KT_MODE_MAP) {
        /* the map: eight ways */
        const int sector = ((int)floorf((deg + 22.5f) / 45.0f)) % 8;
        static const unsigned int dirs[8] = {
          KT_CMD_RIGHT, KT_CMD_RIGHT | KT_CMD_UP, KT_CMD_UP, KT_CMD_LEFT | KT_CMD_UP,
          KT_CMD_LEFT, KT_CMD_LEFT | KT_CMD_DOWN, KT_CMD_DOWN, KT_CMD_RIGHT | KT_CMD_DOWN };
        held |= dirs[sector];
      } else {
        /* a level: walking takes everything within 60 degrees of level, so a
           thumb that drifts a little up or down while running doesn't stop
           Keen to look or open a door; up and down are the 30 degrees
           either side of straight up and down */
        if (deg <= 60.0f || deg >= 300.0f) held |= KT_CMD_RIGHT;
        else if (deg >= 120.0f && deg <= 240.0f) held |= KT_CMD_LEFT;
        else if (deg < 180.0f) held |= KT_CMD_UP;
        else held |= KT_CMD_DOWN;
      }
    }
  }
  for (int i = 0; i < KT_MAX_TOUCHES; i++) {
    const KT_Touch *t = &s->touch[i];
    if (!t->id || !IsCluster(t->owner) || !s->btn[t->owner].shown) continue;
    held |= ClusterCmd(t->owner);
  }
  if (held != s->held) s->dirty = 1;
  s->held = held;
}

void KT_TouchBegan(KT_State *s, long id, float x, float y, double t)
{
  if (!id || FindTouch(s, id)) return;
  KT_Touch *slot = FindTouch(s, 0);
  if (!slot) return;
  memset(slot, 0, sizeof(*slot));
  slot->id = id;
  slot->x0 = slot->x = x;
  slot->y0 = slot->y = y;
  slot->t0 = t;
  slot->on = 1;
  slot->owner = -2;
  s->dirty = 1;

  if (s->mode == KT_MODE_NONE) return;
  if (s->trayOpen) {
    const int b = ButtonAt(s, x, y, 1);
    if (b >= 0 && !OwnedByOther(s, b, slot)) {
      slot->owner = b;
    } else {
      s->trayOpen = 0; /* a touch anywhere else closes it, and does nothing more */
      UpdateShown(s);
    }
    return;
  }
  const int b = ButtonAt(s, x, y, 0);
  if (b >= 0) {
    /* a second finger on a button already held does nothing (one save, one menu) */
    if (!OwnedByOther(s, b, slot)) {
      slot->owner = b;
      /* a tap quicker than a frame still presses it for one */
      if (IsCluster(b) && s->btn[b].shown) s->pulses |= ClusterCmd(b);
    }
  } else if (InStickZone(s, x, y) && s->stickTouch < 0) {
    slot->owner = -1;
    s->stickTouch = (int)(slot - s->touch);
    s->stickX = x;
    s->stickY = y;
  }
  UpdateHeld(s);
}

void KT_TouchMoved(KT_State *s, long id, float x, float y, double t)
{
  (void)t;
  KT_Touch *p = FindTouch(s, id);
  if (!p) return;
  p->x = x;
  p->y = y;
  if (p->owner >= 0) {
    const KT_Button *b = &s->btn[p->owner];
    const int on = hypotf(x - b->x, y - b->y) <= b->radius + KT_TOUCH_SLOP;
    if (IsCluster(p->owner)) {
      /* a thumb can roll from one of JUMP, POGO, FIRE onto another */
      const int nb = ButtonAt(s, x, y, 0);
      if (!on && nb >= 0 && IsCluster(nb) && !OwnedByOther(s, nb, p)) {
        p->owner = nb;
        s->pulses |= ClusterCmd(nb);
      }
      /* sliding off into empty space keeps the button held until the thumb
         lifts or rolls onto another: forgiving for a thumb that drifts */
    } else if (!on && p->on) {
      p->on = 0; /* slid off: the hold or tap is cancelled for good */
      s->dirty = 1;
    }
  }
  if (p->owner == -1) s->dirty = 1;
  UpdateHeld(s);
}

void KT_TouchEnded(KT_State *s, long id, float x, float y, double t, int cancelled)
{
  (void)t;
  KT_Touch *p = FindTouch(s, id);
  if (!p) return;
  p->x = x;
  p->y = y;
  const int owner = p->owner, on = p->on && !cancelled, fired = p->fired;
  if (owner >= 0 && !IsCluster(owner) && on && hypotf(x - s->btn[owner].x, y - s->btn[owner].y) > s->btn[owner].radius + KT_TOUCH_SLOP)
    p->on = 0;
  const int stillOn = owner >= 0 && on && p->on;
  if ((int)(p - s->touch) == s->stickTouch) s->stickTouch = -1;
  memset(p, 0, sizeof(*p));
  s->dirty = 1;

  if (stillOn && s->mode != KT_MODE_NONE) {
    switch (owner) {
    case KT_BTN_STATUS: s->pulses |= KT_CMD_STATUS; break;
    case KT_BTN_MENU: if (!fired) s->pulses |= KT_CMD_MENU; break;
    case KT_BTN_TRAYFPS: s->showFps = !s->showFps; s->trayOpen = 0; break;
    case KT_BTN_TRAYKEYS: s->wantKeyboard = 1; s->trayOpen = 0; break;
    default: break; /* QUICK SAVE / LOAD act only once held long enough */
    }
    UpdateShown(s);
  }
  UpdateHeld(s);
}

void KT_Tick(KT_State *s, double t)
{
  if (s->mode == KT_MODE_NONE) return;
  int saved = 0;
  /* QUICK SAVE first, so if both finish in the same frame only the save happens */
  for (int pass = 0; pass < 3; pass++) {
    const int b = pass == 0 ? KT_BTN_QUICKSAVE : pass == 1 ? KT_BTN_QUICKLOAD : KT_BTN_MENU;
    const double hold = pass == 0 ? KT_QUICKSAVE_HOLD : pass == 1 ? KT_QUICKLOAD_HOLD : KT_MENU_HOLD;
    for (int i = 0; i < KT_MAX_TOUCHES; i++) {
      KT_Touch *p = &s->touch[i];
      if (!p->id || p->owner != b || !p->on || p->fired || t - p->t0 < hold) continue;
      p->fired = 1;
      s->dirty = 1;
      if (b == KT_BTN_QUICKSAVE) { s->pulses |= KT_CMD_QUICKSAVE; saved = 1; }
      else if (b == KT_BTN_QUICKLOAD) { if (!saved) s->pulses |= KT_CMD_QUICKLOAD; }
      else { s->trayOpen = 1; UpdateShown(s); }
    }
  }
  /* rings fill while held */
  for (int i = 0; i < KT_MAX_TOUCHES; i++) {
    const KT_Touch *p = &s->touch[i];
    if (p->id && p->on && !p->fired && (p->owner == KT_BTN_QUICKSAVE || p->owner == KT_BTN_QUICKLOAD || p->owner == KT_BTN_MENU))
      s->dirty = 1;
  }
}

float KT_HoldProgress(const KT_State *s, int b, double t)
{
  if (b != KT_BTN_QUICKSAVE && b != KT_BTN_QUICKLOAD && b != KT_BTN_MENU) return -1.0f;
  const double hold = b == KT_BTN_QUICKSAVE ? KT_QUICKSAVE_HOLD : b == KT_BTN_QUICKLOAD ? KT_QUICKLOAD_HOLD : KT_MENU_HOLD;
  for (int i = 0; i < KT_MAX_TOUCHES; i++) {
    const KT_Touch *p = &s->touch[i];
    if (!p->id || p->owner != b || !p->on) continue;
    if (p->fired) return 1.0f;
    const double f = (t - p->t0) / hold;
    return (float)(f < 0 ? 0 : f > 1 ? 1 : f);
  }
  return -1.0f;
}

int KT_ButtonDown(const KT_State *s, int b)
{
  for (int i = 0; i < KT_MAX_TOUCHES; i++)
    if (s->touch[i].id && s->touch[i].owner == b && (s->touch[i].on || IsCluster(b))) return 1;
  return 0;
}

unsigned int KT_TakeCommands(KT_State *s)
{
  const unsigned int c = s->held | s->pulses;
  s->pulses = 0;
  return c;
}
