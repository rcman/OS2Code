CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

# Targets
ALL_TARGETS = os2run os2loader os2_emulator

.PHONY: all clean test

all: $(ALL_TARGETS)

# Main integrated loader with API emulation
os2run: os2_with_api.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Basic loader (parse and load only, no API)
os2loader: os2_loader.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Standalone API emulator test program
os2_emulator: os2_emulator.c
	$(CC) $(CFLAGS) -DTEST_API -o $@ $< -lpthread

# Run the API test program
test: os2_emulator
	./os2_emulator

# Clean built files
clean:
	rm -f $(ALL_TARGETS) *.o

# Install (optional - copies to /usr/local/bin)
install: os2run
	install -m 755 os2run /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/os2run
