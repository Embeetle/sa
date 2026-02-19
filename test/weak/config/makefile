default: build

.PHONY: build
build: print_build $(BINARIES) show_size

SYSBIN = $(dir $(wildcard /bin/rm))
ifeq ($(SYSBIN)$(_),)
  SHELLSTYLE=cmd
  RM = $(if $(1),del /f /q $(subst /,\,$(1)))
  define REMOVE_ALL =
  for /d %%p in (*) do rmdir /s /q "%%p"
  for %%p in (*) do del /q "%%p"
  endef
  MKDIR = dir $(subst /,\,$(1)) >nul 2>nul || mkdir $(subst /,\,$(1)) >nul 2>nul || mkdir $(subst /,\,$(1))
  TOUCH = type NUL >>
else
  SHELLSTYLE=sh
  RM = $(SYSBIN)rm -f $(1)
  REMOVE_ALL = $(SYSBIN)rm -rf * .[!.]* ..?*
  MKDIR = $(SYSBIN)mkdir -p $(1)
  TOUCH = $(SYSBIN)touch
endif

ifeq ($(abspath $(SOURCE_DIR)),$(abspath .))
  $(info Please use shadow building:)
  $(info create a new directory and build there using)
  $(info $   $(MAKE) SOURCE_DIR=$(abspath .) -f $(abspath $(firstword $(MAKEFILE_LIST))))
  $(error Attempt to build in the source directory)
else
  ifeq ($(wildcard * .[^.]* ..?*),)
    $(shell $(TOUCH) .build)
  endif
  ifeq ($(wildcard .build),)
    $(info Build directory not empty and .build file not found)
    $(info Build directory is $(abspath .))
    $(info Create an empty .build file if you really want to build here)
    $(error Attempt to build in a non-empty non-build directory)
  endif
  SUBDIRS = $(sort $(dir $(OFILES)))
  .PHONY: subdirs
  subdirs: $(SUBDIRS)
  $(OFILES): | subdirs
  %/: ; $(call MKDIR, $@)
endif

define compilation-rules
$(1)%.c.o: $(2)%.c
	$$(c-rule)

$(1)%.cpp.o: $(2)%.cpp
	$$(cxx-rule)

$(1)%.cxx.o: $(2)%.cxx
	$$(cxx-rule)

$(1)%.c++.o: $(2)%.c++
	$$(cxx-rule)

$(1)%.cc.o: $(2)%.cc
	$$(cxx-rule)

$(1)%.C.o: $(2)%.C
	$$(cxx-rule)

$(1)%.s.o: $(2)%.s
	$$(asm-rule)

$(1)%.asm.o: $(2)%.asm
	$$(asm-rule)

$(1)%.S.o: $(2)%.S
	$$(asm-rule)

endef

define c-rule
$(info )
$(info )
$(info Compile: $<)
$(CC) $(CFLAGS) '$<' -o $@ -c
endef

define cxx-rule
$(info )
$(info )
$(info Compile: $<)
$(CXX) $(CXXFLAGS) '$<' -o $@ -c
endef

define asm-rule
$(info )
$(info )
$(info Compile: $<)
$(CC) $(SFLAGS) '$<' -o $@ -c
endef

$(eval $(call compilation-rules,,$(SOURCE_DIR)))

DEPFLAGS = -MMD -MF $(@:.o=.d) -MP -MQ $@
include $(wildcard $(OFILES:.o=.d))

CC      = "$(TOOLPREFIX)gcc"
CXX     = "$(TOOLPREFIX)g++"
LD      = $(if $(CXX_OFILES),$(CXX),$(CC))
OBJCOPY = "$(TOOLPREFIX)objcopy"
OBJSIZE = "$(TOOLPREFIX)size"
GDB     = "$(TOOLPREFIX)gdb"

