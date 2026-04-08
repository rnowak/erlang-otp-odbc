# Makefile for building the odbcserver port program.
# Called from rebar3 hooks.

CURDIR := $(shell pwd)
BASEDIR := $(abspath $(CURDIR)/..)
PRIVDIR := $(BASEDIR)/priv

# Erlang paths
ERL ?= erl
ERTS_INCLUDE_DIR ?= $(shell $(ERL) -noshell -eval "io:format(\"~ts/erts-~ts/include/\", [code:root_dir(), erlang:system_info(version)])." -s init stop)
EI_INCLUDE_DIR ?= $(shell $(ERL) -noshell -eval "io:format(\"~ts\", [code:lib_dir(erl_interface, include)])." -s init stop)
EI_LIB_DIR ?= $(shell $(ERL) -noshell -eval "io:format(\"~ts\", [code:lib_dir(erl_interface, lib)])." -s init stop)

# Platform detection
UNAME_S := $(shell uname -s)

CC ?= cc
CFLAGS ?= -O2 -Wall
LDFLAGS =

# Platform-specific flags
ifeq ($(UNAME_S),Darwin)
    TARGET_FLAGS = -DUNIX
    # Try unixODBC first (Homebrew), fall back to iodbc
    ODBC_LIB := $(shell pkg-config --libs odbc 2>/dev/null || echo "-lodbc")
    ODBC_CFLAGS := $(shell pkg-config --cflags odbc 2>/dev/null)
    ifeq ($(ODBC_LIB),-lodbc)
        # Check Homebrew paths
        HOMEBREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo "/opt/homebrew")
        ifneq ($(wildcard $(HOMEBREW_PREFIX)/opt/unixodbc/lib/libodbc.dylib),)
            ODBC_LIB = -L$(HOMEBREW_PREFIX)/opt/unixodbc/lib -lodbc
            ODBC_CFLAGS = -I$(HOMEBREW_PREFIX)/opt/unixodbc/include
        else
            ODBC_LIB = -lodbc
        endif
    endif
else ifeq ($(UNAME_S),Linux)
    TARGET_FLAGS = -DUNIX
    ODBC_LIB := $(shell pkg-config --libs odbc 2>/dev/null || echo "-lodbc")
    ODBC_CFLAGS := $(shell pkg-config --cflags odbc 2>/dev/null)
else
    # Windows / other
    TARGET_FLAGS = -DWIN32
    ODBC_LIB = -lodbc32
    ODBC_CFLAGS =
endif

# Threading
THR_LIBS = -lpthread
THR_DEFS = -D_THREAD_SAFE -D_REENTRANT

INCLUDES = -I. -I$(EI_INCLUDE_DIR) -I$(ERTS_INCLUDE_DIR) $(ODBC_CFLAGS)
ALL_CFLAGS = $(CFLAGS) $(TARGET_FLAGS) $(THR_DEFS) $(INCLUDES)
ALL_LDFLAGS = -L$(EI_LIB_DIR) -lei $(ODBC_LIB) $(THR_LIBS) $(LDFLAGS)

TARGET = $(PRIVDIR)/bin/odbcserver

all: $(TARGET)

$(TARGET): odbcserver.o
	@mkdir -p $(PRIVDIR)/bin
	$(CC) $< $(ALL_LDFLAGS) -o $@

odbcserver.o: odbcserver.c odbcserver.h
	$(CC) $(ALL_CFLAGS) -c -o $@ $<

clean:
	rm -f odbcserver.o $(TARGET)

.PHONY: all clean
