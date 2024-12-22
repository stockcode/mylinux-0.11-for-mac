include ../Makefile.header

CFLAGS +=-Wall -O1 -I../include -c
LDFLAGS	+= -Ttext 0

OBJECTS=$(patsubst %.c,%.o,$(wildcard *.c))
OBJECTS+=$(patsubst %.s,%.o,$(wildcard *.s))
kernel.o:$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ -r $^
%.o:%.c
	$(CC) $(CFLAGS) -o $@ $<
%.o:%.s
	$(GAS) -o $@ $< -g

.PHONY clean:
clean:
	rm -rf *.o
	@for i in chr_drv blk_drv math; do make clean -C $$i; done