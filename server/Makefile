# Makefile -- build rules of myRPC-server.
# Copyright (C) 2026 myRPC project.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=gnu11 -O2
CPPFLAGS += -I../common -D_GNU_SOURCE
LDFLAGS ?=

PROGRAM  = myRPC-server
VERSION  = 1.0.0
ARCH     = $(shell dpkg --print-architecture 2>/dev/null || echo amd64)

COMMON_DIR = ../common
SOURCES  = myrpc_server.c \
           $(COMMON_DIR)/myrpc_log.c \
           $(COMMON_DIR)/myrpc_config.c \
           $(COMMON_DIR)/myrpc_proto.c
OBJECTS  = $(notdir $(SOURCES:.c=.o))

DESTDIR ?=
PREFIX  ?= /usr
SBINDIR  = $(PREFIX)/sbin
CONFDIR  = /etc/myRPC
UNITDIR  = /lib/systemd/system
DOCDIR   = $(PREFIX)/share/doc/myrpc-server

BUILD_DIR = build
DEB_DIR   = $(BUILD_DIR)/deb
DEB_FILE  = myrpc-server_$(VERSION)_$(ARCH).deb

.PHONY: all clean deb install uninstall

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

myrpc_server.o: myrpc_server.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_log.o: $(COMMON_DIR)/myrpc_log.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_config.o: $(COMMON_DIR)/myrpc_config.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

myrpc_proto.o: $(COMMON_DIR)/myrpc_proto.c $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

install: $(PROGRAM)
	install -d $(DESTDIR)$(SBINDIR)
	install -m 0755 $(PROGRAM) $(DESTDIR)$(SBINDIR)/$(PROGRAM)
	install -d $(DESTDIR)$(CONFDIR)
	install -m 0644 conf/myRPC.conf $(DESTDIR)$(CONFDIR)/myRPC.conf
	install -m 0600 conf/users.conf $(DESTDIR)$(CONFDIR)/users.conf
	install -d $(DESTDIR)$(UNITDIR)
	install -m 0644 systemd/myRPC-server.service \
	        $(DESTDIR)$(UNITDIR)/myRPC-server.service
	install -d $(DESTDIR)$(DOCDIR)
	install -m 0644 README.md $(DESTDIR)$(DOCDIR)/README.md

uninstall:
	rm -f $(DESTDIR)$(SBINDIR)/$(PROGRAM)
	rm -f $(DESTDIR)$(UNITDIR)/myRPC-server.service
	rm -rf $(DESTDIR)$(DOCDIR)

deb: $(PROGRAM)
	rm -rf $(DEB_DIR)
	install -d $(DEB_DIR)/DEBIAN
	install -m 0644 debian/control $(DEB_DIR)/DEBIAN/control
	install -m 0755 debian/postinst $(DEB_DIR)/DEBIAN/postinst
	install -m 0755 debian/prerm $(DEB_DIR)/DEBIAN/prerm
	install -m 0644 debian/conffiles $(DEB_DIR)/DEBIAN/conffiles
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
