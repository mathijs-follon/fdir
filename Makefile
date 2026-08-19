COMPILER    ?= gcc
ARCHIVE     ?= ar
CFLAGS      ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LINKERFLAGS ?=
LIB_NAME    ?= fdir

ROOT     := $(abspath .)
CPPFLAGS ?= -I$(ROOT)/include -I$(ROOT)/src -I$(ROOT)/tests

SOURCES      := $(wildcard $(ROOT)/src/*.c)
OBJECTS      := $(SOURCES:$(ROOT)/src/%.c=$(ROOT)/build/obj/%.o)
TEST_SOURCES := $(filter-out $(ROOT)/tests/test_port.c,$(wildcard $(ROOT)/tests/*.c))
LIBRARY      := $(ROOT)/build/lib$(LIB_NAME).a

# FreeRTOS kernel
FRTOS_ROOT   := $(ROOT)/examples/FreeRTOS/FreeRTOS-Kernel
FRTOS_PORT   := $(FRTOS_ROOT)/portable/ThirdParty/GCC/Posix
FRTOS_KERNEL_SRCS := \
    $(FRTOS_ROOT)/tasks.c \
    $(FRTOS_ROOT)/list.c \
    $(FRTOS_ROOT)/queue.c \
    $(FRTOS_ROOT)/timers.c \
    $(FRTOS_ROOT)/event_groups.c \
    $(FRTOS_PORT)/port.c \
    $(FRTOS_PORT)/utils/wait_for_event.c \
    $(FRTOS_ROOT)/portable/MemMang/heap_4.c

FRTOS_C_INC    := -I$(ROOT)/include \
                  -I$(ROOT)/examples/FreeRTOS \
                  -I$(FRTOS_ROOT)/include \
                  -I$(FRTOS_PORT)
FRTOS_C_CFLAGS := -std=c11 -Wall -Wextra -O2 -D_GNU_SOURCE -Wno-builtin-macro-redefined
FRTOS_C_SRCS   := $(ROOT)/examples/FreeRTOS/main.c \
                  $(ROOT)/examples/FreeRTOS/port.c

# filecopy
FCOPY_C_INC    := -I$(ROOT)/include -I$(ROOT)/examples/filecopy
FCOPY_C_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -D_GNU_SOURCE
FCOPY_C_SRCS   := $(ROOT)/examples/filecopy/main.c \
                  $(ROOT)/examples/filecopy/worker.c \
                  $(ROOT)/examples/filecopy/port.c

# getting_started
GS_C_SRCS  := $(ROOT)/examples/getting_started/main.c

# dual_path
DP_C_SRCS := $(ROOT)/examples/dual_path/main.c

# Sources for compile_commands.json (FreeRTOS kernel internals excluded).
PLAIN_SRCS := $(SOURCES) $(TEST_SOURCES) $(GS_C_SRCS) $(DP_C_SRCS) \
              $(FCOPY_C_SRCS) $(FRTOS_C_SRCS)

.PHONY: all clean test examples \
        getting_started dual_path filecopy freertos \
        compile_commands.json

# Default target must stay first (bare `make` builds the library).
all: $(LIBRARY) compile_commands.json

examples: getting_started dual_path filecopy freertos

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(ROOT)/build/obj
	$(ARCHIVE) rcs $@ $^

$(ROOT)/build/obj/%.o: $(ROOT)/src/%.c
	@mkdir -p $(ROOT)/build/obj
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: all
	@mkdir -p $(ROOT)/build/tests
	@failed=0; \
	for src in $(TEST_SOURCES); do \
		name=$$(basename $$src .c); \
		bin=$(ROOT)/build/tests/$$name; \
		$(COMPILER) $(CPPFLAGS) $(CFLAGS) $$src $(ROOT)/tests/test_port.c $(LIBRARY) $(LINKERFLAGS) -lpthread -o $$bin || { failed=1; continue; }; \
		$$bin || failed=1; \
	done; \
	exit $$failed

getting_started: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(GS_C_SRCS) $(LIBRARY) \
		-o $(ROOT)/build/getting_started

dual_path: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(DP_C_SRCS) $(LIBRARY) \
		-o $(ROOT)/build/dual_path

filecopy: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FCOPY_C_INC) $(FCOPY_C_CFLAGS) $(FCOPY_C_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/fcopy

freertos: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FRTOS_C_INC) $(FRTOS_C_CFLAGS) \
		$(FRTOS_C_SRCS) $(FRTOS_KERNEL_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/example_FreeRTOS

compile_commands.json: Makefile
	@echo '[' > $@.tmp
	@first=1; \
	emit() { \
		if [ $$first -eq 0 ]; then printf ',\n' >> $@.tmp; fi; \
		first=0; \
		printf '{\n  "directory": "%s",\n  "file": "%s",\n  "command": "%s %s %s -c %s"\n}' \
			"$(ROOT)" "$$1" "$$4" "$$2" "$$3" "$$1" >> $@.tmp; \
	}; \
	for f in $(PLAIN_SRCS); do \
		case "$$f" in \
			*examples/filecopy/*) emit "$$f" "$(FCOPY_C_INC)" "$(FCOPY_C_CFLAGS)" "$(COMPILER)" ;; \
			*examples/FreeRTOS/*) emit "$$f" "$(FRTOS_C_INC)" "$(FRTOS_C_CFLAGS)" "$(COMPILER)" ;; \
			*) emit "$$f" "$(CPPFLAGS)" "$(CFLAGS)" "$(COMPILER)" ;; \
		esac; \
	done
	@printf '\n]\n' >> $@.tmp
	@mv $@.tmp $@
	@echo "generated $@"

clean:
	rm -rf $(ROOT)/build compile_commands.json
