#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_plugin_file_actions.c modules/plugins/plugins.c modules/extensions/extensions.c -o /tmp/wb-test-plugin-file-actions
/tmp/wb-test-plugin-file-actions
rm -f /tmp/wb-test-plugin-file-actions
