
VPATH = ../source

TOOLPREFIX = /home/johan/embeetle/beetle_tools/linux/gnu_arm_embedded_toolchain_9.2.1_q4_64b/bin/arm-none-eabi-

CC      = "$(TOOLPREFIX)gcc"
CXX     = "$(TOOLPREFIX)g++"
LD      = $(if $(CXXFILES),$(CXX),$(CC))
OBJCOPY = "$(TOOLPREFIX)objcopy"
OBJSIZE = "$(TOOLPREFIX)size"
CFLAGS = -Wall -Werror
CPPFLAGS = -Iinc

SOURCE_PATH = ../source/ $(MAKEFILE_DIR)../../source/ $(MAKEFILE_DIR) source/
ifeq ($(SOURCE_DIR),)
  SOURCE_DIR = $(firstword $(wildcard $(SOURCE_PATH)))
endif
ifeq ($(wildcard $(SOURCE_DIR)),)
  $(error Source directory $(SOURCE_DIR) not found)
endif

HDIRS = \
components/boards \
components/libraries/delay \
components/libraries/experimental_section_vars \
components/libraries/log \
components/libraries/log/src \
components/libraries/strerror \
components/libraries/util \
components/softdevice/s140/headers \
components/toolchain/cmsis/include \
examples/peripheral/blinky/pca10056/blank/config \
integration/nrfx \
modules/nrfx \
modules/nrfx/hal \
modules/nrfx/mdk \

HDIRS_INCLUDES = $(patsubst %, -I$(SOURCE_DIR)%, $(HDIRS)) \

TARGET_SFLAGS = -D__packed="__attribute__((__packed__))" \
                   -D__weak="__attribute__((weak))" \
                   -x assembler-with-cpp \
                   -DBOARD_PCA10100 \
                   -DCONFIG_GPIO_AS_PINRESET \
                   -DFLOAT_ABI_HARD \
                   -DNRF52833_XXAA \
                   -D__HEAP_SIZE=8192 \
                   -D__STACK_SIZE=8192 \

TARGET_COMMONFLAGS = -mcpu=cortex-m4 \
                        -mfpu=fpv4-sp-d16 \
                        -mfloat-abi=hard \
                        -mthumb \
                        -std=c99 \
                        -mabi=aapcs \
                        -fno-strict-aliasing \
                        -fno-builtin \
                        -fshort-enums \

COMMONFLAGS = \
  $(TARGET_COMMONFLAGS) \
  -Og \
  -g3 \
  -MMD \
  -fmessage-length=0 \
  -ffunction-sections \
  -fdata-sections \
  -Wno-comment \
  -Wno-unused-function \
  -Werror-implicit-function-declaration \

SFLAGS = $(COMMONFLAGS) $(TARGET_SFLAGS) $(HDIRS_INCLUDES)

%.o: %.c; $(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
%.o: %.cpp; $(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS)
%.o: %.S; $(CC) $(SFLAGS) $< -o $@ -c

%.map: %; objdump -t $<

ab: source/a.o source/b.o; $(CC) $^ -r -o $@ || true
