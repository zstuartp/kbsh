# ---------- Project settings ----------
APP		?= kbsh
PACKAGE		?= kbsh
PACKAGE_NAME	?= kbsh
PACKAGE_BUGREPORT ?= parsons.zackary@gmail.com
PACKAGE_URL	?= https://github.com/zstuartp/kbsh/
PACKAGE_PACKAGER ?=
PACKAGE_PACKAGER_BUG_REPORTS ?=
VERSION_FILE	?= VERSION
VERSION		?= $(strip $(shell cat $(VERSION_FILE) 2>/dev/null))

ifeq ($(strip $(VERSION)),)
VERSION := 0.1.0-dev
endif

SRC_DIR		?= src
BUILD_DIR	?= build
OBJ_DIR		:= $(BUILD_DIR)/obj
BIN_DIR		:= $(BUILD_DIR)/bin
TARGET		:= $(BIN_DIR)/$(APP)
CONFIG_HEADER_IN ?= config.h.in
CONFIG_HEADER	:= $(BUILD_DIR)/config.h
CLEAN_STAMP	:= $(BUILD_DIR)/.make-created
PREFIX		?= /usr/local
BINDIR		?= $(PREFIX)/bin
DESTDIR		?=
LOCALEDIR	?= $(PREFIX)/share/locale
ENABLE_NLS	?= 0
TEST_POSIX_RUNNER ?= test/posix/run.sh
PROFILE		?= modern

UNAME_S		?= $(shell uname -s)
CC		?= cc
PKG_CONFIG	?= pkg-config

# ---------- Pretty output / verbosity ----------
V ?= 0
ifeq ($(V),1)
Q :=
define log
endef
else
Q := @
define log
@printf "  %-6s %s\n" "$(1)" "$(2)"
endef
endif

# ---------- Source discovery ----------
SRCS := $(shell find $(SRC_DIR) -type f -name '*.c' \
-not -path '*/$(BUILD_DIR)/*' -not -path '*/.git/*' \
-not -name 'arena_viz.c' \
-print | sed 's|^\./||')

OBJS := $(addprefix $(OBJ_DIR)/,$(SRCS:.c=.o))
DEPS := $(OBJS:.o=.d)

# ---------- Build flags ----------
ifeq ($(PROFILE),portable)
KBSH_STD_DEFAULT := c89
KBSH_POSIX_DEFAULT := 200112L
CPPFLAGS += -DKBSH_PORTABLE_PROFILE=1
else ifeq ($(PROFILE),modern)
KBSH_STD_DEFAULT := c99
KBSH_POSIX_DEFAULT := 200809L
CPPFLAGS += -DKBSH_MODERN_PROFILE=1
else
$(error Unsupported PROFILE='$(PROFILE)'; expected modern or portable)
endif

CSTD ?= $(KBSH_STD_DEFAULT)
POSIX_C_SOURCE ?= $(KBSH_POSIX_DEFAULT)

CPPFLAGS += -I$(BUILD_DIR) -I$(SRC_DIR) -D_POSIX_C_SOURCE=$(POSIX_C_SOURCE)

# ---------- Feature detection ----------
HAVE_POSIX_SPAWN := $(shell echo 'int x=0;' | $(CC) -D_POSIX_C_SOURCE=200112L -include spawn.h -x c -c - -o /dev/null 2>/dev/null && echo 1 || echo 0)
CFLAGS	 += -std=$(CSTD) -Wall -Wextra -Werror -pedantic

DEBUG ?= 0
ifeq ($(DEBUG),1)
CFLAGS += -O0 -g
else
CFLAGS += -O2
endif

SANITIZE ?=
ifneq ($(strip $(SANITIZE)),)
CFLAGS += -O1 -g -fno-omit-frame-pointer -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif

ifeq ($(ENABLE_NLS),1)
LDLIBS += -lintl
endif

ifneq ($(strip $(PACKAGE_PACKAGER)),)
CPPFLAGS += -DPACKAGE_PACKAGER='"$(PACKAGE_PACKAGER)"'
CPPFLAGS += -DPACKAGE_PACKAGER_BUG_REPORTS='"$(PACKAGE_PACKAGER_BUG_REPORTS)"'
endif

# ---------- Targets ----------
.PHONY: all clean install install-user uninstall uninstall-user run
.PHONY: test test-posix test-asan test-ubsan test-portable
.PHONY: print-vars version
.PHONY: .FORCE

all: $(TARGET)

$(TARGET): $(CONFIG_HEADER) $(OBJS) | $(BIN_DIR)
	$(call log,LD,$@)
	$(Q)$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

$(BIN_DIR):
	$(call log,MKDIR,$@)
	$(Q)mkdir -p $@
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)touch $(CLEAN_STAMP)

