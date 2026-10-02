# The emulator test's probe (probe.c). Built in the toolchain image, as the DOL is:
#   make -C buildtools/ui/emulator/probe
include $(DEVKITPRO)/libogc2/gamecube_rules

CFLAGS := -O2 -Wall -Wextra -Werror $(MACHDEP) -I$(LIBOGC_INC)

probe.dol: probe.elf
probe.elf: probe.c
	$(CC) $(CFLAGS) $< $(MACHDEP) -L$(LIBOGC_LIB) -logc -lm -o $@

clean:
	rm -f probe.elf probe.dol

.PHONY: clean