COMMONFLAGS = \
  $(TARGET_COMMONFLAGS) \
  -Og \
  -g3 \
  -fmessage-length=0 \
  -ffunction-sections \
  -fdata-sections \
  -Wno-comment \
  -Wno-unused-function \
  -Werror-implicit-function-declaration \

CFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_CFLAGS) $(HDIR_FLAGS) -DV6

CXXFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_CXXFLAGS) $(HDIR_FLAGS)

SFLAGS = $(COMMONFLAGS) $(DEPFLAGS) $(TARGET_SFLAGS) $(HDIR_FLAGS)

LDFLAGS = \
  -Wl,-Map=output.map \
  -Wl,--gc-sections \
  $(COMMONFLAGS) \
  $(TARGET_LDFLAGS) \

$(ELF_FILE): $(dir $(firstword $(MAKEFILE_LIST)))filetree.mk $(OFILES)

%.elf:
	$(info )
	$(info )
	$(info )
	$(info Link program into $@)
	$(LD) $(LDFLAGS) -o $@ $(filter %.o %.a %.so,$^) $(PROJECT_LDLIBS) $(TOOLCHAIN_LDLIBS)

.PHONY: show_size
show_size: $(ELF_FILE:.elf=.size)

%.size: %.elf
	$(info )
	$(info )
	$(info Preparing: $@)
	$(OBJSIZE) $<

.PHONY: clean
clean: print_clean
ifeq ($(filter $(realpath .)/,$(realpath $(SOURCE_DIR))/),)
	$(REMOVE_ALL)
else
	$(call RM,$(wildcard $(patsubst %,%*.o %*.d,$(SUBDIRS)) *.elf *.size *.bin))
endif

LPAREN :=(
RPAREN :=)

.PHONY: print_clean
print_clean:
	$(info)
	$(info)
	$(info # ----------------------------------------------------------)
	$(info #        __         **************)
	$(info #      __\ \___     * make clean *)
	$(info #      \ _ _ _ \    **************)
	$(info #       \_`_`_`_\ )
	$(info # )
	$(info #  Operating system:    $(OS))
	$(info #  Source folder:       $(abspath $(SOURCE_DIR)))
	$(info #  Build folder:        $(abspath .))
	$(info # ----------------------------------------------------------)

.PHONY: print_build
print_build:
	$(info)
	$(info # ----------------------------------------------------------------)
	$(info #             $(RPAREN)\     **************)
	$(info #   $(LPAREN) =_=_=_=<| |    * make build *)
	$(info #             $(RPAREN)$(LPAREN)     **************)
	$(info #             "" )
	$(info # )
	$(info #  Operating system:    $(OS))
	$(info #  Source folder:       $(abspath $(SOURCE_DIR)))
	$(info #  Build folder:        $(abspath .))
	$(info #  Binary output:       $(BINARIES))
	$(info # ----------------------------------------------------------------)

.PHONY: print_flash
print_flash:
	$(info)
	$(info)
	$(info # ----------------------------------------------------------)
	$(info #        __         **************)
	$(info #       / /_        * make flash *)
	$(info #      /_  /        **************)
	$(info #        /` )
	$(info #       ` )
	$(info #  Operating system:    $(OS))
	$(info #  Source folder:       $(abspath $(SOURCE_DIR)))
	$(info #  Build folder:        $(abspath .))
	$(info # ----------------------------------------------------------)

.PHONY: print_flash_bootloader
print_flash_bootloader:
	$(info)
	$(info)
	$(info # ----------------------------------------------------------)
	$(info #        /\         *************************)
	$(info #       $(LPAREN)  $(RPAREN)        * make flash_bootloader *)
	$(info #       $(LPAREN)  $(RPAREN)        *************************)
	$(info #      /|/\|\ )
	$(info #     /_||||_\ )
	$(info #  Operating system:    $(OS))
	$(info #  Source folder:       $(abspath $(SOURCE_DIR)))
	$(info #  Build folder:        $(abspath .))
	$(info # ----------------------------------------------------------)

