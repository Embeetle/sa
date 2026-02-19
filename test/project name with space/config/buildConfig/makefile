#####################################################################################
#                                                                                   #
#                            MAKEFILE FOR STM32F303K8Tx
#                                                                                   #
#####################################################################################
# COPYRIGHT (c) 2018 Johan Cockx
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# --------------------------------------------------------------------------------------
# |                             ASSUMED PROJECT STRUCTURE                              |
# --------------------------------------------------------------------------------------
#
#    + myProj
#       |
#       |--- source
#       |
#       |--- build                    <- all build output ends up here
#       |
#       `--- config
#              |- buildConfig
#              |           |
#              |           |- makefile
#              |           |- linkerscript.ld
#              |           `- projFiles.txt       <- long list of all .c, .cpp, .h, .hpp files to be compiled
#              |
#              `- flashConfig
#
# --------------------------------------------------------------------------------------
# |                             HOW TO RUN THIS MAKEFILE                               |
# --------------------------------------------------------------------------------------
# The CWD should be the build folder!
# 
# Run make like this:
# 
# > make -f ../config/buildConfig/makefile
# 
# Interesting flags
# ------------------
# -j8                       => use 8 threads
# --output-sync=recurse     => provide synchronized output
#


# 1. MICROCONTROLLER SPECIFIC SETTINGS
# -------------------------------------
CC = $(TOOLPREFIX)gcc

CPUFLAGS = \
  -mcpu=cortex-m4 \
  -mthumb \
  -mfpu=fpv4-sp-d16 \
  -mfloat-abi=hard \

CPPFLAGS = \
  $(patsubst %, -I %, $(INCDIRS)) \
  $(CPUFLAGS) \
  -DSTM32F303x8 \
  -DARM_MATH_CM4 \
  -D__weak="__attribute__((weak))" \
  -D__packed="__attribute__((__packed__))" \
  -D__FPU_PRESENT="1U" \
  -DUSE_HAL_DRIVER \
  -O0 \
  -g3 \
  -Wall \
  -fmessage-length=0 \
  -Werror-implicit-function-declaration \
  -Wno-comment \
  -Wno-unused-function \
  -ffunction-sections \
  -fdata-sections \

CFLAGS = $(CPPFLAGS)

SFLAGS = $(CPUFLAGS) -g

LDFLAGS = \
  $(CPUFLAGS) \
  -specs=nosys.specs \
  -specs=nano.specs \
  -T ../config/buildConfig/linkerscript.ld \
  -Wl,-Map=output.map \
  -Wl,--gc-sections \
  -lm \


# 2. OS SPECIFIC SETTINGS
# ------------------------
ifeq ($(OS),Windows_NT)
  RM = del /F /Q 
  RMDIR = rmdir /S /Q
  MKDIR = mkdir
  ERRIGNORE = 2>NUL || (exit 0)
  PSEP=\\
  ECHO = @echo. \#
else
  RM = rm -rf 
  RMDIR = rm -rf 
  MKDIR = mkdir -p
  ERRIGNORE = 2>/dev/null
  PSEP=/
  ECHO = \#
endif
  
# 3. DEFAULT RULE
# ----------------
default: all
all: print_intro buildDirs myApp.bin myApp.size

# 4. LOCALIZE SOURCE FILES
# -------------------------
SOURCEDIR = ../source
VPATH = $(SOURCEDIR)

ifeq (4.2,$(firstword $(sort $(MAKE_VERSION) 4.2)))
SOURCEFILES = $(file < ../config/buildConfig/projFiles.txt)
else
ifeq ($(OS),Windows_NT)
$(error You version of make ($(MAKE_VERSION)) is too old)
else
SOURCEFILES = $(shell cat ../config/buildConfig/projFiles.txt)
endif
endif

CPPFILES = $(filter %.cpp %.cxx, $(SOURCEFILES))
CFILES = $(filter %.c, $(SOURCEFILES))
SFILES = $(filter %.s, $(SOURCEFILES))
HFILES = $(filter %.h, $(SOURCEFILES))
HPPFILES = $(filter %.hpp %.hxx, $(SOURCEFILES))

