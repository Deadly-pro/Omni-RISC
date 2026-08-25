#!/usr/bin/env bash
# R4 gate: scripted FreeRTOS shell session over honest-baud UART RX (R3).
# Feeds tests/shell_session.txt into tb_soc_shell at real 115200 framing and
# asserts on the decoded TX transcript. Exit 0 = all checks pass.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SESSION="$ROOT/tests/shell_session.txt"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

"$ROOT/hardware/sim/run_sim.sh" soc/tb_soc_shell < "$SESSION" > "$LOG" 2>/dev/null
rc=$?
if [ $rc -ne 0 ]; then
    echo "FAIL: simulation exited $rc"
    exit 1
fi

fail=0
has() {
    if grep -Eq "$1" "$LOG"; then echo "ok    $2"; else echo "FAIL  $2"; fail=1; fi
}
count() {
    n=$(grep -cE "$1" "$LOG")
    if [ "$n" -eq "$2" ]; then echo "ok    $3"; else echo "FAIL  $3 (saw $n, want $2)"; fail=1; fi
}

has '\[SHELL\] ready'                            'shell banner'
count 'commands: help uptime ps ticks gpu quit' 2 'help works, backspace edit (helq<BS>p) replays it'
has 'up [0-9]+\.[0-9]{1,3} s'                    'uptime prints seconds'
has '^console '                                'ps lists console task'
has '^IDLE '                                   'ps lists idle task'
has 'ticks n=[0-9]+ min=49[0-9]{3} max=50[0-9]{3}' 'tick jitter within +-1% of 50k'
has 'gpu sum=110 PASS'                         'GPU vector-add over MMIO'
has 'unknown: foo'                             'bad command rejected'
has '\[SHELL\] QUIT'                             'quit marker ends session'
has 'quit command\)'                           'TB exited on quit, not timeout'

if [ $fail -ne 0 ]; then
    echo "test_shell: FAILED"
    exit 1
fi
echo "test_shell: ALL PASS"
