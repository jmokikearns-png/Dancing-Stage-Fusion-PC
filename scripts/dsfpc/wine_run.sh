#!/usr/bin/env bash
# Run the Win64 StepMania build under Wine/Xvfb for smoke tests.
#   wine_run.sh RUN_DIR EXE_NAME SECONDS [SHOT_PREFIX]
#   KEYS="8:Down 9:Return" sends X key presses to the game window at those seconds
#   (each held HOLD_MS milliseconds, default 150)
# - dismisses "theme element is missing" dialogs with Ignore (logged by StepMania)
# - takes a screenshot every 5 s to SHOT_PREFIX-NN.png when SHOT_PREFIX is given
# Kills only the processes it started (by PID).
set -u
RUN=$1; EXE=$2; SECS=$3; SHOT=${4:-}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/dsf-xdg}
export DISPLAY=${DISPLAY:-:97} WINEPREFIX=${WINEPREFIX:-/home/user/work/builds/wine-dsf} WINEDEBUG=${WINEDEBUG:--all}
pgrep -x Xvfb >/dev/null || { Xvfb "$DISPLAY" -screen 0 1280x1024x24 >/dev/null 2>&1 & sleep 2; }
cd "$RUN" || exit 1
rm -f crashinfo.txt
EXEPATH=$EXE; [ -f "Program/$EXE" ] && EXEPATH="Program/$EXE"
wine "$EXEPATH" > "$RUN/wine-stdout.log" 2>&1 &
WPID=$!
n=0
for ((t=0; t<SECS; t++)); do
  sleep 1
  kill -0 $WPID 2>/dev/null || { echo "exited after ${t}s"; break; }
  for w in $(xdotool search --name '^StepMania$' 2>/dev/null); do
    # Dialog windows have a small size; the game window is the large one.
    geo=$(xdotool getwindowgeometry "$w" 2>/dev/null | awk '/Geometry/{print $2}')
    wdt=${geo%x*}
    if [ -n "$wdt" ] && [ "$wdt" -lt 600 ] 2>/dev/null; then
      xdotool windowactivate --sync "$w" key --clearmodifiers alt+i 2>/dev/null
    fi
  done
  for ev in ${KEYS:-}; do
    if [ "${ev%%:*}" = "$t" ]; then
      gw=$(xdotool search --name "${WINDOW_NAME:-Dancing Stage Fusion PC}" 2>/dev/null | head -1)
      [ -n "$gw" ] && xdotool windowactivate --sync "$gw" 2>/dev/null
      # Held for HOLD_MS (default 150): a bare tap can fall between two of the
      # game's 50 Hz pad reads.
      xdotool keydown --clearmodifiers "${ev#*:}" 2>/dev/null; sleep "$(awk "BEGIN{print ${HOLD_MS:-150}/1000}")"
      xdotool keyup "${ev#*:}" 2>/dev/null
      echo "t=${t}s key ${ev#*:}"
    fi
  done
  if [ -n "$SHOT" ] && (( t % 5 == 4 )); then
    import -window root "$(printf '%s-%02d.png' "$SHOT" $n)" 2>/dev/null; n=$((n+1))
  fi
done
# Close it the way a player would (Alt+F4) so StepMania flushes log.txt.
gw=$(xdotool search --name "${WINDOW_NAME:-Dancing Stage Fusion PC}" 2>/dev/null | head -1)
[ -n "$gw" ] && xdotool windowactivate --sync "$gw" key --clearmodifiers alt+F4 2>/dev/null
for ((i=0; i<10; i++)); do kill -0 $WPID 2>/dev/null || break; sleep 1; done
kill $WPID 2>/dev/null
sleep 1
wineserver -k 2>/dev/null
grep -h "could not be found\|is missing" "$RUN/log.txt" | sed 's/^[0-9:.]*: //' | sort -u
[ -f crashinfo.txt ] && { echo "--- crashinfo.txt"; grep -E "reason|Assert|^0000" crashinfo.txt | head -20; }
exit 0
