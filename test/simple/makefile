CC      = "$(TOOLCHAIN)arm-none-eabi-gcc"
CXX     = "$(TOOLCHAIN)arm-none-eabi-g++"
LD      = $(if $(CXXFILES),$(CXX),$(CC))
OBJCOPY = "$(TOOLCHAIN)arm-none-eabi-objcopy"
OBJSIZE = "$(TOOLCHAIN)arm-none-eabi-size"
CFLAGS = -Wall -Werror
CPPFLAGS = -Iinc

%.o: %.c; $(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)
%.o: %.cpp; $(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS)

EMBEETLE_VERSION = simple.test
