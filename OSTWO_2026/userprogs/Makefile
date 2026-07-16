# Makefile for user programs
# Builds flat binaries from assembly sources

NASM = nasm
NASMFLAGS = -f bin

PROGRAMS = hello.bin loop.bin

all: $(PROGRAMS)

%.bin: %.asm
	$(NASM) $(NASMFLAGS) $< -o $@
	@echo "Built $@ (size: $$(stat -c%s $@) bytes)"

clean:
	rm -f $(PROGRAMS)

.PHONY: all clean
