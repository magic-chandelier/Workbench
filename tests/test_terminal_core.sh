#!/bin/sh
set -eu
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_terminal_core.c modules/terminal/terminal.c -o /tmp/wb-terminal-core-test
/tmp/wb-terminal-core-test
rm -f /tmp/wb-terminal-core-test
