#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugin_previews.c modules/plugins/plugins.c modules/extensions/extensions.c -o /tmp/wb-test-plugin-previews
/tmp/wb-test-plugin-previews
rm -f /tmp/wb-test-plugin-previews
