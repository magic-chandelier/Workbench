#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugin_enable_disable.c modules/plugins/plugins.c -o /tmp/wb-test-plugin-enable-disable
/tmp/wb-test-plugin-enable-disable
rm -f /tmp/wb-test-plugin-enable-disable
