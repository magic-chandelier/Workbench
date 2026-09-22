#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugin_command_sets.c modules/command-sets/command_sets.c -o /tmp/wb-test-plugin-command-sets
/tmp/wb-test-plugin-command-sets
rm -f /tmp/wb-test-plugin-command-sets
