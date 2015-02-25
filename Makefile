OBJS := boot.o memcpy.o gpio.o led.o uart.o mmu.o fpu.o font.o main.o

CROSS := arm-none-eabi-

CC      := $(CROSS)gcc
CXX     := $(CROSS)g++
CXXFILT := $(CROSS)c++filt
LD      := $(CROSS)ld
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy

DEPENDFLAGS := -MD -MP

BASEFLAGS   := -O2 -nostdlib -mpoke-function-name -mno-long-calls
BASEFLAGS   += -ffreestanding -fomit-frame-pointer -flto

WARNFLAGS   := -W -Wall -Wextra -Wshadow -Wcast-align -Wwrite-strings
WARNFLAGS   += -Wredundant-decls -Winline
WARNFLAGS   += -Wno-endif-labels -Wfloat-equal
WARNFLAGS   += -Wformat=2 -Winit-self
WARNFLAGS   += -Winvalid-pch -Wmissing-format-attribute
WARNFLAGS   += -Wmissing-include-dirs
WARNFLAGS   += -Wredundant-decls -Wshadow
WARNFLAGS   += -Wswitch -Wsystem-headers
WARNFLAGS   += -Wno-pragmas
WARNFLAGS   += -Wwrite-strings -Wdisabled-optimization -Wpointer-arith
WARNFLAGS   += -Werror -Wno-error=unused-variable -Wno-error=unused-parameter

ARCHFLAGS   := -march=armv7-a -mfloat-abi=hard -mfpu=vfpv3-d16

INCLUDES    := -I .

ALLFLAGS := $(DEPENDFLAGS) $(BASEFLAGS) $(WARNFLAGS) $(ARCHFLAGS)
ALLFLAGS += $(INCLUDES) -fPIE
CFLAGS   := $(ALLFLAGS) -std=gnu99 -Wstrict-prototypes -Wnested-externs -Winline
CXXFLAGS := $(ALLFLAGS) -std=gnu++11 -fno-exceptions -fno-rtti
# doesn't set constants
#LDFLAGS  := $(BASEFLAGS) $(ARCHFLAGS) -flto -fPIE -shared -Wl,-Bstatic
LDFLAGS  := $(BASEFLAGS) $(ARCHFLAGS) -fPIE -flto

# Set VERBOSE if you want to see the commands being executed
ifdef VERBOSE
  L = @:
  Q =
else
  L = @echo
  Q = @
  MAKEFLAGS += --no-print-directory
endif
export L Q

all: kernel.img

kernel.img: kernel.elf
	$(L) objcopy $< $@
	$(Q) $(OBJCOPY) $< -O binary $@

kernel.elf: link-arm-eabi.ld $(OBJS)
	$(L) link $@ $(OBJS)
	$(Q) $(CXX) $(LDFLAGS) $(OBJS) -l gcc -Tlink-arm-eabi.ld -o $@

%.o: %.S Makefile
	$(L) CC $@ $<
	$(Q) $(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c Makefile
	$(L) CC $@ $<
	$(Q) $(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cc Makefile
	$(L) CXX $@ $<
	$(Q) $(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	$(L) cleaning kernel.img kernel.elf $(OBJS)
	$(Q) rm -f kernel.img kernel.elf $(OBJS)

include *.d

