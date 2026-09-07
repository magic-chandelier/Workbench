#!/bin/sh
set -eu

# Interactive prompts must stay inside the unified raw-event/input layer.
if grep -RInE 'fgets\([^,]+,[^,]+,stdin\)|getchar\(' main.c modules/files-manager/files_ui.inc; then
    echo 'interactive stdio prompt detected' >&2
    exit 1
fi

grep -q 'ui_input_dialog' main.c
grep -q 'ui_confirm_dialog' main.c
grep -q 'UiEvent' main.c
grep -q 'UiHitRegion' main.c
grep -q 'smoke_mouse_only_settings.py' Makefile
grep -q 'smoke_mouse_only_files.py' Makefile
grep -q 'smoke_keyboard_only.py' Makefile
