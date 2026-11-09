/* Keen for iOS: the touch controls' UIKit view (see KeenTouch.h).
   Built with ARC. A transparent view over SDL's: it takes only the touches
   that start on a button or in the stick's area while the game is in a
   level or on a map (KT_Wants); every other touch falls through to SDL, so
   menus and message boxes work by touch as before. All the logic is in
   KeenTouchCore.c; this only hands it touches and draws it. */

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include "KeenTouch.h"
#include "KeenTouchView.h"

#define KT_MESSAGE_TIME 3.0

static KT_State _kt;
static int _ktInit = 0;

@interface KeenTouchOverlay : UIView <UITextFieldDelegate>
@property (nonatomic, strong) UITextField *typing;
@property (nonatomic, copy) NSString *message;
@property (nonatomic) double messageUntil;
@property (nonatomic) int fpsFrames;
@property (nonatomic) double fpsStart;
@property (nonatomic) int fpsShown;
@end

static KeenTouchOverlay *_overlay = nil;

static double KT_Now(void) { return [NSProcessInfo processInfo].systemUptime; }

@implementation KeenTouchOverlay

- (instancetype)initWithFrame:(CGRect)frame
{
  if ((self = [super initWithFrame:frame])) {
    self.opaque = NO;
    self.backgroundColor = [UIColor clearColor];
    self.multipleTouchEnabled = YES;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.contentMode = UIViewContentModeRedraw;
    // The keyboard for the cheat: an off-screen text field
    _typing = [[UITextField alloc] initWithFrame:CGRectMake(-1000, -1000, 10, 10)];
    _typing.delegate = self;
    _typing.autocorrectionType = UITextAutocorrectionTypeNo;
    _typing.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _typing.spellCheckingType = UITextSpellCheckingTypeNo;
    _typing.returnKeyType = UIReturnKeyDone;
    [self addSubview:_typing];
  }
  return self;
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  const UIEdgeInsets in = self.safeAreaInsets;
  KT_Layout(&_kt, self.bounds.size.width, self.bounds.size.height, in.left, in.top, in.right, in.bottom);
  [self setNeedsDisplay];
}

- (void)safeAreaInsetsDidChange
{
  [super safeAreaInsetsDidChange];
  [self setNeedsLayout];
}

// Only touches the controls want; the rest go to the game (SDL's view below)
- (UIView *)hitTest:(CGPoint)p withEvent:(UIEvent *)event
{
  if (self.hidden || !KT_Wants(&_kt, p.x, p.y)) return nil;
  return self;
}

static long KT_Id(UITouch *t) { return (long)(__bridge void *)t; }

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  for (UITouch *t in touches) {
    const CGPoint p = [t locationInView:self];
    KT_TouchBegan(&_kt, KT_Id(t), p.x, p.y, t.timestamp);
  }
  [self setNeedsDisplay];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  for (UITouch *t in touches) {
    const CGPoint p = [t locationInView:self];
    KT_TouchMoved(&_kt, KT_Id(t), p.x, p.y, t.timestamp);
  }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  for (UITouch *t in touches) {
    const CGPoint p = [t locationInView:self];
    KT_TouchEnded(&_kt, KT_Id(t), p.x, p.y, t.timestamp, 0);
  }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
  for (UITouch *t in touches) {
    const CGPoint p = [t locationInView:self];
    KT_TouchEnded(&_kt, KT_Id(t), p.x, p.y, t.timestamp, 1);
  }
}

// ---- the keyboard (the sithewok cheat)

- (BOOL)textFieldShouldReturn:(UITextField *)field
{
  NSString *line = field.text ?: @"";
  field.text = @"";
  [field resignFirstResponder];
  if (line.length && !KeenTouch_CheatTyped(line.UTF8String))
    KeenTouchView_Message("Unknown cheat");
  return NO;
}

// ---- drawing

static void KT_DrawLabel(NSString *text, CGPoint c, CGFloat size, UIColor *color)
{
  NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
  para.alignment = NSTextAlignmentCenter;
  NSDictionary *attrs = @{ NSFontAttributeName : [UIFont boldSystemFontOfSize:size],
                           NSForegroundColorAttributeName : color,
                           NSParagraphStyleAttributeName : para };
  const CGSize sz = [text boundingRectWithSize:CGSizeMake(200, 200) options:NSStringDrawingUsesLineFragmentOrigin
                                    attributes:attrs context:nil].size;
  [text drawInRect:CGRectMake(c.x - 100, c.y - sz.height * 0.5, 200, sz.height) withAttributes:attrs];
}

