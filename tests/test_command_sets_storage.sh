#!/bin/sh
set -eu
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_command_sets_storage.c modules/command-sets/command_sets.c -o /tmp/wb-test-command-sets-storage
/tmp/wb-test-command-sets-storage
rm -f /tmp/wb-test-command-sets-storage