$(OBJ_DIR)/%.o: %.c $(CONFIG_HEADER)
	$(call log,CC,$<)
	$(Q)mkdir -p $(@D)
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)touch $(CLEAN_STAMP)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Stamp tracking config variable values; only changes when a value changes.
# .FORCE ensures the recipe always runs; cmp prevents a spurious timestamp
# update (and therefore a spurious config.h rebuild) when nothing changed.
$(BUILD_DIR)/.config-vars: .FORCE | $(BUILD_DIR)
	$(Q)printf '%s\n' \
	'PACKAGE=$(PACKAGE)' \
	'PACKAGE_NAME=$(PACKAGE_NAME)' \
	'PACKAGE_BUGREPORT=$(PACKAGE_BUGREPORT)' \
	'PACKAGE_URL=$(PACKAGE_URL)' \
	'VERSION=$(VERSION)' \
	'LOCALEDIR=$(LOCALEDIR)' \
	'ENABLE_NLS=$(ENABLE_NLS)' \
	'HAVE_POSIX_SPAWN=$(HAVE_POSIX_SPAWN)' \
	> $@.tmp
	$(Q)cmp -s $@.tmp $@ 2>/dev/null || mv $@.tmp $@
	$(Q)rm -f $@.tmp

$(CONFIG_HEADER): $(CONFIG_HEADER_IN) $(BUILD_DIR)/.config-vars | $(BUILD_DIR)
	$(call log,GEN,$@)
	$(Q)sed \
	-e 's|@PACKAGE@|$(PACKAGE)|g' \
	-e 's|@PACKAGE_NAME@|$(PACKAGE_NAME)|g' \
	-e 's|@PACKAGE_BUGREPORT@|$(PACKAGE_BUGREPORT)|g' \
	-e 's|@PACKAGE_URL@|$(PACKAGE_URL)|g' \
	-e 's|@VERSION@|$(VERSION)|g' \
	-e 's|@LOCALEDIR@|$(LOCALEDIR)|g' \
	-e 's|@ENABLE_NLS@|$(ENABLE_NLS)|g' \
	-e 's|@HAVE_POSIX_SPAWN@|$(HAVE_POSIX_SPAWN)|g' \
	"$(CONFIG_HEADER_IN)" > "$(CONFIG_HEADER).tmp"
	$(Q)mv "$(CONFIG_HEADER).tmp" "$(CONFIG_HEADER)"
	$(Q)touch $(CLEAN_STAMP)

$(BUILD_DIR):
	$(Q)mkdir -p "$(BUILD_DIR)"
	$(Q)touch $(CLEAN_STAMP)

-include $(DEPS)

# ---------- Safer clean ----------
clean:
	$(call log,CLEAN,$(BUILD_DIR))
	@set -eu; \
	dir="$(BUILD_DIR)"; \
	case "$$dir" in \
	""|"/"|"~"|"."|".."|"../"*|"/Users"|"/home"|"/root") \
	echo "Refusing to clean unsafe BUILD_DIR='$$dir'"; exit 1 ;; \
	esac; \
	if [ ! -f "$(CLEAN_STAMP)" ]; then \
	echo "Refusing to clean: missing stamp '$(CLEAN_STAMP)'."; \
	echo "Not created by this Makefile?"; \
	echo "If you're sure, run: make clean FORCE=1"; \
	if [ "$${FORCE:-0}" != "1" ]; then exit 1; fi; \
	fi; \
	rm -rf -- "$$dir"

install: $(TARGET)
	$(call log,INSTALL,$(DESTDIR)$(BINDIR)/$(APP))
	$(Q)set -eu; \
	dest="$(DESTDIR)$(BINDIR)"; \
	file="$$dest/$(APP)"; \
	if ! mkdir -p "$$dest"; then \
	echo "Install failed: cannot create '$$dest'."; \
	echo "Try: make install PREFIX=\"$${HOME:-/path/to/home}/.local\""; \
	echo "Or run with elevated permissions."; \
	exit 1; \
	fi; \
	if [ ! -w "$$dest" ]; then \
	echo "Install failed: '$$dest' is not writable."; \
	echo "Try: make install PREFIX=\"$${HOME:-/path/to/home}/.local\""; \
	echo "Or run with elevated permissions."; \
	exit 1; \
	fi; \
	install -m 0755 "$(TARGET)" "$$file"

install-user: $(TARGET)
	$(Q)set -eu; \
	if [ -z "$${HOME:-}" ]; then \
	echo "Install failed: HOME is not set."; \
	echo "Try: make install PREFIX=\"/path/to/prefix\""; \
	exit 1; \
	fi; \
	$(MAKE) --no-print-directory install PREFIX="$$HOME/.local"; \
	echo "Installed to $$HOME/.local/bin/$(APP)."; \
	echo "If needed, add '$$HOME/.local/bin' to PATH."

