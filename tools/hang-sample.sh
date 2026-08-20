#!/bin/bash
# hang-sample.sh — stack-sample the running/hung gamemd.
#
# USAGE (run from a TERMINAL so sudo can ask for your password once):
#   ./hang-sample.sh          # 2 samples  (hang/black-screen forensics)
#   ./hang-sample.sh 15       # 15 samples (lag/stutter profiling: run DURING
#                             #   gameplay; samples land inside the hitches)
# THEN: send the saved ~/Desktop/hang-*.txt file to Claude.
#
# Reading it: identical stacks across samples = deadlock; changing leaf
# addresses = live code. 0x004xxxxx-0x007xxxxx = gamemd, 0x778Cxxxx+ = Phobos.

SAMPLES=${1:-2}

# Find the real game process: pgrep -f also matches Syringe.exe (the game's
# name appears in its arguments), so filter by the process's own comm name.
PID=""
for p in $(pgrep -f 'gamemd-spawn.exe'); do
    comm=$(cat /proc/$p/comm 2>/dev/null)
    case "$comm" in
        gamemd*) PID=$p; break ;;
    esac
done
if [ -z "$PID" ]; then
    echo "gamemd is not running (only found: $(pgrep -af gamemd-spawn.exe | awk '{print $1": "$2}' | tr '\n' ' '))"
    exit 1
fi

# sudo needs a way to ask for the password. In a terminal it just prompts;
# without one (double-click launch) fall back to pkexec's GUI dialog.
SUDO="sudo"
if ! [ -t 0 ]; then
    if command -v pkexec >/dev/null; then SUDO="pkexec"; else
        echo "No terminal for the sudo password. Run this from a terminal."
        exit 1
    fi
fi
# Warm up sudo once so the sampling loop is un-prompted.
$SUDO true || exit 1

OUT=~/Desktop/hang-$(date +%Y%m%d-%H%M%S).txt
{
    echo "=== $SAMPLES sample(s) of gamemd PID $PID ($(cat /proc/$PID/comm)) at $(date) ==="
    grep -E 'Name|State|Threads' /proc/$PID/status
    echo "--- module map (names the DLL owning any 0x7xxxxxxx sample) ---"
    grep -iE '\.(exe|dll)' /proc/$PID/maps | awk '{print $1" "$6}' | sort -u
    for i in $(seq 1 "$SAMPLES"); do
        echo "=== gdb stack sample #$i / $SAMPLES ==="
        $SUDO gdb -p "$PID" -batch \
            -ex 'set pagination off' \
            -ex 'thread apply all bt 8' \
            -ex detach 2>/dev/null | grep -vE '^\[|New LWP|warning:|Reading|Download'
        [ "$i" -lt "$SAMPLES" ] && sleep 0.4
    done
} | tee "$OUT"
echo
echo ">>> saved to $OUT — send this file to Claude <<<"
