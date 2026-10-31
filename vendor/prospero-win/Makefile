# ProsperoPZ selects only the Wine runtime and Project Zomboid frontend.
ROOT := ../..
.PHONY: all native check
all: check
native:
	bash $(ROOT)/scripts/build-ps5.sh
check:
	bash $(ROOT)/tests/runtime/run.sh
