EE_BIN = platformer32.elf
EE_OBJS = main.o
GSKIT ?= $(PS2DEV)/gsKit
EE_INCS += -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -lmc -lc -lkernel
EE_CFLAGS += -Wno-unused-function

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
