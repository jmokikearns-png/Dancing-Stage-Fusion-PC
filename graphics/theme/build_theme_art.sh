#!/usr/bin/env bash
# Render the DSF PC StepMania theme's graphics and font from the design
# tokens (Remotion) straight into the theme, and write the actor files that
# use them.  build_theme_art.sh ENGINE_DIR   (the folder with Themes/ and Data/)
#
# The title logo is our own design (ThemeTitleLogoOwn) unless the player supplies the game's
# logo: LOGO=their-logo.png (400x175 art, or any multiple).  It is then
# upscaled 4x (ESRGAN_MODEL=RealESRGAN_x4plus_anime_6B.pth, see
# upscale/esrgan_np.py; without it the logo is used as it is), traced into
# clean vector layers (upscale/vectorize_logo.py, needs potrace and scipy) and
# redrawn by Remotion with "PC" after "Fusion".  The game's logo is Konami's: never commit the result,
# so with LOGO the target may not be inside a git work tree.
set -euo pipefail
G=$(cd "$(dirname "$0")/.." && pwd)
E=$1
T="$E/Themes/DSF PC"
BROWSER=${BROWSER:-/opt/pw-browsers/chromium_headless_shell-1194/chrome-linux/headless_shell}
still() { (cd "$G" && npx remotion still src/index.ts "$1" "$2" --browser-executable="$BROWSER" --image-format=png --log=error "${@:3}"); }

LOGO=${LOGO:-}
if [ -n "$LOGO" ] && git -C "$T" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	echo "build_theme_art.sh: $T is in a git work tree; the game's logo may not go there (use the game folder)" >&2
	exit 1
fi

"$G/fonts/build_font.sh" "$T"

mkdir -p "$T/Graphics" "$T/BGAnimations/ScreenWithMenuElements background" "$T/BGAnimations/ScreenTitleMenu background"
still ThemeHeader "$T/Graphics/_shared menu header (res 640x44).png"
still ThemeFooter "$T/Graphics/_shared footer (res 640x32).png"
still ThemeOptionsPage "$T/Graphics/ScreenOptions page (res 640x480).png"
# (the logo's layers may not start with "ScreenTitleMenu logo": StepMania
# would take them for the element itself, beside the xml)
if [ -z "$LOGO" ]; then
	still ThemeTitleLogoOwn "$T/Graphics/_title logo main (res 374x176).png"
	still ThemeTitleLogoPC "$T/Graphics/_title logo pc (res 126x104).png"
fi
still ThemeBackground "$T/BGAnimations/ScreenWithMenuElements background/bg (res 640x480).png"
still ThemeTitleBackground "$T/BGAnimations/ScreenTitleMenu background/bg (res 640x480).png"
still ThemeSplash "$E/Data/splash.png"
still ThemeInfoCursor "$T/Graphics/ScreenPS1Info cursor (res 160x28).png"

for d in "ScreenWithMenuElements background" "ScreenTitleMenu background"; do
	printf '[Layer1]\r\nFile=bg (res 640x480).png\r\nType=sprite\r\nStretch=1\r\n' > "$T/BGAnimations/$d/BGAnimation.ini"
done

if [ -z "$LOGO" ]; then
	# our own logo (ThemeArt.tsx ThemeTitleLogoOwn): it drops in, then "PC" stamps on
	rm -f "$T/Graphics/ScreenTitleMenu logo "*.png "$T/Graphics/_title logo dsf "*.png "$T/Graphics/_title logo (res 640x120).png"
	{
		printf '<ActorFrame>\r\n<children>\r\n'
		printf '\t<Layer1 Class="Sprite" Texture="_title logo main (res 374x176).png" OnCommand="zoom,1.6;diffusealpha,0;decelerate,0.35;zoom,1;diffusealpha,1" />\r\n'
		printf '\t<Layer2 Class="Sprite" Texture="_title logo pc (res 126x104).png" OnCommand="x,70;y,14;diffusealpha,0;zoom,4;sleep,0.4;diffusealpha,1;accelerate,0.14;zoom,0.85;decelerate,0.12;zoom,1.08;accelerate,0.08;zoom,1;glowshift;effectperiod,2.5;effectcolor1,1,1,1,0;effectcolor2,1,1,0.6,0.35" />\r\n'
		printf '</children>\r\n</ActorFrame>\r\n'
	} > "$T/Graphics/ScreenTitleMenu logo.xml"
