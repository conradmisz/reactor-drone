#!/usr/bin/env python3
"""drive_ui.py — drive Reactor Drone's real UI on an X display, for verification
that scripted `--keys` / `--clicks` cannot reach.

WHY THIS EXISTS
    `--keys` has no letter-key vocabulary (ESC/SPACE/F1/B/TAB/Q/digits only), so
    any screen with typed text — name entry, the feedback form — is unreachable
    headlessly. This drives the actual window via XTest instead. Precedent: the
    D197 leaderboard walkthrough; used again for D198/D200.

THREE THINGS THAT COST AN HOUR EACH TO REDISCOVER — encoded in the helpers below:

1. CLICKS ARE IN DESIGN SPACE, NOT SCREEN SPACE.
   Widget rects in GameData.json live in an 800x600 design canvas.
   ui_canvas_transform (engine/ecs/systems/ui_render_math.hpp) maps it to the
   window with scale = min(win_w/800, win_h/600) and centring offsets. At the
   default 980x660 that is scale 1.1, offset_x 50, offset_y 0. Design y is
   BOTTOM-UP; screen y is top-down. Clicking raw rect coords lands somewhere
   else entirely — the first attempt hit QUIT instead of FEEDBACK and killed
   the game. Use click_widget(), which does the mapping.

2. KEYS MUST BE HELD ACROSS FRAMES.
   Menu/text keys arrive as SDL events and a 1ms tap is fine, but movement and
   SPACE are read with SDL_GetKeyboardState() — polled once per frame. A press
   and release inside one frame is never observed. key() holds ~70ms (>= 4
   frames at 60fps). A too-fast SPACE silently fails to start a run.

3. THERE IS NO WINDOW MANAGER ON A BARE Xvfb.
   Nothing assigns input focus, so XTest key events go nowhere. focus_game()
   finds the window by name and calls set_input_focus explicitly. It also
   accumulates ancestor geometry offsets, because the window is not necessarily
   at the root origin.

ALSO WORTH KNOWING
  - Screenshots: pass --screenshot N ... to the game rather than grabbing the
    root window; the game writes BMPs to logs/<timestamp>/ relative to its CWD,
    so run it from a writable directory.
  - An unregistered save (`registered: false` in saves/meta.json) plus a live
    network boots into NAME ENTRY, not the title. A probe that assumes the
    title will type into the name field instead. Write a registered meta.json
    first, and RESTORE the original afterwards.
  - Frame count is a valid freeze probe: Timer::end_frame() increments
    frame_count_, end_frame_no_advance() does not. If a screen is meant to
    freeze the sim, its frame count must NOT scale with how long you sit on it.

USAGE
    from drive_ui import Driver
    with Driver(seed=7, screenshots=range(120, 3000, 120)) as g:
        g.key("space")                       # start a run
        g.key("Escape")                      # pause
        g.click_widget(540, 36, 86, 48)      # pause_feedback's design rect
        g.type_text("Subject here")
        g.key("Tab")
"""
import os
import subprocess
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
GAME = os.path.join(REPO, "CPP", "build", "game", "game")
UI_DESIGN_W, UI_DESIGN_H = 800.0, 600.0

os.environ.setdefault("DISPLAY", ":1")
from Xlib import X, XK, display          # noqa: E402
from Xlib.ext import xtest               # noqa: E402

SHIFTED = set('~!@#$%^&*()_+{}|:"<>?')
KEYNAMES = {' ': 'space', ',': 'comma', '.': 'period', '-': 'minus',
            '!': 'exclam', "'": 'apostrophe', '?': 'question', '_': 'underscore',
            '/': 'slash', ':': 'colon', ';': 'semicolon', '\n': 'Return'}


