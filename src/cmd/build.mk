# /export/home/david/src/build.mk
TOP      ?= ../../..

# Force GNU Toolchain for Solaris
CC       ?= gcc
CXX      = g++
INSTALL  ?= install
MKDIR    ?= mkdir -p
STRIP    ?= strip

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
MANDIR   ?= $(PREFIX)/share/man/man1

CFLAGS   ?= -O2 -Wall -Wextra -std=c11
CXXFLAGS ?= -O2 -Wall -Wextra -std=c++17
CPPFLAGS ?= -I$(TOP)/include -I../include -I./include -I./inc
LDFLAGS  ?= 

UNAME_S := $(shell uname -s)

# Solaris-specific compatibility flags
ifeq ($(UNAME_S),SunOS)
    CPPFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
    LIBS     += -lsocket -lnsl
endif

# Automate Object List from SRCS
OBJS = $(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(patsubst %.cpp,%.o,$(SRCS))))

# Determine Linker (Use CXX if there are C++ files)
ifeq ($(words $(filter %.cpp %.cc,$(SRCS))),0)
    LD = $(CC)
else
    LD = $(CXX)
endif

define install-links
	@set -- $(1); \
	while [ $$# -ge 2 ]; do \
		src=$$1; dst=$$2; \
		ln -sf $$src $(DESTDIR)$(BINDIR)/$$dst; \
		printf "  LINK  $(BINDIR)/$$src -> $(BINDIR)/$$dst\n"; \
		shift 2; \
	done
endef

# Logic for MLINKS (Manual Symlinks)
define install-mlinks
	@set -- $(1); \
	while [ $$# -ge 2 ]; do \
		src=$$1; dst=$$2; \
		ln -sf $$src $(DESTDIR)$(MANDIR)/$$dst; \
		printf "  MLINK $(MANDIR)/$$src -> $(MANDIR)/$$dst\n"; \
		shift 2; \
	done
endef

all: $(PROG)

# Pattern Rules
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: all
	@printf "  INST  $(PROG)\n"
	@$(MKDIR) $(DESTDIR)$(BINDIR)
	@$(INSTALL) -m 755 $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	@if [ -n "$(MAN)" ]; then \
		$(MKDIR) $(DESTDIR)$(MANDIR); \
		$(INSTALL) -m 644 $(MAN) $(DESTDIR)$(MANDIR)/$(MAN); \
	fi
	$(call install-links,$(LINKS))
	$(call install-mlinks,$(MLINKS))

clean:
	rm -f $(PROG) $(OBJS) *.core

.PHONY: all clean install
