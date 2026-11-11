"""Titles in Serpentine, drawn by FreeType (PIL): white, a dark outline and a red
under-edge, like the game's headers.  text/<slug>.png, 200 px type."""
import re, sys
from PIL import Image, ImageDraw, ImageFont
FONT = '/home/user/work/trailer-public/serpentine.ttf'
OUT = '/home/user/work/trailer-public/text/'
TEXTS = ['THE PLAYSTATION\nORIGINAL', 'NOW ON PC', '12 NEW SONGS', 'YOUR', 'DISC.', 'PC.',
         'BUILT FROM YOUR OWN DISC', 'SMOOTHER PLAYBACK', 'UPDATES FROM INSIDE THE GAME', 'ON 1.0? RUN THE UPDATER ONCE', 'Version 1.0.1', '+ IN-GAME UPDATES', 'A MASOCHISTIC\nEXPERIMENT', '22 YEARS\nIN THE MAKING', 'THE ORIGINAL', 'AND', 'EVEN', 'BETTER.', 'FREE & OPEN SOURCE', 'THE ORIGINAL GAME', 'INSIDE STEPMANIA', 'THE ORIGINAL GAMEPLAY',
         'SMOOTH ARROWS', 'KEYBOARD & DANCE PADS', 'TOUGH ENOUGH', 'GO WEST', 'KIDS IN AMERICA', 'UNLOCK IT',
         'HEAVEN AND EARTH', 'MICKEY', 'PC SETTINGS', 'AUTO TIMING', 'UP TO 1440p', 'BUILD NEW SONGS', 'PARANOIA SURVIVOR',
         'Dancing Stage Fusion PC', 'Now available on GitHub',
         'Version 1.1', 'ACHIEVEMENTS', 'YOUR RECORDS COUNT', 'NEW!!', 'TRACK YOUR PROGRESS', '+ NEW!! MARKER',
         'EARN AS YOU PLAY', 'UNLOCKED!', 'EVERY MODE COUNTS', '25 TO EARN', 'NEWLY UNLOCKED', 'KEPT ON YOUR PC',
         'PLAY.', 'EARN.', 'UNLOCK.', 'REPEAT.']
def slug(t):
    return re.sub(r'[^a-z0-9]+', '-', t.lower()).strip('-')
f = ImageFont.truetype(FONT, 200)
for t in TEXTS:
    sw, drop = 13, 20
    probe = ImageDraw.Draw(Image.new('L', (1, 1)))
    l, tp, r, b = probe.multiline_textbbox((0, 0), t, font=f, stroke_width=sw, align='center', spacing=10)
    W, H = int(r - l + 8), int(b - tp + drop + 8)
    im = Image.new('RGBA', (W, H))
    d = ImageDraw.Draw(im)
    o = (int(4 - l), int(4 - tp))
    d.multiline_text((o[0], o[1] + drop), t, font=f, fill=(216, 53, 42, 255), stroke_width=sw, stroke_fill=(216, 53, 42, 255), align='center', spacing=10)
    d.multiline_text(o, t, font=f, fill=(255, 255, 255, 255), stroke_width=sw, stroke_fill=(26, 5, 3, 255), align='center', spacing=10)
    im.save(OUT + slug(t) + '.png')
    print(slug(t), im.size)
