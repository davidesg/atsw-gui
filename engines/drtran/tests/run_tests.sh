#!/bin/sh
# run_tests.sh -- drtran's battery (test_battery.sh), as `make check` at the
# root runs it. It takes a few minutes: it estimates the m6 network several
# times.
cd "$(dirname "$0")/.." || exit 2
bash test_battery.sh
