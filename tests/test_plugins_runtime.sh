#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugins_runtime.c modules/plugins/plugins.c -o /tmp/wb-test-plugins-runtime
/tmp/wb-test-plugins-runtime
rm -f /tmp/wb-test-plugins-runtime
