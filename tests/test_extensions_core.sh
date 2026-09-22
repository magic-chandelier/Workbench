#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I. tests/test_extensions_core.c modules/extensions/extensions.c modules/plugins/plugins.c -o /tmp/wb-test-extensions-core
/tmp/wb-test-extensions-core
rm -f /tmp/wb-test-extensions-core
