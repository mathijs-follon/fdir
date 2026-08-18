COMPILER    ?= gcc
ARCHIVE     ?= ar
CFLAGS      ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS    ?= -Iinclude
LINKERFLAGS ?=
LIB_NAME    ?= fdir

ROOT := $(abspath .)

SOURCES      := $(wildcard $(ROOT)/src/*.c)
OBJECTS      := $(SOURCES:$(ROOT)/src/%.c=$(ROOT)/build/%.o)
TEST_SOURCES := $(wildcard $(ROOT)/tests/*.c)
LIBRARY      := $(ROOT)/build/lib$(LIB_NAME).a

ALL_EXAMPLE_SOURCES := $(wildcard $(ROOT)/examples/*/*.c)
COMPDB_SRCS         := $(SOURCES) $(ALL_EXAMPLE_SOURCES) $(TEST_SOURCES)

.PHONY: all clean example test

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(ROOT)/build
	$(ARCHIVE) rcs $@ $^

$(ROOT)/build/%.o: $(ROOT)/src/%.c
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: all
	@mkdir -p $(ROOT)/build
	@if [ -z "$(TEST_SOURCES)" ]; then \
		echo "No test files found in $(ROOT)/tests/"; \
	else \
		$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LIBRARY) $(LINKERFLAGS) -o $(ROOT)/build/test_runner && \
		$(ROOT)/build/test_runner; \
	fi

example: all
ifndef EX
	$(error Please specify an example folder, e.g., 'make example EX=my_folder')
endif
	@mkdir -p $(ROOT)/build
	$(COMPILER) $(CPPFLAGS) $(CFLAGS) $(wildcard $(ROOT)/examples/$(EX)/*.c) $(LIBRARY) $(LINKERFLAGS) \
		-o $(ROOT)/build/example_$(EX)

compile_commands.json: Makefile $(COMPDB_SRCS)
	@echo '[' > $@.tmp
	@first=1; \
	for f in $(COMPDB_SRCS); do \
		if [ $$first -eq 0 ]; then printf ',\n' >> $@.tmp; fi; \
		first=0; \
		printf '{\n  "directory": "%s",\n  "file": "%s",\n  "command": "%s %s %s -c %s"\n}' \
			"$(ROOT)" "$$f" "$(COMPILER)" "$(CPPFLAGS)" "$(CFLAGS)" "$$f" >> $@.tmp; \
	done
	@printf '\n]\n' >> $@.tmp
	@mv $@.tmp $@
	@echo "generated $@"

clean:
	rm -rf $(ROOT)/build compile_commands.json