class Driver:
    def __init__(self, seed=42, screenshots=(), args=(), win_w=980, win_h=660,
                 log_path=None, cwd=REPO):
        self.d = display.Display()
        self.root = self.d.screen().root
        self.win_w, self.win_h = win_w, win_h
        argv = [GAME, "--seed", str(seed)]
        if screenshots:
            argv += ["--screenshot"] + [str(n) for n in screenshots]
        argv += list(args)
        self._log = open(log_path or os.devnull, "w")
        self.proc = subprocess.Popen(argv, stdout=self._log,
                                     stderr=subprocess.STDOUT, cwd=cwd)
        self.log_path = log_path
        time.sleep(4)
        self._focus()

    # --- lifecycle -------------------------------------------------------
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self._log.close()

    def frames(self):
        """Final frame count from the game's shutdown line (call after close())."""
        if not self.log_path:
            return -1
        for line in open(self.log_path):
            if line.startswith("Shutting down"):
                return int(line.split("Frames:")[1].split()[0])
        return -1

    # --- see note 3 ------------------------------------------------------
    def _focus(self):
        def find(w):
            try:
                name = w.get_wm_name()
            except Exception:
                name = None
            if name and "Reactor Drone" in str(name):
                return w
            for c in w.query_tree().children:
                r = find(c)
                if r:
                    return r
            return None

        win = None
        for _ in range(20):
            win = find(self.root)
            if win:
                break
            time.sleep(0.5)
        if not win:
            raise RuntimeError("game window not found on " + os.environ["DISPLAY"])
        self.d.set_input_focus(win, X.RevertToParent, X.CurrentTime)
        self.d.sync()
        self.ox = self.oy = 0
        w = win
        while True:
            g = w.get_geometry()
            self.ox += g.x
            self.oy += g.y
            parent = w.query_tree().parent
            if parent.id == self.root.id:
                break
            w = parent

    # --- see note 2 ------------------------------------------------------
    def key(self, name, shift=False, hold=0.07):
        code = self.d.keysym_to_keycode(XK.string_to_keysym(name))
        shift_code = self.d.keysym_to_keycode(XK.string_to_keysym('Shift_L'))
        if shift:
            xtest.fake_input(self.d, X.KeyPress, shift_code)
        xtest.fake_input(self.d, X.KeyPress, code)
        self.d.sync()
        time.sleep(hold)          # must span >= 1 polled frame
        xtest.fake_input(self.d, X.KeyRelease, code)
        if shift:
            xtest.fake_input(self.d, X.KeyRelease, shift_code)
        self.d.sync()
        time.sleep(0.04)

    def type_text(self, text):
        for c in text:
            if c in KEYNAMES:
                self.key(KEYNAMES[c], shift=(c in SHIFTED))
            elif c.isupper():
                self.key(c.lower(), shift=True)
            elif c in SHIFTED:
                self.key(c, shift=True)
            else:
                self.key(c)

    # --- see note 1 ------------------------------------------------------
    def design_to_screen(self, dx, dy, dw=0.0, dh=0.0):
        """Centre of a GameData.json rect (bottom-up design space) -> screen px."""
        scale = min(self.win_w / UI_DESIGN_W, self.win_h / UI_DESIGN_H)
        off_x = (self.win_w - UI_DESIGN_W * scale) * 0.5
        off_y = (self.win_h - UI_DESIGN_H * scale) * 0.5
        cx, cy = dx + dw / 2.0, dy + dh / 2.0
        # round(), not int(): 800*1.1 is 880.0000000000001, so truncation puts
        # the left edge at 49 instead of 50 and every click drifts a pixel.
        return (round(off_x + cx * scale) + self.ox,
                round(self.win_h - off_y - cy * scale) + self.oy)

    def click_widget(self, dx, dy, dw, dh, settle=0.3):
        x, y = self.design_to_screen(dx, dy, dw, dh)
        xtest.fake_input(self.d, X.MotionNotify, x=x, y=y)
        self.d.sync()
        time.sleep(0.1)
        xtest.fake_input(self.d, X.ButtonPress, 1)
        self.d.sync()
        time.sleep(0.08)
        xtest.fake_input(self.d, X.ButtonRelease, 1)
        self.d.sync()
        time.sleep(settle)


if __name__ == "__main__":
    # Self-check: the design->screen mapping against the documented 980x660 case.
    class _Fake(Driver):
        def __init__(self):
            self.win_w, self.win_h = 980, 660
            self.ox = self.oy = 0
    f = _Fake()
    assert f.design_to_screen(0, 600) == (50, 0), f.design_to_screen(0, 600)
    assert f.design_to_screen(800, 0) == (930, 660), f.design_to_screen(800, 0)
    # pause_feedback (540,36,86,48) sits low-right, as its bottom-up y implies
    x, y = f.design_to_screen(540, 36, 86, 48)
    assert 630 < x < 700 and 590 < y < 650, (x, y)
    print("drive_ui self-check OK")
