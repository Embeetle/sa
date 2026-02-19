################################################################################
#                                 DASHBOARD.MK                                 #
################################################################################
# COPYRIGHT (c) 2020 Embeetle                                                  #
# This software component is licensed by Embeetle under the MIT license. Con-  #
# sult the license text at the bottom of this file.                            #
#                                                                              #
# Compatible with Embeetle makefile interface version 6                        #
#                                                                              #
#------------------------------------------------------------------------------#
#                                   SUMMARY                                    #
#------------------------------------------------------------------------------#
# This file is intended to be included in the makefile. It contains all        #
# variables that depend on dashboard settings in Embeetle.                     #
#                                                                              #
# We suggest to include this file in your makefile like so:                    #
#                                                                              #
#     MAKEFILE := $(lastword $(MAKEFILE_LIST))                                 #
#     MAKEFILE_DIR := $(dir $(MAKEFILE))                                       #
#     include $(MAKEFILE_DIR)dashboard.mk                                      #
#                                                                              #
#------------------------------------------------------------------------------#
#                                    EDITS                                     #
#------------------------------------------------------------------------------#
# This file was automatically generated, but feel free to edit. When you chan- #
# ge something in the dashboard, Embeetle will ask your permission to modify   #
# this file accordingly. You'll be shown a proposal for a 3-way-merge in a     #
# diffing window. In other words, your manual edits won't be lost.             #
#                                                                              #
#------------------------------------------------------------------------------#
#                               MORE INFORMATION                               #
#------------------------------------------------------------------------------#
# Consult the Embeetle website for more info about this file:                  #
# https://embeetle.com/#embeetle-ide/manual/beetle-anatomy/dashboard           #
#                                                                              #
################################################################################

CFG = default

