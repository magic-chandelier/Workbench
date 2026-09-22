#!/bin/sh
set -eu
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_command_sets_actions.c modules/command-sets/command_sets.c -o /tmp/wb-test-command-sets-actions
/tmp/wb-test-command-sets-actions
rm -f /tmp/wb-test-command-sets-actions
