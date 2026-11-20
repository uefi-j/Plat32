EE_BIN = plat32.elf
EE_OBJS = main_plat.o
EE_LIBS = -lgskit -ldmakit -lpad -lmc -lkernel -lc
EE_CFLAGS += -Wno-unused-function

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