################################################################################
#                                [cfg: default]                                #
################################################################################
ifeq ($(CFG),default)
  # 1.TOOLS
  # =======
  # When invoking the makefile, Embeetle passes absolute paths to the toolchain
  # (ARM, RISCV, ...) and the flash tool (OpenOCD, esptool, ...) on the command-
  # line.
  # Example:
  #
  #   > "TOOLPREFIX=C:/my_tools/gnu_arm_toolchain_9.2.1/bin/arm-none-eabi-"
  #   > "FLASHTOOL=C:/my_tools/openocd_0.10.0_dev01138_32b/bin/openocd.exe"
  #
  # If you ever invoke the makefile without these commandline-arguments,
  # you need a fallback mechanism. Therefore, we provide a default value
  # for these variables here. Read more about the reasons in ADDENDUM 2.
  TOOLPREFIX = arm-none-eabi-
  FLASHTOOL = bossac
  
  # 2. PROJECT LAYOUT
  # =================
  # The PROJECT LAYOUT section in the dashboard points to all important config
  # file locations (eg. linkerscript, openocd config files, ...). If you change
  # any of those locations in the dashboard, Embeetle changes the variables be-
  # low accordingly.
  #
  # NOTES:
  #     - These paths are all relative to the build directory.
  #     - Locations of 'dashboard.mk' and 'filetree.mk' are not
  #       defined here. That's because they're always located in
  #       the same folder with the makefile.
  ELF_FILE = application.elf
  SOURCE_DIR = ../
  LINKERSCRIPT = ../config/linkerscript.ld
  
  # 3. BINARIES
  # ===========
  # Define the binaries that must be built.
  BINARIES = \
    $(ELF_FILE) \
    $(ELF_FILE:.elf=.bin) \
    $(ELF_FILE:.elf=.hex) \
  # Define the rules to build these binaries from the .elf file.
  %.bin: %.elf
	$(info )
	$(info )
	$(info Preparing: $@)
	$(OBJCOPY) -O binary $< $@
  
  %.hex: %.elf
	$(info )
	$(info )
	$(info Preparing: $@)
	$(OBJCOPY) -O ihex $< $@
  
  # 4. COMPILATION FLAGS
  # ====================
  # CPU specific flags for C++, C and assembly compilation and linking.
  TARGET_COMMONFLAGS = -mcpu=cortex-m3 \
                       -mthumb \
                       -DF_CPU=84000000L \
                       -DARDUINO=10813 \
                       -DARDUINO_ARCH_SAM \
                       -D__SAM3X8E__ \
                       -DUSB_VID=0x2341 \
                       -DUSBCON \
                       "-DUSB_MANUFACTURER=\"Arduino LLC\"" \
                       -DARDUINO_SAM_DUE \
                       -DUSB_PID=0x003e \
                       "-DUSB_PRODUCT=\"Arduino Due\"" \
  
  # CPU specific C compilation flags
  TARGET_CFLAGS = -w \
                  -std=gnu11 \
                  -nostdlib \
                  --param max-inline-insns-single=500 \
                  -Dprintf=iprintf \
  
  # CPU specific C++ compilation flags
  TARGET_CXXFLAGS = -w \
                    -std=gnu++11 \
                    -nostdlib \
                    -fno-exceptions \
                    -fno-threadsafe-statics \
                    --param max-inline-insns-single=500 \
                    -fno-rtti \
  
  # CPU specific assembler flags
  TARGET_SFLAGS = -x assembler-with-cpp \
  
  # CPU specific linker flags
  TARGET_LDFLAGS = -Wl,--cref \
                   -Wl,--check-sections \
                   -Wl,--entry=Reset_Handler \
                   -Wl,--unresolved-symbols=report-all \
                   -Wl,--warn-common \
                   -Wl,--warn-section-align \
                   -u _sbrk \
                   -u link \
                   -u _close \
                   -u _fstat \
                   -u _isatty \
                   -u _lseek \
                   -u _read \
                   -u _write \
                   -u _exit \
                   -u kill \
                   -u _getpid \
                   -T $(LINKERSCRIPT) \
                   -L $(dir $(LINKERSCRIPT)) \
  
  # Libraries from the toolchain
  TOOLCHAIN_LDLIBS = -lm \
                     -lgcc \
  # NOTE:
  # The -DARM_MATH_XX flag can be:
  #     ARM_MATH_CM7
  #     ARM_MATH_CM4
  #     ARM_MATH_CM3
  #     ARM_MATH_CM0PLUS
  #     ARM_MATH_CM0
  #     ARM_MATH_ARMV8MBL
  #     ARM_MATH_ARMV8MML
  
  # 5. FLASH RULES
  # ==============
  # The 'flash' target flashes the .bin file to the target microcontroller. To
  # achieve this it invokes the 'bossac' program, pointed to by the FLASHTOOL
  # variable (defined at the top of this file), and provides the right parame-
  # ters to launch bossac properly.
  
  # IMPORTANT:
  # Make sure you connect to the Arduino DUE's 'Programming Port (USB2)' (the
  # one closest to the DC power connector)! Check this webpage for more info:
  # https://embeetle.com/#supported-hardware/arduino/boards/due
  
  # NOTE: Unlike other Arduino boards, the ATSAM3X8E microcontroller on this
  #       board needs to be reset right before flashing the firmware through its
  #       Flash Port (Serial Port). There is a trick to do this without pressing
  #       the reset button: open the port briefly at 1200 baud to trigger the
  #       chip to reset:
  ifeq ($(SHELLSTYLE),cmd)
    # Windows
    RESET_CHIP = mode $(FLASH_PORT) baud=12 dtr=on & mode $(FLASH_PORT) baud=12 dtr=off
  else
    # Linux
    RESET_CHIP = stty 1200 raw ignbrk hup < $(FLASH_PORT)
  endif
  
  # Back to the flash-procedure. The flash-rule defined below launches bossac
  # and instructs it to flash the firmware through a Serial Port.
  .PHONY: flash
  flash: $(BINARIES) print_flash
	$(RESET_CHIP)
	$(FLASHTOOL) --info \
                 --port=$(FLASH_PORT) \
                 --force_usb_port=false \
                 --erase \
                 --write \
                 --verify \
                 --boot=1 \
                 --reset \
                 $(ELF_FILE:.elf=.bin)
  
  # Let's examine these flags one-by-one:
  #
  #   -i, --info          Display diagnostic information identifying the target
  #                       device.
  #
  #   -d, --debug         Print verbose diagnostic messages for debug purposes.
  #
  #   -p, --port =<port>  Use the serial port <port> to communicate with the de-
  #                       vice.
  #
  #   -U, --force_usb_port =<bool>  Enable automatic detection of the target's
  #                                 USB port if <bool> is false. Disable  USB
  #                                 port autodetection if <bool> is true.
  #
  #   -e, --erase         Erase  the  target's  entire  flash  memory  before
  #                       performing  any  read or write operations.
  #
  #   -w, --write         Write FILE to the target's flash memory. This operat-
  #                       ion can be expedited  immensely if used in conjunction
  #                       with the '--erase' option.
  #
  #   -v, --verify        Verify that FILE matches the contents of flash on the
  #                       target, or vice-versa if you prefer.
  #
  #   -b, --boot =<val>   Boot from ROM if <val> is 0. Boot from FLASH if <val>
  #                       is 1. (The latter is default.) This option is comp-
  #                       letely disregarded on unsupported devices.
  #
  #   -R, --reset         Reset  the  CPU  after  writing  FILE  to  the  tar-
  #                       get. This option is completely disregarded on unsup-
  #                       ported devices.
endif

# ADDENDUM 1. MIT LICENSE
# =======================
# Copyright (c) 2020 Embeetle
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is furn-
# ished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# ADDENDUM 2. WHY THE FALLBACK MECHANISM FOR TOOLS?
# =================================================
# You might wonder: why bother with a default value? Embeetle could simply
# insert the actual paths (as selected in the dashboard) here, like:
#
# TOOLPREFIX = C:/my_tools/gnu_arm_toolchain_9.2.1/bin/arm-none-eabi-
# FLASHTOOL  = C:/my_tools/openocd_0.10.0_dev01138_32b/bin/openocd.exe
#
# However, that would make this dashboard.mk file location dependent: the
# location of the tool would be hardcoded. That's a problem if you access
# this project from two computers where the same tool is stored in different
# locations.