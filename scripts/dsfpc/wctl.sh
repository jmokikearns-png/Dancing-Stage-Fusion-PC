#!/usr/bin/env bash
# Drive the Win64 build under Wine/Xvfb step by step.
#   wctl.sh start RUN_DIR [EXE]     start (returns at once; PID in RUN_DIR/wine.pid)
#   wctl.sh key KEY [HOLD_MS]       hold a key on the game window (default 150 ms)
#   wctl.sh keys "K1 K2 ..." [GAP_S] several keys, GAP_S seconds apart (default 0.6)
#   wctl.sh shot FILE.png           screenshot of the X screen
#   wctl.sh stop RUN_DIR            kill the processes it started
set -u
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/dsf-xdg}
export DISPLAY=${DISPLAY:-:97} WINEPREFIX=${WINEPREFIX:-/home/user/work/builds/wine-dsf} WINEDEBUG=${WINEDEBUG:--all}
WIN=${WINDOW_NAME:-Dancing Stage Fusion PC}
focus() {
  local gw; gw=$(xdotool search --name "$WIN" 2>/dev/null | head -1)
  [ -n "$gw" ] && xdotool windowactivate --sync "$gw" 2>/dev/null
}
hold() {
  focus
  xdotool keydown --clearmodifiers "$1" 2>/dev/null
  sleep "$(awk "BEGIN{print ${2:-150}/1000}")"
  xdotool keyup "$1" 2>/dev/null
}
case "$1" in
start)
  RUN=$2; EXE=${3:-StepMania.exe}
  cd "$RUN" || exit 1
  EXEPATH=$EXE; [ -f "Program/$EXE" ] && EXEPATH="Program/$EXE"
  setsid wine "$EXEPATH" > "$RUN/wine-stdout.log" 2>&1 &
  echo $! > "$RUN/wine.pid"; echo "started $(cat "$RUN/wine.pid")" ;;
key) hold "$2" "${3:-150}" ;;
keys) for k in $2; do hold "$k" 150; sleep "${3:-0.6}"; done ;;
shot) import -window root "$2" 2>/dev/null && echo "$2" ;;
stop)
  RUN=$2
  [ -f "$RUN/wine.pid" ] && kill -- -"$(cat "$RUN/wine.pid")" 2>/dev/null
  wineserver -k 2>/dev/null; sleep 1; rm -f "$RUN/wine.pid"; echo stopped ;;
esac
