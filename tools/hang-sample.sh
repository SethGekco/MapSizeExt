#!/bin/bash
# hang-sample.sh — capture stack samples of a hung (black-screened) gamemd.
#
# WHEN: the game black-screens/freezes (no Fatal Error dialog, no crash dump).
# RUN:  ./hang-sample.sh   (from any terminal, BEFORE killing the game)
# THEN: send the saved ~/Desktop/hang-*.txt file to Claude.
#
# Takes TWO gdb samples 3 s apart: identical stacks = deadlock/blocked;
# changing leaf addresses = a spin loop (and the addresses name the loop:
# 0x004xxxxx-0x007xxxxx = gamemd, 0x778Cxxxx+ = Phobos.dll, etc.).
# Needs sudo because kernel.yama.ptrace_scope=1 restricts attaching.

PID=$(pgrep -f 'gamemd-spawn.exe' | head -1)
if [ -z "$PID" ]; then
    echo "gamemd-spawn.exe is not running (nothing to sample)."
    exit 1
fi
OUT=~/Desktop/hang-$(date +%Y%m%d-%H%M%S).txt
{
    echo "=== hang sample of gamemd PID $PID at $(date) ==="
    grep -E 'Name|State|Threads' /proc/$PID/status
    echo "--- thread states / wait channels ---"
    for t in /proc/$PID/task/*; do
        tid=$(basename "$t")
        st=$(awk '{print $3}' "$t/stat" 2>/dev/null)
        wc=$(cat "$t/wchan" 2>/dev/null)
        echo "tid $tid: state=$st wchan=$wc"
    done
    for pass in 1 2; do
        echo "=== gdb stack sample #$pass ==="
        sudo gdb -p "$PID" -batch \
            -ex 'set pagination off' \
            -ex 'info threads' \
            -ex 'thread apply all bt 10' \
            -ex detach 2>&1
        [ "$pass" = 1 ] && sleep 3
    done
} | tee "$OUT"
echo
echo ">>> saved to $OUT — send this file to Claude <<<"
