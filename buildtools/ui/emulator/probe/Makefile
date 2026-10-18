# The emulator test's probe (probe.c). Built in the toolchain image, as the DOL is:
#   make -C buildtools/ui/emulator/probe
include $(DEVKITPRO)/libogc2/gamecube_rules

ICONV := $(abspath ../../../../.build-deps/libiconv-lite)
CFLAGS := -O2 -Wall -Wextra -Werror $(MACHDEP) -I$(LIBOGC_INC) -I$(ICONV)/include

probe.dol: probe.elf
probe.elf: probe.c probe_grid.h iconv_check.h $(ICONV)/lib/libiconv.a
	$(CC) $(CFLAGS) $< $(MACHDEP) -L$(LIBOGC_LIB) -logc -lm $(ICONV)/lib/libiconv.a -o $@

clean:
	rm -f probe.elf probe.dol

.PHONY: clean
