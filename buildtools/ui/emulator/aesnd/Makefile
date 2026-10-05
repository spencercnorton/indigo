# AESND stopped hundreds of times (aesnd.c), with the AESND the Swiss build
# made. Built in the toolchain image after make dev:
#   make -C buildtools/ui/emulator/aesnd
include $(DEVKITPRO)/libogc2/gamecube_rules

SWISS := ../../../../cube/swiss
CFLAGS := -O2 -Wall -Wextra -Werror $(MACHDEP) -I$(SWISS)/aesnd/libogc2/include -I$(LIBOGC_INC)

aesnd.dol: aesnd.elf
aesnd.elf: aesnd.c $(SWISS)/build/aesndlib.o
	$(CC) $(CFLAGS) $^ $(MACHDEP) -L$(LIBOGC_LIB) -logc -lm -o $@

$(SWISS)/build/aesndlib.o:
	@echo "build Swiss first (make dev): this links the AESND it made" >&2; exit 1

clean:
	rm -f aesnd.elf aesnd.dol

.PHONY: clean
