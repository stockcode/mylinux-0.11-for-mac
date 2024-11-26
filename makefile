OS = Mac

# indicate the Hardware Image file
HDA_IMG = hdc-0.11.img

include Makefile.header


LDFLAGS	+= -Ttext 0
CFLAGS	+= -Iinclude -c
CPP	+= -Iinclude


OBJECTS=boot/head.o kernel/kernel.o init/main.o lib/lib.o mm/mm.o
disk:image
	dd if=image of=floppy.img
image:boot/boot.bin boot/setup.bin tools/system
	#tools/build boot/boot.bin boot/setup.bin system>image

	@cp -f tools/system system.tmp
	@$(STRIP) system.tmp
	@$(OBJCOPY) -O binary -R .note -R .comment system.tmp tools/kernel
	@tools/build.sh boot/boot.bin boot/setup.bin tools/kernel Image $(ROOT_DEV)
	@rm system.tmp
	@rm -f tools/kernel
	@sync

boot/boot.bin:boot/boot.asm
	 $(AS) -f bin -o $@ $<
boot/setup.bin:boot/setup.asm
	 $(AS) -f bin -o $@ $<
boot/head.o:boot/head.asm	 
	$(AS) -f elf -o $@ $<
init/main.o:init/main.c
	$(CC) $(CFLAGS) -o $@ $<
kernel/kernel.o:
	(cd kernel;make)
lib/lib.o:
	(cd lib;make)
mm/mm.o:
	(cd mm;make)
tools/build:tools/build.c
	$(GCC) -o $@ $< -g
tools/system:$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	@nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map

start: clean image
	@qemu-system-i386 -m 32M -drive format=raw,file=Image,if=floppy -boot a # -hda $(HDA_IMG)

debug: clean image
	@qemu-system-i386 -m 16M -drive format=raw,file=Image,if=floppy -boot a -s -S #-hda $(HDA_IMG)

.PHONY:clean
clean:
	rm -f boot/*.bin boot/head.o init/*.o tools/system image System.map
	(cd kernel;make clean)
	(cd lib;make clean)
	(cd mm;make clean)