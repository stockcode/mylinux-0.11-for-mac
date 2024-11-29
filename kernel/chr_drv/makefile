include ../../Makefile.header

CFLAGS +=-Wall -I../../include -c
LDFLAGS	+= -Ttext 0

OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c))
OBJECTS+=$(patsubst %.asm,%.o,$(wildcard *.asm))
OBJECTS+=$(patsubst %.s,%.o,$(wildcard *.s))
chr_drv.o:$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ -r $^
%.o:%.c
	$(CC) $(CFLAGS) -o $@ $<
%.o:%.asm	
	$(AS) -o $@ -f elf $< -g
%.o:%.s
	$(GAS) -o $@ $< -g
keyboard.s: kb.S ../../include/linux/config.h
	@$(CPP) kb.S -o keyboard.s
.PHONY clean:
clean:
	rm -rf *.o	