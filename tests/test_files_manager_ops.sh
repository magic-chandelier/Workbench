#!/bin/sh
set -eu
cc -O2 -pipe -Wall -Wextra -Wpedantic -I. tests/test_files_manager_ops.c modules/files-manager/files_manager.c -o /tmp/wb-test-files-manager
/tmp/wb-test-files-manager
rm -f /tmp/wb-test-files-manager
