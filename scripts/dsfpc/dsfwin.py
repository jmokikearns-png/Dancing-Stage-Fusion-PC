"""Drive the Win64 build under Wine/Xvfb from Python: start/stop, held keys,
window screenshots and screen detection.  Used by record_demo.py and the
test scripts.  The game window is 960x720 at +160+165 on the :97 screen."""
import io, os, subprocess, time
from PIL import Image

ENV = dict(os.environ, DISPLAY=':97', WINEPREFIX='/home/user/work/builds/wine-dsf', WINEDEBUG='-all',
           XDG_RUNTIME_DIR=os.environ.get('XDG_RUNTIME_DIR',
               '/tmp/dsf-xdg'))
WIN_X, WIN_Y, W, H = 160, 165, 960, 720
RUN = '/home/user/work/builds/dsfpc-run'
_wine = None

def start(run=RUN, exe='Program/StepMania.exe', log=None):
    global _wine
    stop()
    _wine = subprocess.Popen(['wine', exe], cwd=run, env=ENV, start_new_session=True,
                             stdout=open(log or os.path.join(run, 'wine-stdout.log'), 'w'), stderr=subprocess.STDOUT)
    return _wine

def stop():
    subprocess.run(['wineserver', '-k'], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

def running():
    return _wine is not None and _wine.poll() is None

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

def keys(seq, gap=0.7):
    for k in seq.split():
        key(k, gap=gap)

def grab():
    png = subprocess.run(['import', '-window', 'root', '-crop', '%dx%d+%d+%d' % (W, H, WIN_X, WIN_Y), 'png:-'],
                         env=ENV, capture_output=True).stdout
    return Image.open(io.BytesIO(png)).convert('RGB')

def shot(path):
    grab().save(path)
    return path

def near(px, ref, tol):
    return all(abs(a - b) <= tol for a, b in zip(px, ref))

def is_language(im):	# the green highlight bar of the boot language list, on black
    if max(im.getpixel((100, 60))) > 40 or max(im.getpixel((480, 690))) > 40:
        return False
    return any(im.getpixel((480, y))[1] > 110 and im.getpixel((480, y))[1] > im.getpixel((480, y))[0] + 40
               for y in range(140, 230, 4))

def is_options(im):	# the OPTIONS header: red top, orange body
    return near(im.getpixel((480, 8)), (232, 69, 47), 45) and near(im.getpixel((100, 60)), (247, 112, 14), 45)

def wait_for(pred, timeout, what='screen'):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if not running():
            raise SystemExit('StepMania exited while waiting for ' + what)
        if pred(grab()):
            return
        time.sleep(0.25)
    raise SystemExit('%s did not appear within %d s' % (what, timeout))

def boot_to_options():
    """StepMania title -> PLAY -> English -> game title -> mode menu -> OPTIONS."""
    time.sleep(19)
    key('Return')
    wait_for(is_language, 60, 'the language screen')
    time.sleep(2.0)
    key('x')
    time.sleep(15)
    key('Return', gap=3.0)
    key('Up', gap=1.2)      # wraps to QUIT (the last entry)
    key('Up', gap=1.2)      # OPTIONS
    key('Return')
    wait_for(is_options, 20, 'OPTIONS')
    time.sleep(1.5)
