PROJECT_NAME = gurpil
FAP_APPID = flipper_gurpil

# Local override: create a gitignored `local.mk` with your real path, e.g.
#   FLIPPER_FIRMWARE_PATH = /home/you/flipperzero-firmware
# The committed default below is a placeholder on purpose — never commit a real path.
-include local.mk
FLIPPER_FIRMWARE_PATH ?= <Path>/flipperzero-firmware
PWD = $(shell pwd)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

.PHONY: all help test prepare fap clean clean_firmware format linter

all: test

help:
	@echo "Targets for $(PROJECT_NAME):"
	@echo "  make test           - Host unit tests (domain logic)"
	@echo "  make prepare        - Symlink app into firmware applications_user"
	@echo "  make fap            - Clean firmware build + compile .fap"
	@echo "  make format         - clang-format"
	@echo "  make linter         - cppcheck"
	@echo "  make clean          - Remove local objects"
	@echo "  make clean_firmware - rm firmware build dir"

# --- host tests: compile domain + test TU with gcc, run the binary ---
# Each tests/test_*.c is its own suite with its own `main`, so each links into its own
# binary (test_gurpil_<suite>); `make test` runs all of them. Add a new suite by appending
# its .o files to OBJS_<SUITE> and its binary to TEST_BINS, mirroring the pairs below.
OBJS_SMOKE = version_info.o test_smoke.o
OBJS_SHAPES = shapes.o test_shapes.o
TEST_BINS = test_gurpil_smoke test_gurpil_shapes

test: $(TEST_BINS)
	./test_gurpil_smoke
	./test_gurpil_shapes

test_gurpil_smoke: $(OBJS_SMOKE)
	$(CC) $(CFLAGS) -o test_gurpil_smoke $(OBJS_SMOKE)

test_gurpil_shapes: $(OBJS_SHAPES)
	$(CC) $(CFLAGS) -o test_gurpil_shapes $(OBJS_SHAPES)

version_info.o: src/domain/version_info.c include/domain/version_info.h
	$(CC) $(CFLAGS) -c src/domain/version_info.c -o version_info.o

test_smoke.o: tests/test_smoke.c include/domain/version_info.h
	$(CC) $(CFLAGS) -c tests/test_smoke.c -o test_smoke.o

shapes.o: src/domain/shapes.c include/domain/shapes.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c src/domain/shapes.c -o shapes.o

test_shapes.o: tests/test_shapes.c include/domain/shapes.h include/domain/terrain_kind.h
	$(CC) $(CFLAGS) -c tests/test_shapes.c -o test_shapes.o

# --- format / lint ---
FORMAT_FILES := $(shell git ls-files '*.c' '*.h' 2>/dev/null)
ifeq ($(strip $(FORMAT_FILES)),)
FORMAT_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.h' \) ! -path './.git/*' | sort)
endif

format:
	clang-format -i $(FORMAT_FILES)

linter:
	cppcheck --enable=all --inline-suppr -I. \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction:main.c \
		src/domain/version_info.c src/domain/shapes.c src/app/gurpil_app.c main.c \
		tests/test_smoke.c tests/test_shapes.c

# --- build the .fap via the firmware tree (ufbt/fbt; not available in this sandbox) ---
prepare:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		mkdir -p $(FLIPPER_FIRMWARE_PATH)/applications_user; \
		ln -sfn $(PWD) $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME); \
		echo "Linked to $(FLIPPER_FIRMWARE_PATH)/applications_user/$(PROJECT_NAME)"; \
	else \
		echo "Firmware not found at $(FLIPPER_FIRMWARE_PATH)"; \
	fi

clean_firmware:
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)/build" ]; then \
		rm -rf $(FLIPPER_FIRMWARE_PATH)/build; \
	fi

fap: prepare clean_firmware clean
	@if [ -d "$(FLIPPER_FIRMWARE_PATH)" ]; then \
		cd $(FLIPPER_FIRMWARE_PATH) && ./fbt fap_$(FAP_APPID); \
	fi

clean:
	rm -f *.o tests/*.o test_gurpil test_gurpil_smoke test_gurpil_shapes
