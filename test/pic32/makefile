default: application.elf

SOURCE = $(dir $(firstword $(MAKEFILE_LIST)))


LINKERSCRIPT = linkerscript.ld

TARGET_COMMONFLAGS = -EL \
                     -mips32r2 \
                     -mhard-float \
                     -mno-mips16 \
                     -mno-micromips \
                     -mmcu \

# CPU specific C compilation flags
TARGET_CFLAGS = -fmessage-length=0 \
                -fno-common \
                -G0 \

# CPU specific C++ compilation flags
TARGET_CXXFLAGS = -fmessage-length=0 \
                  -fno-common \
                  -G0 \

# CPU specific assembler flags
TARGET_SFLAGS = -x assembler-with-cpp \
                -fmessage-length=0 \
                -fno-common \
                -G0 \

# CPU specific linker flags
TARGET_LDFLAGS = -nostdlib \
                 -Wcast-align=strict \
                 -T $(LINKERSCRIPT) \
                 -L $(dir $(LINKERSCRIPT)) \

COMMONFLAGS = \
  $(TARGET_COMMONFLAGS) \
  -Og \
  -g3 \
  -fmessage-length=0 \
  -ffunction-sections \
  -fdata-sections \
  -Wno-comment \
  -Wno-unused-function \

# Libraries from the toolchain
TOOLCHAIN_LDLIBS = -lm -lgcc

HDIR_FLAGS = -I$(SOURCE)include -I$(SOURCE)

CFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_CFLAGS) $(HDIR_FLAGS) \
  -Werror-implicit-function-declaration \

CXXFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_CXXFLAGS) $(HDIR_FLAGS)

SFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_SFLAGS) $(HDIR_FLAGS)

LDFLAGS = \
  -Wl,-Map=output.map \
  -Wl,--gc-sections \
  $(COMMONFLAGS) \
  $(TARGET_LDFLAGS) \

INSTALL = $(HOME)/embeetle
TOOLPREFIX = \
$(INSTALL)/beetle_tools/linux/mips_mti_toolchain_11.2.0_64b/bin/mips-mti-elf-

CC      = "$(TOOLPREFIX)gcc"
CXX     = "$(TOOLPREFIX)g++"
LD      = $(CXX)

OFILES = main.o start.o

%.elf: $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $(filter %.o %.a %.so,$^) $(PROJECT_LDLIBS) $(TOOLCHAIN_LDLIBS)

%.o: %.c; $(CC) -o $@ $(CFLAGS) -c $< 
%.o: %.cxx; $(CXX) -o $@ $(CXXFLAGS) -c $<
%.o: %.S; $(CC) -o $@ $(SFLAGS) -c $<

#$(OFILES): %.o: %.x; # Compile $@ from $<

VPATH = foo/bar ..

%.o: %.top

%.o: %.foo %.bar
	# Make top from foo and bar

bar/%.o: %.c
