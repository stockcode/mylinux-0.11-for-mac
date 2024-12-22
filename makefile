OS = Mac

# indicate the Hardware Image file
HDA_IMG = hdc-0.11.img

#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
RAMDISK = -DRAMDISK=2048

include Makefile.header


LDFLAGS	+= -Ttext 0 -e startup_32
CFLAGS	+= $(RAMDISK) -Iinclude -c
CPP	+= -Iinclude

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS =kernel/blk_drv/blk_drv.o kernel/chr_drv/chr_drv.o
MATH	=kernel/math/math.o
LIBS	=lib/lib.o

disk:image
	dd if=image of=floppy.img
image:boot/bootsect boot/setup tools/system
	#tools/build boot/boot.bin boot/setup.bin system>image

	@cp -f tools/system system.tmp
	@$(STRIP) system.tmp
	@$(OBJCOPY) -O binary -R .note -R .comment system.tmp tools/kernel
	@tools/build.sh boot/bootsect boot/setup tools/kernel Image $(ROOT_DEV)
	@rm system.tmp
	@rm -f tools/kernel
	@sync

init/main.o:init/main.c
	$(CC) $(CFLAGS) -o $@ $<

kernel/math/math.o:
	@make -C kernel/math

kernel/chr_drv/chr_drv.o:
	@make -C kernel/chr_drv
kernel/blk_drv/blk_drv.o:
	@make -C kernel/blk_drv
kernel/kernel.o:
	@make -C kernel
fs/fs.o:
	@make -C fs
lib/lib.o:
	@make -C lib
mm/mm.o:
	@make -C mm

boot/setup: boot/setup.s
	@make setup -C boot

boot/bootsect: boot/bootsect.s
	@make bootsect -C boot

tmp.s:	boot/bootsect.s tools/system
	@(echo -n "SYSSIZE = (";ls -l tools/system | grep system \
		| cut -c25-31 | tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s
	@cat boot/bootsect.s >> tmp.s

tools/build:tools/build.c
	$(GCC) -o $@ $< -g
tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	@$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system
	@nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map

start: clean image
	@qemu-system-i386 -cpu 486 -m 16M -drive format=raw,file=Image,if=floppy -drive format=raw,file=$(HDA_IMG),if=ide,index=0 -boot a

debug: clean image
	@qemu-system-i386 -cpu qemu32 -m 16M -drive format=raw,file=Image,if=floppy -drive format=raw,file=$(HDA_IMG),if=ide,index=0 -boot a -s -S

.PHONY:clean
clean:
	@rm -f Image System.map tmp_make core boot/bootsect boot/setup
	@rm -f init/*.o tools/system boot/*.o typescript* info bochsout.txt
	@for i in mm fs kernel lib boot; do make clean -C $$i; done