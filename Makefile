COMPILER    ?= gcc
ARCHIVE     ?= ar
CFLAGS      ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LINKERFLAGS ?=
LIB_NAME    ?= fdir

CXX         ?= g++
CXXFLAGS    ?= -std=c++20 -Wall -Wextra -O2

ROOT     := $(abspath .)
CPPFLAGS ?= -I$(ROOT)/include -I$(ROOT)/src -I$(ROOT)/tests

SOURCES      := $(wildcard $(ROOT)/src/*.c)
OBJECTS      := $(SOURCES:$(ROOT)/src/%.c=$(ROOT)/build/obj/%.o)
TEST_SOURCES := $(wildcard $(ROOT)/tests/*.c)
LIBRARY      := $(ROOT)/build/lib$(LIB_NAME).a

# FreeRTOS kernel (shared between C and C++ examples)
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
                  -I$(ROOT)/examples/FreeRTOS/c \
                  -I$(FRTOS_ROOT)/include \
                  -I$(FRTOS_PORT)
FRTOS_C_CFLAGS := -std=c11 -Wall -Wextra -O2 -D_GNU_SOURCE -Wno-builtin-macro-redefined
FRTOS_C_SRCS   := $(ROOT)/examples/FreeRTOS/c/main.c \
                  $(ROOT)/examples/FreeRTOS/c/port.c

FRTOS_CXX_INC    := -I$(ROOT)/include \
                    -I$(ROOT)/examples/FreeRTOS/cxx \
                    -I$(FRTOS_ROOT)/include \
                    -I$(FRTOS_PORT)
FRTOS_CXX_FLAGS  := -std=c++20 -Wall -Wextra -O2 -D_GNU_SOURCE -Wno-builtin-macro-redefined
FRTOS_CXX_SRCS   := $(ROOT)/examples/FreeRTOS/cxx/main.cpp

# filecopy
FCOPY_C_INC    := -I$(ROOT)/include -I$(ROOT)/examples/filecopy/c
FCOPY_C_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O2 -D_GNU_SOURCE
FCOPY_C_SRCS   := $(ROOT)/examples/filecopy/c/main.c \
                  $(ROOT)/examples/filecopy/c/worker.c \
                  $(ROOT)/examples/filecopy/c/port.c

FCOPY_CXX_INC   := -I$(ROOT)/include -I$(ROOT)/examples/filecopy/cxx
FCOPY_CXX_FLAGS := -std=c++20 -Wall -Wextra -O2 -D_GNU_SOURCE
FCOPY_CXX_SRCS  := $(ROOT)/examples/filecopy/cxx/main.cpp \
                   $(ROOT)/examples/filecopy/cxx/worker.cpp \
                   $(ROOT)/examples/filecopy/cxx/port.cpp

# getting_started
GS_C_SRCS  := $(ROOT)/examples/getting_started/c/main.c
GS_CXX_SRCS := $(ROOT)/examples/getting_started/cxx/main.cpp

# Sources for compile_commands.json, grouped by their build flags.
# FreeRTOS kernel internals are excluded (not edited here).
PLAIN_SRCS := $(SOURCES) $(TEST_SOURCES) $(GS_C_SRCS)

.PHONY: all clean test examples \
        getting_started getting_started_cxx \
        filecopy filecopy_cxx \
        freertos freertos_cxx \
        compile_commands.json

all: $(LIBRARY) compile_commands.json

examples: getting_started getting_started_cxx filecopy filecopy_cxx freertos freertos_cxx

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
		$(COMPILER) $(CPPFLAGS) $(CFLAGS) $$src $(LIBRARY) $(LINKERFLAGS) -o $$bin || { failed=1; continue; }; \
		$$bin || failed=1; \
	done; \
	exit $$failed

getting_started: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(GS_C_SRCS) $(LIBRARY) \
		-o $(ROOT)/build/getting_started

getting_started_cxx: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GS_CXX_SRCS) $(LIBRARY) \
		-o $(ROOT)/build/getting_started_cxx

filecopy: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FCOPY_C_INC) $(FCOPY_C_CFLAGS) $(FCOPY_C_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/fcopy

filecopy_cxx: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(CXX) $(FCOPY_CXX_INC) $(FCOPY_CXX_FLAGS) $(FCOPY_CXX_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/fcopy_cxx

freertos: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FRTOS_C_INC) $(FRTOS_C_CFLAGS) \
		$(FRTOS_C_SRCS) $(FRTOS_KERNEL_SRCS) $(LIBRARY) -lpthread \
		-o $(ROOT)/build/example_FreeRTOS

freertos_cxx: $(LIBRARY)
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(FRTOS_C_INC) $(FRTOS_C_CFLAGS) -c $(FRTOS_KERNEL_SRCS) 2>/dev/null; \
	KERNEL_OBJS=$$(for f in $(FRTOS_KERNEL_SRCS); do echo "$$(basename $$f .c).o"; done | tr '\n' ' '); \
	$(CXX) $(FRTOS_CXX_INC) $(FRTOS_CXX_FLAGS) \
		$(FRTOS_CXX_SRCS) $$KERNEL_OBJS $(LIBRARY) -lpthread \
		-o $(ROOT)/build/example_FreeRTOS_cxx && \
	rm -f $$KERNEL_OBJS

compile_commands.json: Makefile
	@echo '[' > $@.tmp
	@first=1; \
	emit() { \
		if [ $$first -eq 0 ]; then printf ',\n' >> $@.tmp; fi; \
		first=0; \
		printf '{\n  "directory": "%s",\n  "file": "%s",\n  "command": "%s %s %s -c %s"\n}' \
			"$(ROOT)" "$$1" "$$4" "$$2" "$$3" "$$1" >> $@.tmp; \
	}; \
	for f in $(PLAIN_SRCS); do emit "$$f" "$(CPPFLAGS)" "$(CFLAGS)" "$(COMPILER)"; done; \
	for f in $(GS_CXX_SRCS); do emit "$$f" "$(CPPFLAGS)" "$(CXXFLAGS)" "$(CXX)"; done; \
	for f in $(FCOPY_C_SRCS); do emit "$$f" "$(FCOPY_C_INC)" "$(FCOPY_C_CFLAGS)" "$(COMPILER)"; done; \
	for f in $(FCOPY_CXX_SRCS); do emit "$$f" "$(FCOPY_CXX_INC)" "$(FCOPY_CXX_FLAGS)" "$(CXX)"; done; \
	for f in $(FRTOS_C_SRCS); do emit "$$f" "$(FRTOS_C_INC)" "$(FRTOS_C_CFLAGS)" "$(COMPILER)"; done; \
	for f in $(FRTOS_CXX_SRCS); do emit "$$f" "$(FRTOS_CXX_INC)" "$(FRTOS_CXX_FLAGS)" "$(CXX)"; done
	@printf '\n]\n' >> $@.tmp
	@mv $@.tmp $@
	@echo "generated $@"

clean:
	rm -rf $(ROOT)/build compile_commands.json