static void KT_DrawKeyboard(CGPoint c, UIColor *color)
{
  // a small keyboard: a rounded box with three rows of keys
  UIBezierPath *box = [UIBezierPath bezierPathWithRoundedRect:CGRectMake(c.x - 11, c.y - 7, 22, 14) cornerRadius:2.5];
  box.lineWidth = 1.5;
  [color setStroke];
  [box stroke];
  [color setFill];
  for (int row = 0; row < 2; row++)
    for (int k = 0; k < 4; k++)
      UIRectFill(CGRectMake(c.x - 8 + k * 4.5, c.y - 4.5 + row * 3.5, 2.5, 2));
  UIRectFill(CGRectMake(c.x - 5, c.y + 2.5, 10, 2));
}

- (void)drawRect:(CGRect)rect
{
  if (_kt.mode == KT_MODE_NONE) return;
  const double now = KT_Now();

  // The stick, where the left thumb is down
  if (_kt.stickTouch >= 0) {
    const KT_Touch *t = &_kt.touch[_kt.stickTouch];
    UIBezierPath *base = [UIBezierPath bezierPathWithArcCenter:CGPointMake(_kt.stickX, _kt.stickY) radius:KT_STICK_RADIUS
                                                    startAngle:0 endAngle:2 * M_PI clockwise:YES];
    [[UIColor colorWithWhite:1 alpha:0.12] setFill];
    [base fill];
    [[UIColor colorWithWhite:1 alpha:0.5] setStroke];
    base.lineWidth = 2;
    [base stroke];
    float dx = t->x - _kt.stickX, dy = t->y - _kt.stickY;
    const float d = hypotf(dx, dy);
    if (d > KT_STICK_RADIUS) { dx *= KT_STICK_RADIUS / d; dy *= KT_STICK_RADIUS / d; }
    UIBezierPath *knob = [UIBezierPath bezierPathWithArcCenter:CGPointMake(_kt.stickX + dx, _kt.stickY + dy) radius:24
                                                    startAngle:0 endAngle:2 * M_PI clockwise:YES];
    [[UIColor colorWithWhite:1 alpha:0.55] setFill];
    [knob fill];
  }

  for (int b = 0; b < KT_BTN_COUNT; b++) {
    const KT_Button *btn = &_kt.btn[b];
    if (!btn->shown) continue;
    const BOOL down = KT_ButtonDown(&_kt, b);
    const CGFloat alpha = down ? 1.0 : KT_IDLE_ALPHA;
    const CGPoint c = CGPointMake(btn->x, btn->y);
    UIBezierPath *circle = [UIBezierPath bezierPathWithArcCenter:c radius:btn->radius startAngle:0 endAngle:2 * M_PI clockwise:YES];
    [[UIColor colorWithWhite:down ? 0.45 : 0.15 alpha:0.55 * alpha] setFill];
    [circle fill];
    [[UIColor colorWithWhite:1 alpha:0.8 * alpha] setStroke];
    circle.lineWidth = 2;
    [circle stroke];
    UIColor *text = [UIColor colorWithWhite:1 alpha:alpha];
    if (b == KT_BTN_TRAYKEYS) {
      KT_DrawKeyboard(c, text);
    } else {
      NSString *label = @(btn->label);
      if (b == KT_BTN_TRAYFPS && _kt.showFps) label = @"FPS\nON";
      KT_DrawLabel(label, c, btn->radius >= 40 ? 15 : btn->radius >= 30 ? 12 : 8.5, text);
    }
    // QUICK SAVE, QUICK LOAD and MENU: a ring fills round the button while held
    const float f = KT_HoldProgress(&_kt, b, now);
    if (f > 0) {
      UIBezierPath *ring = [UIBezierPath bezierPathWithArcCenter:c radius:btn->radius + 4 startAngle:-M_PI_2
                                                        endAngle:-M_PI_2 + 2 * M_PI * f clockwise:YES];
      ring.lineWidth = 4;
      [[UIColor colorWithRed:1 green:0.85 blue:0.2 alpha:1] setStroke];
      [ring stroke];
    }
  }

  // The tray's version label: which build this is
  if (_kt.trayOpen) {
    NSDictionary *info = [NSBundle mainBundle].infoDictionary;
    NSString *ver = info[@"CFBundleShortVersionString"] ?: @"?";
    NSString *build = info[@"CFBundleVersion"];
    NSString *label = [NSString stringWithFormat:@"v%@", ver];
    if (build.length && ![build isEqualToString:ver]) label = [label stringByAppendingFormat:@" (%@)", build];
    const CGFloat midX = (_kt.btn[KT_BTN_TRAYFPS].x + _kt.btn[KT_BTN_TRAYKEYS].x) * 0.5;
    KT_DrawLabel(label, CGPointMake(midX, _kt.versionY), 9, [UIColor colorWithWhite:1 alpha:0.5]);
  }

  if (_kt.showFps)
    KT_DrawLabel([NSString stringWithFormat:@"%d FPS", self.fpsShown], CGPointMake(_kt.fpsX, _kt.fpsY), 11,
                 [UIColor colorWithWhite:1 alpha:0.8]);

  if (self.message && now < self.messageUntil) {
    NSDictionary *attrs = @{ NSFontAttributeName : [UIFont boldSystemFontOfSize:14] };
    const CGSize sz = [self.message sizeWithAttributes:attrs];
    const CGRect r = CGRectMake(_kt.W * 0.5 - sz.width * 0.5 - 12, _kt.top + 50, sz.width + 24, sz.height + 12);
    [[UIColor colorWithWhite:0 alpha:0.6] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:r cornerRadius:8] fill];
    KT_DrawLabel(self.message, CGPointMake(CGRectGetMidX(r), CGRectGetMidY(r)), 14, [UIColor whiteColor]);
  }
}

