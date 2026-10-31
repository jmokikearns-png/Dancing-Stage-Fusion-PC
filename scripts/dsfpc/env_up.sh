#!/usr/bin/env bash
# Start the headless test display (Xvfb :97) and a PulseAudio null sink that
# Wine's DirectSound plays into (recordable from nullsink.monitor).  Idempotent.
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/dsf-xdg}
mkdir -p "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
pgrep -x Xvfb >/dev/null || { Xvfb :97 -screen 0 1280x1024x24 >/dev/null 2>&1 & }
if ! pactl info >/dev/null 2>&1; then
  pulseaudio --daemonize=yes --exit-idle-time=-1 -n \
    --load="module-null-sink sink_name=nullsink rate=44100" \
    --load="module-native-protocol-unix" --log-target=file:"$XDG_RUNTIME_DIR/pa.log"
fi
sleep 2
pgrep -x Xvfb >/dev/null && echo "Xvfb up"; pactl list short sinks 2>/dev/null | grep -q nullsink && echo "null sink up"
