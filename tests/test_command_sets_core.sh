#!/bin/sh
set -eu
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_command_sets_core.c modules/command-sets/command_sets.c -o /tmp/wb-test-command-sets-core
/tmp/wb-test-command-sets-core
rm -f /tmp/wb-test-command-sets-core
