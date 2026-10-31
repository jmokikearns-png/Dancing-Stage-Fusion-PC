#!/usr/bin/env python3
"""Record the Win64 build under Wine/Xvfb for the show-off video.
usage: record_demo.py RUN_DIR OUT_DIR
Captures the game window (x11grab) and the PulseAudio null sink, drives the
build through a fixed storyboard with held keys, and writes OUT_DIR/raw.mkv
plus OUT_DIR/events.json (seconds from the start of the recording)."""
import io, json, os, signal, subprocess, sys, time
from PIL import Image

RUN, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
ENV = dict(os.environ, DISPLAY=':97', WINEPREFIX='/home/user/work/builds/wine-dsf', WINEDEBUG='-all',
           XDG_RUNTIME_DIR=os.environ.get('XDG_RUNTIME_DIR',
               '/tmp/dsf-xdg'))
WIN_X, WIN_Y, W, H = 160, 165, 960, 720
events = []
t0 = None

def now():
    return time.monotonic() - t0

def mark(name):
    events.append({'t': round(now(), 2), 'event': name})
    print('%7.2f %s' % (now(), name), flush=True)

def x(*args):
    subprocess.run(['xdotool'] + list(args), env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def key(k, hold=0.15, gap=0.0):
    win = subprocess.run(['xdotool', 'search', '--name', 'Dancing Stage Fusion PC'], env=ENV,
                         capture_output=True, text=True).stdout.split()
    if win:
        x('windowactivate', '--sync', win[0])
    x('keydown', '--clearmodifiers', k)
    time.sleep(hold)
    x('keyup', k)
    if gap:
        time.sleep(gap)

def alive():
    if wine.poll() is not None or not os.path.exists(os.path.join(RUN, 'log.txt')):
        raise SystemExit('StepMania is not running any more (t=%.1f)' % now())
    with open(os.path.join(RUN, 'log.txt'), errors='replace') as f:
        if 'Exception:' in f.read():
            raise SystemExit('StepMania threw an exception (t=%.1f), see log.txt' % now())

def grab():
    png = subprocess.run(['import', '-window', 'root', '-crop', '%dx%d+%d+%d' % (W, H, WIN_X, WIN_Y), 'png:-'],
                         env=ENV, capture_output=True).stdout
    return Image.open(io.BytesIO(png)).convert('RGB')

def near(px, ref, tol):
    return all(abs(a - b) <= tol for a, b in zip(px, ref))

def is_language(im):	# the green highlight bar of the boot language list, on black
    if max(im.getpixel((100, 60))) > 40 or max(im.getpixel((480, 690))) > 40:
        return False
    return any(im.getpixel((480, y))[1] > 110 and im.getpixel((480, y))[1] > im.getpixel((480, y))[0] + 40
               for y in range(140, 230, 4))

def is_options(im):	# the OPTIONS header: red top, orange body
    return near(im.getpixel((480, 8)), (232, 69, 47), 45) and near(im.getpixel((100, 60)), (247, 112, 14), 45)

def wait_for(name, pred, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        alive()
        if pred(grab()):
            mark(name)
            return
        time.sleep(0.25)
    raise SystemExit('%s did not appear within %d s (t=%.1f)' % (name, timeout, now()))

def keys(seq, gap=0.7):
    for k in seq.split():
        key(k, gap=gap)

ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error',
    '-thread_queue_size', '2048', '-f', 'x11grab', '-framerate', '50', '-video_size', '%dx%d' % (W, H),
    '-i', ':97.0+%d,%d' % (WIN_X, WIN_Y),
    '-thread_queue_size', '2048', '-f', 'pulse', '-i', 'nullsink.monitor',
    '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '17', '-pix_fmt', 'yuv420p',
    '-c:a', 'aac', '-b:a', '192k', os.path.join(OUT, 'raw.mkv')], env=ENV, stdin=subprocess.PIPE)
t0 = time.monotonic()
mark('record')
exe = 'Program/StepMania.exe'
wine = subprocess.Popen(['wine', exe], cwd=RUN, env=ENV, stdout=open(os.path.join(OUT, 'wine.log'), 'w'),
                        stderr=subprocess.STDOUT, start_new_session=True)
mark('launch')
try:
    time.sleep(19); mark('sm_title')
    time.sleep(3)
    keys('Down Return', 1.2); mark('sm_options')
    time.sleep(2)
    key('Return'); mark('sm_mappings')
    time.sleep(5)
    key('Escape', gap=1.5); mark('sm_options_back')
    key('Escape', gap=1.5); mark('sm_title_back')
    alive()
    key('Return'); mark('play')	# back on the title, the cursor is on PLAY again
    wait_for('language', is_language, 60)
    time.sleep(2.0)
    key('x'); mark('language_ok')
    time.sleep(15); mark('title')
    key('Return', gap=3.0); mark('mode_menu')
    key('Up', gap=1.2)
    alive()
    key('Return'); mark('options_enter')
    wait_for('options', is_options, 20)
    time.sleep(1.5)	# the menu ignores input while it fades in
    keys('Down x', 1.2); mark('sound_options')
    time.sleep(1.5)
    keys('Down Down Down', 0.9); mark('volume')
    time.sleep(1.0)
    key('Left', gap=1.4); mark('volume_90')
    key('Right', gap=1.4); mark('volume_100')
    key('Down', gap=1.6); mark('audio_buffer')
    key('Down', gap=1.6); mark('picture_sync')
    key('Down', gap=1.6); mark('picture_delay')
    key('Down', gap=1.2); mark('sound_exit')
    key('x', gap=1.5); mark('options_top')
    keys('Down Down Down Down', 0.6)
    key('x', gap=1.8); mark('graphic_options')
    keys('Down Down Down Down', 0.7); mark('display_settings_row')
    time.sleep(1.5)
    key('x', gap=1.8); mark('display_page')
    for r in ('resolution', 'refresh_rate', 'vsync', 'game_resolution', 'renderer'):
        key('Down', gap=1.4); mark('page_' + r)
    key('Down', gap=1.0); mark('page_exit')
    key('x', gap=1.5); mark('graphic_back')
    key('s', gap=1.5); mark('options_top_back')
    keys('Down Down Down', 0.6)
    key('x'); mark('mode_menu_back')
    alive()
    time.sleep(125); alive(); mark('end')
finally:
    os.killpg(wine.pid, signal.SIGTERM)
    subprocess.run(['wineserver', '-k'], env=ENV)
    time.sleep(1)
    ff.communicate(b'q', timeout=60)
    json.dump(events, open(os.path.join(OUT, 'events.json'), 'w'), indent=1)