@end

static void KT_Attach(void)
{
  UIWindow *window = nil;
  for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) continue;
    for (UIWindow *w in ((UIWindowScene *)scene).windows)
      if (w.isKeyWindow) window = w;
  }
  UIView *host = window.rootViewController.view;
  if (!host) return; // no key window yet: try again next frame
  _overlay = [[KeenTouchOverlay alloc] initWithFrame:host.bounds];
  [host addSubview:_overlay];
  [_overlay setNeedsLayout];
}

unsigned int KeenTouchView_Frame(int mode)
{
  if (!_ktInit) {
    KT_Init(&_kt);
    _ktInit = 1;
  }
  // Video settings recreate SDL's window: move to the new one, letting go of
  // every touch on the old
  if (_overlay && (_overlay.window == nil || _overlay.window.hidden)) {
    [_overlay removeFromSuperview];
    _overlay = nil;
    KT_SetMode(&_kt, KT_MODE_NONE);
  }
  if (!_overlay) KT_Attach();
  if (_overlay && _overlay.superview && _overlay.superview.subviews.lastObject != _overlay)
    [_overlay.superview bringSubviewToFront:_overlay]; // stay above anything SDL adds

  const double now = KT_Now();
  KT_SetMode(&_kt, mode);
  KT_Tick(&_kt, now);

  if (_overlay) {
    // The keyboard closes when the controls go away (a menu opened)
    if (mode == KT_MODE_NONE && _overlay.typing.isFirstResponder) [_overlay.typing resignFirstResponder];
    if (_kt.wantKeyboard) {
      _kt.wantKeyboard = 0;
      [_overlay.typing becomeFirstResponder];
    }
    // FPS: frames drawn (KeenTouchView_FrameDrawn) over KT_FPS_PERIOD
    if (now - _overlay.fpsStart >= KT_FPS_PERIOD) {
      const int fps = (int)lround(_overlay.fpsFrames / (now - _overlay.fpsStart));
      if (_kt.showFps && fps != _overlay.fpsShown) _kt.dirty = 1;
      _overlay.fpsShown = fps;
      _overlay.fpsFrames = 0;
      _overlay.fpsStart = now;
    }
    if (_overlay.message && now >= _overlay.messageUntil) {
      _overlay.message = nil;
      _kt.dirty = 1;
    }
    if (_kt.dirty) {
      _kt.dirty = 0;
      [_overlay setNeedsDisplay];
    }
  }
  return KT_TakeCommands(&_kt);
}

void KeenTouchView_FrameDrawn(void)
{
  if (_overlay) _overlay.fpsFrames++;
}

void KeenTouchView_Message(const char *text)
{
  if (!_overlay) return;
  _overlay.message = @(text);
  _overlay.messageUntil = KT_Now() + KT_MESSAGE_TIME;
  _kt.dirty = 1;
}
