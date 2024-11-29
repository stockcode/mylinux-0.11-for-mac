include ../Makefile.header

CFLAGS +=-Wall -I../include -c
LDFLAGS	+= -Ttext 0

OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c))
OBJECTS+=$(patsubst %.asm,%.o,$(wildcard *.asm))
fs.o:$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ -r $^
%.o:%.c
	$(CC) $(CFLAGS) -o $@ $<
%.o:%.asm	
	$(AS) -o $@ -f elf $< -g

.PHONY clean:
clean:
	rm -rf *.o	