else
	PUB=$(mktemp -d)
	trap 'rm -rf "${PUB:?}"' EXIT
	if [ -n "${ESRGAN_MODEL:-}" ]; then
		python3 "$G/upscale/esrgan_np.py" "$ESRGAN_MODEL" "$LOGO" "$PUB/dsf-logo.png"
	else
		cp "$LOGO" "$PUB/dsf-logo.png"
	fi
	python3 "$G/upscale/vectorize_logo.py" "$PUB/dsf-logo.png" "$PUB/dsf-logo-vector.json"
	# canvas sizes and "PC"'s place, from the one layout file
	read -r LW LH PW PH PX PY < <(python3 - "$G/src/logoLayout.json" <<'PY'
import json, sys
L = json.load(open(sys.argv[1]))
w = L['cropW'] + L['padR']
h = L['logoH'] + L['logoH'] % 2	# even, as ThemeArt.tsx logoCanvas
print(w, h, L['pcBoxW'], L['pcBoxH'],
      L['pcX'] - L['cropX'] + L['pcBoxW'] / 2 - 12 - w / 2, L['pcY'] - h / 2)
PY
)
	rm -f "$T/Graphics/_title logo (res 640x120).png" "$T/Graphics/_title logo main "*.png "$T/Graphics/ScreenTitleMenu logo "*.png "$T/Graphics/_title logo dsf "*.png "$T/Graphics/_title logo pc "*.png
	still ThemeTitleLogoDSFArt "$T/Graphics/_title logo dsf (res ${LW}x${LH}).png" --public-dir="$PUB"
	still ThemeTitleLogoPC "$T/Graphics/_title logo pc (res ${PW}x${PH}).png" --public-dir="$PUB"
	# the logo drops in; "PC" stamps on after it, then glints now and then
	{
		printf '<ActorFrame>\r\n<children>\r\n'
		printf '\t<Layer1 Class="Sprite" Texture="_title logo dsf (res %sx%s).png" OnCommand="zoom,1.6;diffusealpha,0;decelerate,0.35;zoom,1;diffusealpha,1" />\r\n' "$LW" "$LH"
		printf '\t<Layer2 Class="Sprite" Texture="_title logo pc (res %sx%s).png" OnCommand="x,%s;y,%s;diffusealpha,0;zoom,4;sleep,0.4;diffusealpha,1;accelerate,0.14;zoom,0.85;decelerate,0.12;zoom,1.08;accelerate,0.08;zoom,1;glowshift;effectperiod,2.5;effectcolor1,1,1,1,0;effectcolor2,1,1,0.6,0.35" />\r\n' "$PW" "$PH" "$PX" "$PY"
		printf '</children>\r\n</ActorFrame>\r\n'
	} > "$T/Graphics/ScreenTitleMenu logo.xml"
fi

# Each menu screen's header: the bar, with the screen's name on its plate.
header() {
	printf '<ActorFrame>\r\n<children>\r\n\t<Layer1 Class="Sprite" Texture="_shared menu header (res 640x44).png" />\r\n' > "$T/Graphics/$1 header.xml"
	if [ -n "$2" ]; then
		printf '\t<Layer2 Class="BitmapText" Font="Common Normal" Text="%s" OnCommand="y,-1;zoom,0.82;diffuse,1,0.85,0.3,1;shadowlength,0" />\r\n' "$2" >> "$T/Graphics/$1 header.xml"
	fi
	printf '</children>\r\n</ActorFrame>\r\n' >> "$T/Graphics/$1 header.xml"
}
header ScreenWithMenuElements ""
header ScreenOptionsMenu "OPTIONS"
header ScreenSoundOptions "SOUND OPTIONS"
header ScreenGraphicOptions "GRAPHIC OPTIONS"
header ScreenMapControllers "KEY / JOY MAPPINGS"
header ScreenTestInput "TEST INPUT"
header ScreenPS1Pause "PC MENU"
header ScreenPS1Info "INFO"
header ScreenPS1Welcome "WELCOME"
echo "theme art written to $T"