uninstall:
	$(call log,RM,$(DESTDIR)$(BINDIR)/$(APP))
	$(Q)set -eu; \
	dest="$(DESTDIR)$(BINDIR)"; \
	file="$$dest/$(APP)"; \
	case "$$dest" in \
	""|"/"|"//"|"///"*|"~"|"."|".."|"../"*|"/Users"|"/home"|"/root") \
	echo "Refusing unsafe uninstall directory '$$dest'."; \
	exit 1 ;; \
	esac; \
	case "$$file" in \
	""|"/"|"//"|"///"*|"~"|"."|"..") \
	echo "Refusing unsafe uninstall path '$$file'."; \
	exit 1 ;; \
	esac; \
	if [ -d "$$file" ]; then \
	echo "Refusing to uninstall directory '$$file'."; \
	exit 1; \
	fi; \
	if [ ! -e "$$file" ] && [ ! -L "$$file" ]; then \
	echo "Nothing to uninstall at '$$file'."; \
	exit 0; \
	fi; \
	rm -f -- "$$file"; \
	echo "Uninstalled '$$file'."

uninstall-user:
	$(Q)set -eu; \
	if [ -z "$${HOME:-}" ]; then \
	echo "Uninstall failed: HOME is not set."; \
	echo "Try: make uninstall PREFIX=\"/path/to/prefix\""; \
	exit 1; \
	fi; \
	$(MAKE) --no-print-directory uninstall PREFIX="$$HOME/.local"

run: $(TARGET)
	$(Q)./$(TARGET)

test: $(TARGET)
	$(call log,TEST,test/smoke)
	$(Q)set -eu; \
	hello="$$(./$(TARGET) test/smoke/hello.sh)"; \
	if [ "$$hello" != "Hello, World!" ]; then \
	echo "test failed: hello.sh output mismatch: $$hello"; \
	exit 1; \
	fi; \
	line="$$(./$(TARGET) test/smoke/line-continue.sh)"; \
	if [ "$$line" != "Hello, World!" ]; then \
	echo "test failed: line-continue.sh output mismatch: $$line"; \
	exit 1; \
	fi; \
	set +e; \
	./$(TARGET) test/smoke/unexpected-eof.sh >/dev/null 2>&1; \
	unexpected_status=$$?; \
	set -e; \
	if [ "$$unexpected_status" -eq 0 ]; then \
	echo "test failed: unexpected-eof.sh should fail"; \
	exit 1; \
	fi; \
	if [ "$$unexpected_status" -ge 128 ]; then \
	echo "test failed: unexpected-eof.sh crashed (status=$$unexpected_status)"; \
	exit 1; \
	fi; \
	pipe="$$(./$(TARGET) test/smoke/pipe.sh)"; \
	if [ "$$pipe" != "hello" ]; then \
	echo "test failed: pipe.sh output mismatch: $$pipe"; \
	exit 1; \
	fi; \
	cmdsub="$$(./$(TARGET) test/smoke/cmdsub.sh)"; \
	if [ "$$cmdsub" != "subshell" ]; then \
	echo "test failed: cmdsub.sh output mismatch: $$cmdsub"; \
	exit 1; \
	fi; \
	redir="$$(./$(TARGET) test/smoke/redir.sh)"; \
	if [ "$$redir" != "redir" ]; then \
	echo "test failed: redir.sh output mismatch: $$redir"; \
	exit 1; \
	fi; \
	echo "kbsh tests passed"

test-posix: $(TARGET)
	$(call log,TEST,$(TEST_POSIX_RUNNER))
	$(Q)ROOT="$(CURDIR)" TARGET="$(CURDIR)/$(TARGET)" \
	sh "$(TEST_POSIX_RUNNER)"

test-asan:
	$(call log,TEST,asan)
	$(Q)$(MAKE) --no-print-directory clean FORCE=1
	$(Q)$(MAKE) --no-print-directory DEBUG=1 SANITIZE=address all
	$(Q)$(MAKE) --no-print-directory DEBUG=1 SANITIZE=address test

test-ubsan:
	$(call log,TEST,ubsan)
	$(Q)$(MAKE) --no-print-directory clean FORCE=1
	$(Q)$(MAKE) --no-print-directory DEBUG=1 SANITIZE=undefined all
	$(Q)$(MAKE) --no-print-directory DEBUG=1 SANITIZE=undefined test

test-portable:
	$(call log,TEST,portable)
	$(Q)$(MAKE) --no-print-directory clean FORCE=1
	$(Q)$(MAKE) --no-print-directory PROFILE=portable all
	$(Q)$(MAKE) --no-print-directory PROFILE=portable test

version:
	@echo "$(VERSION)"

print-vars:
	@echo "APP=$(APP)"
	@echo "PACKAGE=$(PACKAGE)"
	@echo "PACKAGE_NAME=$(PACKAGE_NAME)"
	@echo "PACKAGE_PACKAGER=$(PACKAGE_PACKAGER)"
	@echo "VERSION=$(VERSION)"
	@echo "PREFIX=$(PREFIX)"
	@echo "BINDIR=$(BINDIR)"
	@echo "LOCALEDIR=$(LOCALEDIR)"
	@echo "ENABLE_NLS=$(ENABLE_NLS)"
	@echo "PROFILE=$(PROFILE)"
	@echo "CSTD=$(CSTD)"
	@echo "POSIX_C_SOURCE=$(POSIX_C_SOURCE)"
	@echo "SANITIZE=$(SANITIZE)"
	@echo "TEST_POSIX_RUNNER=$(TEST_POSIX_RUNNER)"
