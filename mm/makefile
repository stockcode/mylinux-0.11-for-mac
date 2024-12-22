include ../Makefile.header

CFLAGS += -nostdinc -I../include -g -c
OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c))
OBJECTS+=$(patsubst %.asm,%.o,$(wildcard *.asm))
OBJECTS+=$(patsubst %.s,%.o,$(wildcard *.s))
mm.o:$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ -r $^
%.o:%.c
	$(CC) $(CFLAGS) -o $@ $<
%.o:%.asm	
	$(AS) -o $@ -f elf $< -g
%.o:%.s
	$(GAS) -o $@ $< -g

.PHONY clean:
clean:
	rm -rf *.o	