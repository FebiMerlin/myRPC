# Makefile -- top level build rules of the myRPC subproject.
# Copyright (C) 2026 myRPC project.
#
# Targets:
#   all    -- build every program;
#   clean  -- remove every object and temporary file, restore the sources;
#   deb    -- build the binary packages of every program;
#   test   -- build and run the unit tests and the integration test;
#   help   -- show this list.

MODULES = client server

.PHONY: all clean deb test help $(MODULES)

all:
	@for module in $(MODULES); do \
	  echo "=== building $$module ==="; \
	  $(MAKE) -C $$module all || exit 1; \
	done

clean:
	@for module in $(MODULES); do \
	  echo "=== cleaning $$module ==="; \
	  $(MAKE) -C $$module clean || exit 1; \
	done
	$(MAKE) -C tests clean
	rm -rf build

deb:
	@for module in $(MODULES); do \
	  echo "=== packaging $$module ==="; \
	  $(MAKE) -C $$module deb || exit 1; \
	done
	install -d build
	cp client/build/*.deb server/build/*.deb build/
	@echo "=== packages ==="
	@ls -1 build/*.deb

test: all
	$(MAKE) -C tests test

help:
	@echo "myRPC -- remote command execution over sockets"
	@echo
	@echo "  make all    build myRPC-client and myRPC-server"
	@echo "  make clean  remove the build artefacts"
	@echo "  make deb    build the deb packages of both programs"
	@echo "  make test   run the unit and the integration tests"
