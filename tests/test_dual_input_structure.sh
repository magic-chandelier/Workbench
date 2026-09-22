#!/bin/sh
set -eu
grep -q 'typedef struct .*UiEvent' main.c || grep -q '} UiEvent;' main.c
grep -q 'ui_read_event' main.c
grep -q 'UiHitRegion' main.c
grep -q 'ui_hit_add' main.c
grep -q 'ui_hit_test' main.c
