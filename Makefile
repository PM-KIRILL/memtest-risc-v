# Makefile для мультиархитектурной версии MemTest86+
#
# Основано на Makefile от Chris Brady

.PNONY: all test clean
all:

ROOT_DIR=$(CURDIR)
ARCH_DIR=$(ROOT_DIR)/arch/$(ARCH)

ifndef ARCH
$(error ARCH variable is not set, see `arch/*` for existing names)
endif

include $(ARCH_DIR)/config.mk

all: clean $(ARTIFACTS)
	@echo
	@echo "==> Build artifacts:" $(ARTIFACTS)

VPATH=$(ARCH_DIR):.

CFLAGS= $(ARCH_CFLAGS) -I$(ARCH_DIR) -I$(ROOT_DIR) -ggdb3 -Wall -O0 -fomit-frame-pointer -fno-builtin \
	-ffreestanding $(SMP_FL) -fno-stack-protector -fPIC

ASM_OBJS = $(subst .S,.o,$(ASM_SOURCES))

COMMON_OBJS = common.o test.o random.o vmem.o

OBJS = $(ASM_OBJS) reloc.o main.o init.o lib.o patn.o screen_buffer.o \
	config.o error.o smp.o $(COMMON_OBJS) $(ARCH_OBJS)

# Сначала выполняется статическая компоновка, чтобы убедиться в отсутствии
# неопределенных символов, а затем динамическая компоновка для получения
# полной информации о релокации
memtest_shared: memtest_shared.lds Makefile $(OBJS)
	$(LD) $(LD_FLAGS) --warn-constructors --warn-common -static -T $< -o $@ $(OBJS)
	$(LD) $(LD_FLAGS) -shared -Bsymbolic -T $< -o $@ $(OBJS)

memtest_shared.bin: memtest_shared
	$(OBJCOPY) -O binary $< $@

%.o: %.S config.h defs.h test.h
	$(CC) -I$(ARCH_DIR) -I$(ROOT_DIR) $(CFLAGS) -traditional $< -c -o $@

reloc.o: reloc.c
	$(CC) $(CFLAGS) -fno-strict-aliasing -c $<

test.o: test.c
	$(CC) $(ARCH_CFLAGS) -I$(ARCH_DIR) -I$(ROOT_DIR) $(TEST_OPT_FLAGS) -ggdb3 -Wall -O0 -fPIC -fomit-frame-pointer -fno-builtin -ffreestanding -c $<

random.o: random.c
	$(CC) $(ARCH_CFLAGS) -I$(ARCH_DIR) -I$(ROOT_DIR) -ggdb3 -Wall -O3 -fPIC -fomit-frame-pointer -fno-builtin -ffreestanding -c $<

# Сборка выполняется статически для упрощения запуска в qemu-user на сторонних архитектурах
test-runner: test-runner.c $(COMMON_OBJS)
	$(CC) $(ARCH_CFLAGS) -I$(ARCH_DIR) -I$(ROOT_DIR) -ggdb3 -Wall -O0 -fPIC -static $^ -o $@

test: test-runner
	$(RUN_WITH) ./test-runner

clean:
	rm -f *.o *.s *.iso memtest.bin memtest memtest_shared \
		memtest_shared.bin memtest.iso memtest.bin test-runner