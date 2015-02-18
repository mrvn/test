OBJS := boot.o main.o

CFLAGS := -O2 -W -Wall -fPIE
CFLAGS += -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16
CFLAGS += -ffreestanding -nostdlib
CXXFLAGS := $(CFLAGS) -std=gnu++11 -fno-exceptions
CFLAGS += -std=gnu99
LDFLAGS := -fPIE -nostdlib

all: kernel.img

kernel.img: kernel.elf
	arm-none-eabi-objcopy $< -O binary $@

kernel.elf: link-arm-eabi.ld $(OBJS)
	arm-none-eabi-g++ $(LDFLAGS) $(OBJS) -Tlink-arm-eabi.ld -o $@

%.o: %.S Makefile
	arm-none-eabi-gcc $(CFLAGS) -c -o $@ $<

%.o: %.c Makefile
	arm-none-eabi-gcc $(CFLAGS) -c -o $@ $<

%.o: %.cc Makefile
	arm-none-eabi-g++ $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f kernel.img kernel.elf $(OBJS)

