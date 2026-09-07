CC ?= cc
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic
LDFLAGS ?=

VERSION := 0.15.0
TARGET := wb
SRC := main.c modules/linux-core/linux_core.c modules/centos/centos.c modules/files-manager/files_manager.c modules/terminal/terminal.c modules/command-sets/command_sets.c
HEADERS := workbench.h modules/linux-core/linux_core.h modules/centos/centos.h modules/files-manager/files_manager.h modules/terminal/terminal.h modules/command-sets/command_sets.h
ACTION_DATA := $(wildcard modules/linux-core/actions/*.inc) $(wildcard modules/centos/actions/*.inc)
UI_DATA := modules/files-manager/files_ui.inc modules/command-sets/command_sets_ui.inc
TEST_SCRIPTS := \
	tests/test_selftest.sh \
	tests/test_module_layout.sh \
	tests/test_category_layout.sh \
	tests/test_catalogue_scale.sh \
	tests/test_template_syntax.sh \
	tests/test_template_safety.sh \
	tests/test_destructive_path_guard.sh \
	tests/test_module_registry.sh \
	tests/test_search_semantics.sh \
	tests/test_linux_profile.sh \
	tests/test_centos_overlay.sh \
	tests/test_profile_categories.sh \
	tests/test_profile_config_persistence.sh \
	tests/test_files_manager_ops.sh \
	tests/test_files_manager_safety.sh \
	tests/test_dual_input_structure.sh \
	tests/test_dual_input_safety.sh \
	tests/test_terminal_core.sh \
	tests/test_terminal_config.sh \
	tests/test_command_sets_core.sh \
	tests/test_command_sets_storage.sh \
	tests/test_command_sets_actions.sh \
	tests/test_release_metadata.sh \
	tests/test_release_policy.sh \
	tests/test_source_package.sh

.PHONY: all clean install test package package-only

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS) $(ACTION_DATA) $(UI_DATA)
	$(CC) $(CFLAGS) -I. $(SRC) -o $(TARGET) $(LDFLAGS)
	strip $(TARGET) 2>/dev/null || true

clean:
	rm -f $(TARGET)
	rm -rf dist/workbench-v$(VERSION) dist/workbench-v$(VERSION)-source.tar.gz

install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/wb

test: $(TARGET)
	@set -e; for t in $(TEST_SCRIPTS); do echo "== $$t =="; "$$t"; done
	python3 tests/smoke_tui.py
	python3 tests/smoke_files.py
	python3 tests/smoke_mouse_only_settings.py
	python3 tests/smoke_mouse_only_files.py
	python3 tests/smoke_virtual_keyboard.py
	python3 tests/smoke_dual_commands.py
	python3 tests/smoke_keyboard_only.py
	python3 tests/smoke_ui_preferences.py
	python3 tests/smoke_terminal.py
	python3 tests/smoke_terminal_mouse.py
	python3 tests/smoke_terminal_commands.py
	python3 tests/smoke_command_sets_hierarchy.py
	python3 tests/smoke_command_sets_crud.py
	python3 tests/smoke_command_sets_mouse.py
	python3 tests/smoke_terminal_command_sets.py
	python3 tests/smoke_terminal_settings.py
	python3 tests/smoke_terminal_clear.py

package: test package-only

package-only:
	rm -rf dist/workbench-v$(VERSION) dist/workbench-v$(VERSION)-source.tar.gz
	mkdir -p dist/workbench-v$(VERSION)
	cp main.c workbench.h Makefile README.md CHANGELOG.md LICENSE THIRD_PARTY_NOTICES.md DEPENDENCY_POLICY.md .gitignore dist/workbench-v$(VERSION)/
	cp -R modules tests docs dist/workbench-v$(VERSION)/
	tar -czf dist/workbench-v$(VERSION)-source.tar.gz -C dist workbench-v$(VERSION)
	@echo "Created dist/workbench-v$(VERSION)-source.tar.gz"
