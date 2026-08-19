COMPILER    ?= gcc
ARCHIVE     ?= ar
CFLAGS      ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LINKERFLAGS ?=
LIB_NAME    ?= fdir

ROOT     := $(abspath .)
CPPFLAGS ?= -I$(ROOT)/include -I$(ROOT)/src -I$(ROOT)/tests

SOURCES      := $(wildcard $(ROOT)/src/*.c)
OBJECTS      := $(SOURCES:$(ROOT)/src/%.c=$(ROOT)/build/obj/%.o)
TEST_SOURCES := $(wildcard $(ROOT)/tests/*.c)
LIBRARY      := $(ROOT)/build/lib$(LIB_NAME).a

# FreeRTOS example (Posix port, Linux host)
FRTOS_ROOT   := $(ROOT)/examples/FreeRTOS/FreeRTOS-Kernel
FRTOS_PORT   := $(FRTOS_ROOT)/portable/ThirdParty/GCC/Posix
FRTOS_INC    := -I$(ROOT)/include \
                -I$(ROOT)/examples/FreeRTOS \
                -I$(FRTOS_ROOT)/include \
                -I$(FRTOS_PORT)
FRTOS_CFLAGS := -std=c11 -Wall -Wextra -O2 -D_GNU_SOURCE -Wno-builtin-macro-redefined
FRTOS_SRCS   := $(ROOT)/examples/FreeRTOS/main.c \
                $(ROOT)/examples/FreeRTOS/port.c \
                $(FRTOS_ROOT)/tasks.c \
                $(FRTOS_ROOT)/list.c \
                $(FRTOS_ROOT)/queue.c \
                $(FRTOS_ROOT)/timers.c \
                $(FRTOS_ROOT)/event_groups.c \
                $(FRTOS_PORT)/port.c \
                $(FRTOS_PORT)/utils/wait_for_event.c \
                $(FRTOS_ROOT)/portable/MemMang/heap_4.c

# filecopy example
FCOPY_INC    := -I$(ROOT)/include -I$(ROOT)/examples/filecopy
FCOPY_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -D_GNU_SOURCE
FCOPY_SRCS   := $(ROOT)/examples/filecopy/main.c \
                $(ROOT)/examples/filecopy/worker.c \
                $(ROOT)/examples/filecopy/port.c

# Sources for compile_commands.json, grouped by their build flags.
# FreeRTOS kernel internals are excluded (not edited here).
PLAIN_SRCS := $(SOURCES) $(TEST_SOURCES) \
              $(ROOT)/examples/getting_started/main.c
FRTOS_EDIT_SRCS := $(ROOT)/examples/FreeRTOS/main.c \
                   $(ROOT)/examples/FreeRTOS/port.c

.PHONY: all clean test getting_started filecopy freertos compile_commands.json

all: $(LIBRARY) compile_commands.json

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(ROOT)/build/obj
	$(ARCHIVE) rcs $@ $^

$(ROOT)/build/obj/%.o: $(ROOT)/src/%.c
	@mkdir -p $(ROOT)/build/obj
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: all
	@mkdir -p $(ROOT)/build/obj
	@if [ -z "$(TEST_SOURCES)" ]; then \
		echo "No test files found in $(ROOT)/tests/"; \
	else \
		$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LIBRARY) $(LINKERFLAGS) -o $(ROOT)/build/test_runner && \
		$(ROOT)/build/test_runner; \
	fi

getting_started: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(ROOT)/examples/getting_started/main.c $(LIBRARY) \
		-o $(ROOT)/build/getting_started

filecopy: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FCOPY_INC) $(FCOPY_CFLAGS) $(FCOPY_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/fcopy

freertos: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FRTOS_INC) $(FRTOS_CFLAGS) $(FRTOS_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/example_FreeRTOS

compile_commands.json: Makefile
	@echo '[' > $@.tmp
	@first=1; \
	emit() { \
		if [ $$first -eq 0 ]; then printf ',\n' >> $@.tmp; fi; \
		first=0; \
		printf '{\n  "directory": "%s",\n  "file": "%s",\n  "command": "%s %s %s -c %s"\n}' \
			"$(ROOT)" "$$1" "$(COMPILER)" "$$2" "$$3" "$$1" >> $@.tmp; \
	}; \
	for f in $(PLAIN_SRCS); do emit "$$f" "$(CPPFLAGS)" "$(CFLAGS)"; done; \
	for f in $(FCOPY_SRCS); do emit "$$f" "$(FCOPY_INC)" "$(FCOPY_CFLAGS)"; done; \
	for f in $(FRTOS_EDIT_SRCS); do emit "$$f" "$(FRTOS_INC)" "$(FRTOS_CFLAGS)"; done
	@printf '\n]\n' >> $@.tmp
	@mv $@.tmp $@
	@echo "generated $@"

clean:
	rm -rf $(ROOT)/build compile_commands.json
