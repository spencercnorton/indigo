# The menu's audio stopped hundreds of times (aesnd.c). Built in the toolchain
# image, as the DOL is:
#   make -C buildtools/ui/emulator/aesnd
include $(DEVKITPRO)/libogc2/gamecube_rules

GUI := ../../../../cube/swiss/source/gui
CFLAGS := -O2 -Wall -Wextra -Werror $(MACHDEP) -I$(LIBOGC_INC) -I$(GUI)

aesnd.dol: aesnd.elf
aesnd.elf: aesnd.c $(GUI)/aesnd_reset.h
	$(CC) $(CFLAGS) $< $(MACHDEP) -L$(LIBOGC_LIB) -laesnd -logc -lm -o $@

clean:
	rm -f aesnd.elf aesnd.dol

.PHONY: clean
