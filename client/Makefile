# Makefile -- build rules of myRPC-client.
# Copyright (C) 2026 myRPC project.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=gnu11 -O2
CPPFLAGS += -I../common
LDFLAGS ?=

PROGRAM  = myRPC-client
VERSION  = 1.0.0
ARCH     = $(shell dpkg --print-architecture 2>/dev/null || echo amd64)

COMMON_DIR = ../common
SOURCES  = myrpc_client.c \
           $(COMMON_DIR)/myrpc_log.c \
           $(COMMON_DIR)/myrpc_config.c \
           $(COMMON_DIR)/myrpc_proto.c
OBJECTS  = $(notdir $(SOURCES:.c=.o))

DESTDIR ?=
PREFIX  ?= /usr
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man/man1
DOCDIR   = $(PREFIX)/share/doc/myrpc-client

BUILD_DIR = build
DEB_DIR   = $(BUILD_DIR)/deb
DEB_FILE  = myrpc-client_$(VERSION)_$(ARCH).deb

.PHONY: all clean deb install uninstall

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

# The sources of the shared part live in another directory, therefore the
# rules are written explicitly.
myrpc_client.o: myrpc_client.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_log.o: $(COMMON_DIR)/myrpc_log.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_config.o: $(COMMON_DIR)/myrpc_config.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_proto.o: $(COMMON_DIR)/myrpc_proto.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

install: $(PROGRAM)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(PROGRAM) $(DESTDIR)$(BINDIR)/$(PROGRAM)
	install -d $(DESTDIR)$(DOCDIR)
	install -m 0644 README.md $(DESTDIR)$(DOCDIR)/README.md

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGRAM)
	rm -rf $(DESTDIR)$(DOCDIR)

# Build the binary package with dpkg-deb; the control file and the
# maintainer scripts are taken from the debian directory.
deb: $(PROGRAM)
	rm -rf $(DEB_DIR)
	install -d $(DEB_DIR)/DEBIAN
	install -m 0644 debian/control $(DEB_DIR)/DEBIAN/control
	sed -i 's/^Version:.*/Version: $(VERSION)/' $(DEB_DIR)/DEBIAN/control
	sed -i 's/^Architecture:.*/Architecture: $(ARCH)/' $(DEB_DIR)/DEBIAN/control
	$(MAKE) install DESTDIR=$(CURDIR)/$(DEB_DIR)
	# --root-owner-group есть только в dpkg 1.19 и новее; на более старых
	# системах (например, Astra Linux 2.12) используется fakeroot, а при
	# его отсутствии -- обычная сборка от текущего пользователя.
	dpkg-deb --build --root-owner-group $(DEB_DIR) $(BUILD_DIR)/$(DEB_FILE) 2>/dev/null 	  || fakeroot dpkg-deb --build $(DEB_DIR) $(BUILD_DIR)/$(DEB_FILE) 2>/dev/null 	  || dpkg-deb --build $(DEB_DIR) $(BUILD_DIR)/$(DEB_FILE)
	@echo "package built: $(BUILD_DIR)/$(DEB_FILE)"

clean:
	rm -f $(OBJECTS) $(PROGRAM)
	rm -rf $(BUILD_DIR)