DIRS = $(sort $(dir $(CPPFILES) $(CFILES) $(SFILES)))

# 5. CREATE SUBDIRS IN THE BUILD FOLDER
# --------------------------------------
buildDirs:
	$(MKDIR) $(subst /,$(PSEP),$(DIRS)) $(ERRIGNORE)

INCDIRS = $(patsubst %, $(SOURCEDIR)/%, $(sort $(dir $(HFILES) $(HPPFILES))))

# 6. INCLUDE ALL DEPENDENCY FILES
# --------------------------------
include $(wildcard $(patsubst %, %*.d, $(DIRS)))

# 7. RULE TO IGNORE OLD H-FILES
# ------------------------------
# Imagine following situation:
#     main.c -> #include "foo.h"
#     foo.c  -> #include "foo.h"
#
# After building foo.o, make creates the foo.d and main.d dependency files,
# which contains the following lines:
#     main.d -> main.o: main.c foo.h
#     foo.d  -> foo.o: foo.c foo.h
# 
# Imagine you then change the name "foo" into "bar".
#     main.c -> #include "bar.h"
#     bar.c  -> #include "bar.h"
# 
# The dependency files are still the old ones upon the next build. So make believes
# that main.o still depends on foo.h. Make searches for a rule to build foo.h, cannot
# find such rule and gives the error: "cannot find rule to build foo.h".
# The empty rule below provides a rule to build foo.h (although nothing really happens).
# Make believes that foo.h is built, and moves on.
# 
# As the dependency files are still the old ones, the dependency from main.o upon bar.h
# is not yet declared in main.d.
# However, main.o rebuilds anyhow because the timestamp of main.c has changed.
# Rebuilding main.o results in overwriting the main.d dependency file, such
# that this dependency file is now updated for the next run.
%.h:
	$(ECHO) $@ moved or deleted

# 8. RULE TO BUILD THE FINAL .ELF FILE
# -------------------------------------
myApp.elf: $(patsubst %.c, %.o, $(CPPFILES) $(CFILES) $(SFILES))

# 9. RULES TO BUILD .O FILES
# ---------------------------
%.o: %.cpp
	$(CC) $< -o $@ -c -MMD $(CPPFLAGS)

%.o: %.c
	$(CC) $< -o $@ -c -MMD $(CFLAGS)

%.o: %.s
	$(CC) $< -o $@ -c $(SFLAGS)

%.elf:
	$(CC) -o $@ $^ $(LDFLAGS)

# 10. RULE TO BUILD THE BINARY FROM THE ELF
# ------------------------------------------
%.bin: %.elf
	arm-none-eabi-objcopy -O binary $< $@

# 11. SIZE INFORMATION
# ---------------------
%.size: %.elf
	arm-none-eabi-size $<

# 12. CLEAN
# ----------
.PHONY: clean
clean: print_clean
	cd .. ; $(RMDIR) build

# 13. ASCII ART
# --------------
.PHONY: print_intro
print_intro: 
	$(ECHO)
	$(ECHO)  ----------------------------------------------------------------
	$(ECHO)             )\     **************
	$(ECHO)   ( =_=_=_=^<^| ^|    * make all   *
	$(ECHO)             )(     **************
	$(ECHO)             ""
	$(ECHO)
	$(ECHO)  Operating system:    $(OS)        
	$(ECHO)  Project directory:   $(PROJDIR)
	$(ECHO)  Source folder:       $(SOURCEDIR)
	$(ECHO)  Build folder:        $(BUILDDIR)
	$(ECHO)  hIncludes:           $(hIncludes)
	$(ECHO)  cFiles:              $(cFilesFS)
	$(ECHO)  ----------------------------------------------------------------
	$(ECHO) 

.PHONY: print_clean
print_clean:
	$(ECHO)
	$(ECHO)
	$(ECHO) ----------------------------------------------------------
	$(ECHO)        __         **************    
	$(ECHO)      __\ \___     * make clean *    
	$(ECHO)      \ _ _ _ \    **************    
	$(ECHO)       \_`_`_`_\                                          .
	$(ECHO)                                     
	$(ECHO)
	$(ECHO) ----------------------------------------------------------
