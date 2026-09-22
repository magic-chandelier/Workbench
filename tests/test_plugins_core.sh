#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugins_core.c modules/plugins/plugins.c -o /tmp/wb-test-plugins-core
/tmp/wb-test-plugins-core
rm -f /tmp/wb-test-plugins-core
