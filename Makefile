OBJS := mmu.o

all: mmu.img

mmu.img: mmu.elf
	arm-none-eabi-objcopy $< -O binary $@

mmu.elf: link-arm-eabi.ld $(OBJS)
	arm-none-eabi-ld $(OBJS) -Tlink-arm-eabi.ld -o $@

%.o: %.S
	arm-none-eabi-gcc -march=armv7-a -mfloat-abi=hard \
	                  -mfpu=vfpv3-d16 -ffreestanding -nostdlib \
	                  -c -o $@ $<

install:
	sudo mount /dev/sdi1 /mnt/usb1
	sudo cp mmu.img /mnt/usb1/kernel7.img
	sudo umount /mnt/usb1

clean:
	rm -f mmu.img mmu.elf mmu.o

