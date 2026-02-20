################################################################################
# Generic configuration
################################################################################

# Our debugging code only produces output when SA_DEBUG is set
# SA_DEBUG=1 writes to sa_output.txt in the project's .beetle folder.
# SA_DEBUG=2 writes to standard output
export SA_DEBUG = 2

# Preprocessor flags
#
# CPPDEPFLAGS takes care of automatic dependency generation (see further)
CPPFLAGS = \
  $(CPPDEPFLAGS) \
#  -DCHECK \
#  -DSUPPRESS_UNDEFINED_DIAGNOSTICS \

# Common compilation flags
COMMON_FLAGS = -O3 -W -Wextra -Wall -Werror -fPIC \
-g \
#   -g -O0 \

# C++ compilation flags
CXXFLAGS = $(COMMON_FLAGS) -Wno-maybe-uninitialized

# C compilation flags
CFLAGS = $(COMMON_FLAGS)

# Linker flags
LDFLAGS = -g $(PLATFORM_LDFLAGS)

# By default, $(CC) is used as linker as well as as C compiler, but for C++,
# $(CXX) is required.
LD = $(CXX)

OS = $(patsubst MINGW%,windows,$(patsubst Linux,linux,$(shell uname)))

# Architecture suffix for the sys output folder
# (Override if needed: make ARCH=x86_64)
ARCH ?= $(shell uname -m)
# Normalize a few common variants just in case
ARCH := $(patsubst amd64,x86_64,$(patsubst x64,x86_64,$(ARCH)))

# Build output sys folder (in the build directory)
# Examples:
#   sys-windows-x86_64
#   sys-linux-x86_64
SA_SYS = sys-$(OS)-$(ARCH)

################################################################################
# Project configuration
################################################################################

.PHONY: default
default: selftest test

# Define SOURCE and vpaths for shadow building
# Do NOT define VPATH,  or a vpath that matches 'sys'!
SOURCE = $(patsubst %/,%,$(dir $(firstword $(MAKEFILE_LIST))))
ifeq ($(realpath $(SOURCE)), $(realpath .))
$(error Build started in source directory)
endif
vpath %.c $(SOURCE)
vpath %.cpp $(SOURCE)
vpath %.py $(SOURCE)

CC = gcc
CXX = g++
SEVENZIP = 7za
RSYNC = rsync

EMBEETLE_SYS = $(HOME)/embeetle/sys

#-------------------------------------------------------------------------------
# Platform-dependent settings
#-------------------------------------------------------------------------------

ifeq ($(OS),windows)

# Extension for executables
EXE = .exe

# Extension for shared libraries
SLIB = .dll

# In CMD,  msys paths need this prefix.
PATHPREFIX = C:/msys64

# Python. On Windows, we call python from the cmd shell for realistic testing.
# This definition needs to be $(call)'d because of the required closing quote.
PYTHON = $(PYTHON_ENV) "C:\Program Files\Python312\python" $(PATHPREFIX)$(1)

# Additional libraries needed when linking against Clang
CLANG_LDLIBS = -lpthread -lz -lzstd -lVersion -lole32 -luuid

# The ARM toolchain used for SA regression testing
ARM_TOOLCHAIN = gnu_arm_toolchain_9.3.1_9-2020-q2-update_32b

PLATFORM_LDFLAGS =
#export PATH := $(SOURCE)/sys/$(OS)/lib;$(PATH)

# Check that we are running in the correct shell: the MSYS2 UCRT64 environment.
# This produces native Windows binaries (no dependency on msys2.dll) as long as
# we link against /ucrt64 libraries (not /usr libraries).
CMAKE_EXE = $(shell which cmake)
ifneq ($(CMAKE_EXE),/ucrt64/bin/cmake)
$(error Please use the MSYS2 UCRT64 shell (ucrt64.exe) and install mingw-w64-ucrt-x86_64-cmake so that 'which cmake' is /ucrt64/bin/cmake.)
endif

else

# Extension for executables
EXE =

# Extension for shared libraries
SLIB = .so

# Python. This definition needs to be $(call)'d for compatibility with Windows.
PYTHON = $(PYTHON_ENV) python $(1)

# Additional libraries needed when linking against Clang
CLANG_LDLIBS = -lpthread -lz \
  $(if $(wildcard /lib/x86_64-linux-gnu/libtinfo.so.5),-ltinfo,)

# The ARM toolchain used for SA regression testing
ARM_TOOLCHAIN = gnu_arm_toolchain_9.3.1_9-2020-q2-update_64b

PLATFORM_LDFLAGS = -ldl
#export LD_LIBRARY_PATH := $(SOURCE)/sys/$(OS)/lib:$(LD_LIBRARY_PATH)

endif

#-------------------------------------------------------------------------------
# Our self-built and patched version of LLVM.
#-------------------------------------------------------------------------------

LLVM_SRC = $(HOME)/llvm
LLVM_BLD = $(HOME)/bld/llvm
LLVM_LIB = $(LLVM_BLD)/lib
LLVM_BIN = $(LLVM_BLD)/bin

# CLANG_INCLUDES are the include dirs for code compiled against Clang
CLANG_INCLUDES = \
  $(LLVM_SRC)/clang/include \
  $(LLVM_SRC)/clang/tools/libclang \
  $(LLVM_SRC)/llvm/include \
  $(LLVM_BLD)/tools/clang/include \
  $(LLVM_BLD)/include \

CLANG_LIB_NAMES = \
  clangAnalysis \
  clangAST \
  clangASTMatchers \
  clangBasic \
  clangDriver \
  clangEdit \
  clangFrontend \
  clangLex \
  clangParse \
  clangSema \
  clangSerialization \
  clangSupport \
  LLVMAsmParser \
  LLVMBinaryFormat \
  LLVMBitReader \
  LLVMBitstreamReader \
  LLVMCore \
  LLVMDebugInfoDWARF \
  LLVMDemangle \
  LLVMFrontendOpenMP \
  LLVMIRReader \
  LLVMMC \
  LLVMMCParser \
  LLVMObject \
  LLVMOption \
  LLVMProfileData \
  LLVMRemarks \
  LLVMSupport \
  LLVMTargetParser \
  LLVMTextAPI \
  LLVMWindowsDriver \

CLANG_LIBS = $(patsubst %,$(LLVM_LIB)/lib%.a,$(CLANG_LIB_NAMES))

# Ninja wants output paths (relative to the build directory) when building
# archives; target names like "clangSupport" are not always exported.
# Convert /home/.../bld/llvm/lib/libXYZ.a -> lib/libXYZ.a
CLANG_ARCHIVES_NINJA = $(patsubst $(LLVM_BLD)/%,%,$(CLANG_LIBS))

# How to call cmake and ninja with gcc.
#
# Note: CMake does not always pick up the compiler to be used from PATH, but can
# default to /usr/bin/gcc and /usr/bin/g++. To override, set CC and CXX
# environment variables. See https://stackoverflow.com/questions/17275348
#
# On Windows, we build in the MSYS2 UCRT64 environment. This produces native
# Windows binaries (no msys2.dll dependency) when linking only against /ucrt64.
#
# -Wno-dev suppresses CMake "developer" policy warnings from older LLVM trees.
CMAKE = CC=$(CC) CXX=$(CXX) cmake -Wno-dev $(1)
NINJA = ninja -C $(1) $(2)

# Workaround for older LLVM trees with newer GCC/libstdc++:
# Force-include fixed-width integer typedefs everywhere.
LLVM_FORCE_CXXFLAGS = -include cstdint
LLVM_FORCE_CFLAGS   = -include stdint.h

# CLANG is the location of the clang executable in the LLVM build tree.
LLVM_CLANG = $(LLVM_BIN)/clang$(EXE)

# LLD is the location of the executable in the LLVM build tree.
LLVM_LLD = $(LLVM_BIN)/lld$(EXE)

# Use Ninja to update clang.
#
# Ninja for clang builds everything by default. It also supports the following
# targets, amongst others: clang (the clang executable), libclang (the clang
# library with stable API), lib*.a (with * taken from $(CLANG_LIB_NAMES)). In this
# makefile, we define targets for clang and clang-ibs for lib*.a.
#
# We cannot put a dependency on the result of ninja, because that would cause
# the target to be always rebuilt: `make` checks timestamps before any rules
# execute, and assumes without checking that the timestamp is updated after
# running a rule.  The only workaround is to run `make clang clang-libs` before
# invoking make to update a target that depends on clang.
#
.PHONY: llvm clang-libs clang lld

# Build all we need from llvm: the clang and lld executables and the clang libs
# IMPORTANT: build *output files* (lib/lib*.a) rather than logical target names
# because older LLVM CMake setups may not expose per-library targets.
llvm: %: $(LLVM_BLD)/build.ninja
	$(call NINJA,$(LLVM_BLD),lld clang)

# Build the necessary clang libs
clang-libs: $(LLVM_BLD)/build.ninja
	$(call NINJA,$(LLVM_BLD),clang)

# Build the clang executable
clang: %: $(LLVM_BLD)/build.ninja
	$(call NINJA,$(LLVM_BLD),clang)

# Build the lld executable (= llvm linker)
lld: %: $(LLVM_BLD)/build.ninja
	$(call NINJA,$(LLVM_BLD),lld)

# Run Cmake to prepare the clang build folder for Ninja.
#
# IMPORTANT: When running inside MSYS2/UCRT64, pass an MSYS-style path for the
# source tree (e.g. /home/...), not a Windows path (C:/...). Windows paths are
# only needed when invoking tools via CMD.
#
# Use modern -S/-B so the source dir is unambiguous and argument order cannot
# accidentally turn the source dir into an "extra path".
#
# CMake 4.x removed compatibility with projects that set cmake_minimum_required()
# below 3.5. For older LLVM checkouts, use the minimum policy version workaround.
$(LLVM_BLD)/build.ninja:
	mkdir -p $(LLVM_BLD)
	$(call CMAKE, \
	  -S $(LLVM_SRC)/llvm -B $(LLVM_BLD) -GNinja \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DLLVM_ENABLE_PROJECTS='clang;lld' \
	  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	  -DCMAKE_CXX_FLAGS='$(LLVM_FORCE_CXXFLAGS)' \
	  -DCMAKE_C_FLAGS='$(LLVM_FORCE_CFLAGS)')

.PHONY: clang-clean clang-libs-clean llvm-clean lld-clean
clang-clean:
	rm -rf $(LLVM_CLANG)
lld-clean:
	rm -rf $(LLVM_LLD)
clang-libs-clean:
	rm -rf $(LLVM_BLD)
llvm-clean: clang-clean lld-clean clang-libs-clean

# Give instructions for dependencies on files in the clang build tree.
$(LLVM_CLANG):
	# Use 'make clang' to build $@
	false
$(LLVM_LLD):
	# Use 'make lld' to build $@
	false
$(CLANG_LIBS):
	# Use 'make clang-libs' to build $@
	false


#-------------------------------------------------------------------------------
# Compilation and linking for the source analyzer
#-------------------------------------------------------------------------------

# All object files of the source analyzer
SA_OBJECTS = \
  Analyzer.o \
  Clang.o \
  source_analyzer.o \
  Project.o \
  File.o \
  Symbol.o \
  GlobalSymbol.o \
  LocalSymbol.o \
  Hdir.o \
  Diagnostic.o \
  Entity.o \
  EmptyLoop.o \
  ManagedEntity.o \
  Section.o \
  AsmAnalyzer.o \
  BinaryAnalyzer.o \
  FlagExtractor.o \
  MakeCommandInfo.o \
  LinkerScriptAnalyzer.o \
  LinkCommandAnalyzer.o \
  ExternalAnalyzer.o \
  Unit.o \
  Process.o \
  Linker.o \
  Occurrence.o \
  Inclusion.o \
  Task.o \
  LineOffsetTable.o \
  EditLog.o \
  FileKind.o \
  compiler.o \
  environment.o \
  cache.o \
  base/Tag.o \
  base/Timer.o \
  base/platform.o \
  base/float_manip.o \
  $(BASE_OBJS)

BASE_OBJS = \
  base/debug.o \
  base/time_util.o \
  base/os.o \
  base/filesystem.o \
  base/RefCounted.o \

#  Record.o \
  VirtualFunction.o \
  Constructor.o \


# We use a version script sa.version to make sure that only the intended symbols
# are globally available in the shared object, in order to avoid accidental
# conflicts with symbols with the same name in unrelated code. For example, we
# make sure that all symbols defined in LLVM remain local.
libsource_analyzer.so: $(SA_OBJECTS)
libsource_analyzer.so: $(CLANG_LIBS)
libsource_analyzer.so: $(SOURCE)/sa.version
libsource_analyzer.so: LDFLAGS += -Wl,--version-script=$(SOURCE)/sa.version
libsource_analyzer.so: LDFLAGS += $(CLANG_LDLIBS)

# Compiled code using the Clang lib needs special options
myclang: $(CLANG_LIBS)
# We currently use archives not shared libraries.  Note: when using shared
# libraries, -lclang only works if libclang.so is a symlink pointing to
# libclang.so.13, but symlinks cause trouble with Seafile

CLANG_INCLUDERS = Clang.cpp myclang.cpp
CLANG_INCLUDER_TARGETS = $(CLANG_INCLUDERS:.cpp=.o) $(CLANG_INCLUDERS:.cpp=.i)
$(CLANG_INCLUDER_TARGETS): \
  CPPFLAGS += $(patsubst %, -I%, $(CLANG_INCLUDES))
$(CLANG_INCLUDER_TARGETS): \
  CXXFLAGS += -fno-rtti --warn-no-unused-parameter -Wno-nonnull -Wno-address


#-------------------------------------------------------------------------------
# Installed sys folder for use in Embeetle
#-------------------------------------------------------------------------------

# The sys lib folder where compiled code (Clang, SA, ...) is installed
SA_SYSLIB = $(SA_SYS)/lib

# Files installed into SA_SYS in addition to $(SOURCE)/sys
SA_CLANG = $(SA_SYSLIB)/clang$(EXE)
SA_LLD = $(SA_SYSLIB)/lld$(EXE)
SA_WRAPPER = $(SA_SYS)/esa/source_analyzer.py
SA_SO = $(SA_SYSLIB)/libsource_analyzer.so

# Commands to finalize the sys tree.
finalize-sys = $(finalize-sys-$(OS))

# To finalize the sys tree on Windows, add commands needed by Embeetle and copy
# all used shared libraries originating from MSYS.
#
# `which 7za` returns '/usr/bin/7za' which is a script calling executable
# '/usr/lib/p7zip/7za', so we cannot copy `which 7za`
#
define finalize-sys-windows =
mkdir -p $@/bin $@/lib
cp $$(which diff) $@/bin
cp $$(which diff3) $@/bin
cp $$(which rsync) $@/bin
cp $$(which ssh) $@/bin
ldd $@/bin/* $@/lib/* \
| sed -e 's@.* => \([^ ]*\).*@cp \1 $@/lib@' -e t -e d \
| sort -u \
| grep -v ' /c/' \
| sh
endef

# Currently no extra commands needed to finalize the sys tree on Linux.
# Needed commands and shared libraries are added in Docker at release time.
#
define finalize-sys-linux =
endef

.PHONY: sys
sys: $(SA_SYS)

# Create the sys folder (with OS/arch suffix) as a copy of $(SOURCE)/sys with additional files
# Flatten $(SOURCE)/sys/$(OS) into $(SA_SYS) (no $(OS) subfolder in output).
$(SA_SYS): $(SOURCE)/sys \
  $(wildcard \
    $(SOURCE)/sys/esa   $(SOURCE)/sys/esa/*   $(SOURCE)/sys/esa/*/* \
    $(SOURCE)/sys/$(OS) $(SOURCE)/sys/$(OS)/* $(SOURCE)/sys/$(OS)/*/* \
  ) \
  $(LLVM_CLANG) $(LLVM_LLD) $(SOURCE)/source_analyzer.py libsource_analyzer.so
	rm -rf $@
	mkdir $@
	cp -a $</esa $@
	cp -a $</$(OS)/. $@
	cp $(LLVM_CLANG) $(SA_CLANG)
	cp $(LLVM_LLD) $(SA_LLD)
	cp $(SOURCE)/source_analyzer.py $(SA_WRAPPER)
	cp libsource_analyzer.so $(SA_SO)
	$(finalize-sys)

# Add a version file after checking that all source files are checked in
.PHONY: version-stamp
version-stamp: $(SA_SYS)
	# Are all source files checked in to git?
	cd $(SOURCE) && git status --short
	cd $(SOURCE) && test "$$(git status --short)" = ""
	cd $(LLVM_SRC) && git status --short
	cd $(LLVM_SRC) && test "$$(git status --short)" = ""
	( cd $(SOURCE) && git log --pretty='format:SA %H %ai%n' -n 1 ) >  $(SA_SYS)/version.txt
	( cd $(LLVM_SRC) && git log --pretty='format:LLVM %H %ai%n' -n 1 ) >> $(SA_SYS)/version.txt

release: version-stamp selftest test install

.PHONY: release
ifeq ($(OS),windows)

# To release on Windows, simply upload the build's sys directory
release: upload

# For testing, the SA sys tree should be available in the embeetle development
# tree to avoid having to release the SA to the server and download it again
# into the embeetle tree. On Linux, we can use a symbolic link, but on Windows,
# we need to maintain a mirror copy.
install: $(EMBEETLE_SYS)/timestamp

else

# To release on Linux, use docker to create a portable linux build
release:
	$(SOURCE)/docker/build-vm
	$(SOURCE)/docker/run-vm upload-sa

endif

# Convenience target: install SA so that it can be used for testing with
# Embeetle.  On Linux, no action is required except for updating all installed
# files.  On Windows, the sys tree must be copied into the Embeetle development
# tree. This is achieved by an extra dependency for Windows only on
# $(EMBEETLE_SYS)/timestamp.
.PHONY: install
install: sys

# Create or update a copy of the sys folder in the Embeetle development tree.
#
# We use the native (msys2) version of rsync, and not
# $(SOURCE)/sys/$(OS)/bin/rsync, because the latter requires Windows-style paths
# C:/... and does not understand /home/johan
#
# The sys folder in the Embeetle development tree should not contain version.txt
# because version.txt requires all source files to be checked in, which is
# annoying during development.
$(EMBEETLE_SYS)/timestamp: sys
	rsync -a --delete -v $(SA_SYS)/ $(EMBEETLE_SYS)
	touch $@

#-------------------------------------------------------------------------------
# Upload
#-------------------------------------------------------------------------------
# Do not use the 'upload' target directly; instead, use 'release'.  On
# Linux, the uploaded sys folder must be built in Docker, and using 'release'
# takes care of that. On Windows, 'upload' and 'release' are equivalent.

SA_UPLOAD_SERVER = embeetle@embeetle.com
SA_UPLOAD_LOC = /srv/new.embeetle/downloads/sa/$(OS)

sys.7z: $(SA_INSTALLED_FILES) version-stamp
	rm -f $@
	$(SEVENZIP) a $@ $(SA_SYS)

.PHONY: upload
upload: sys.7z sys selftest test
	scp $< $(SA_UPLOAD_SERVER):$(SA_UPLOAD_LOC)
	ssh $(SA_UPLOAD_SERVER) \
      "cd $(SA_UPLOAD_LOC) && rm -rf $(SA_SYS) && umask 22 && 7za x sys.7z"
	cat $(SA_SYS)/version.txt

#-------------------------------------------------------------------------------
# Testing
#-------------------------------------------------------------------------------

# For SA testing purposes, we need some toolchains. The required toolchains are
# downloaded automatically from the Embeetle server and installed locally in
# $(TOOLS_LOC) if not available yet.
TOOLS_URL = https://new.embeetle.com/downloads/beetle_tools/$(OS)
TOOLS_LOC = ../beetle_tools
TOOLCHAINS = \
  $(TOOLS_LOC)/$(ARM_TOOLCHAIN) \
  $(TOOLS_LOC)/$(PIC_TOOLCHAIN) \

PIC_TOOLCHAIN = mips_mti_toolchain_11.2.0_64b
# ARM_TOOLCHAIN is platform dependent

toolchains: $(TOOLCHAINS)
	touch $@

SEVENZIP_MIME_TYPE = application/x-7z-compressed; charset=binary

# Download and install a toolchain for testing.
# To re-use an existing beetle_tools directory, create a symlink at $(TOOLS_LOC)
$(TOOLS_LOC)/%:
	mkdir -p $(TOOLS_LOC)
	curl --output $@.7z $(TOOLS_URL)/$*.7z
	file --brief --mime $@.7z
	test "$$(file --brief --mime $@.7z)" = "$(SEVENZIP_MIME_TYPE)"
	rm -rf $@.tmp
	$(SEVENZIP) x -o$@.tmp $@.7z
	rm $@.7z
	mv $@.tmp/* $@
	rmdir $@.tmp

TOOLCHAIN_DIR=$(TOOLS_LOC)/$(ARM_TOOLCHAIN)
#test/pic32.test: TOOLCHAIN_DIR=$(TOOLS_LOC)/$(PIC_TOOLCHAIN)

# Environment variables to be set when running tests
#
# Python path makes sure that 'import source_analyzer' and 'import template'
# work.
#
# Toolchain dir makes sure that tests can find a toolchain.
PYTHON_ENV = \
  PYTHONPATH=$(SOURCE) \
  TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \

# Extra dependencies of some test programs
linktest: linktest.o libsource_analyzer.so
base/Chain_test: base/debug.o base/time_util.o base/os.o
myclang: myclang.o
base/Chain_test: LDFLAGS += -lpthread

# Enable linking rule for test programs
PROGRAMS += t linktest myclang base/Chain_test
SHAREDLIBS += libsource_analyzer.so

# Foo test
libfoo$(SLIB): foo.o $(SA_OBJECTS) $(CLANG_LIBS)
	g++ -shared -Wl,-soname,$(notdir $@) -Wl,-rpath,"\$$ORIGIN" -o $@ \
      $(filter %.o,$^) -Wl,--start-group $(filter %.a %.so,$^) -Wl,--end-group \
      $(LDFLAGS) $(CLANG_LDLIBS) -Wl,--version-script=$(SOURCE)/sa.version

foopy: foo.py libfoo$(SLIB)
	LD_LIBRARY_PATH=$(SA_SYSLIB) $(PYTHON_ENV) python C:/msys64/home/johan/sa/test/foo.py
#	"C:\Program Files\Python312\python" C:/msys64/home/johan/sa/foo.py

# Test toolchain settings
TARGET=arm-none-eabi

myclang.run: RUNFLAGS = \
  -target $(TARGET) \
  $(SOURCE)/test/myclang_data.c \

# target to analyze x/y.c with myclang is x/y.c.myclang
%.myclang: % myclang
	LD_LIBRARY_PATH=$(LLVM_LIB) ./myclang -target $(TARGET) $<

# Extra flags needed for test code from StackOverflow post; see
# python3_support.cpp (end of file) for explanation.
#python3_support.o python3_support.i: CPPFLAGS += -I/usr/include/python3.6m

SUBDIRS = $(patsubst %/, %, $(sort $(filter-out ./, test $(dir $(SA_OBJECTS)))))

test/stddef.test: $(SOURCE)/test/stddef.c
test/error.test: $(SOURCE)/test/error.c
test/cpp.test: $(wildcard $(SOURCE)/test/cpp/source/*)

test/foo.test: \
  $(SOURCE)/test/foo.c \
  $(SOURCE)/test/bar.c \
  $(SOURCE)/test/foo.mkf \

test/multi.test: \
  $(SOURCE)/test/multi.c \
  $(SOURCE)/test/multi.h \
  $(SOURCE)/test/multi2.c \
  $(SOURCE)/test/multi2.h \

test/simple.test: \
  $(SOURCE)/test/simple/source/simple_main.c \
  $(SOURCE)/test/simple/source/simple.c \
  $(SOURCE)/test/simple/source/simple2.c \
  $(SOURCE)/test/simple/source/simple.h \
  $(SOURCE)/test/simple/source/inc/simple2.h \
  $(SOURCE)/test/simple/makefile \

test/hello.test: \
  $(SOURCE)/test/hello/makefile \
  $(SOURCE)/test/hello/hello.c \

test/inline.test: \
  $(SOURCE)/test/inline/makefile \
  $(SOURCE)/test/inline/inline.c \
  $(SOURCE)/test/inline/inc \
  $(SOURCE)/test/inline/inc/inline.h \

test/symbol_kinds.test: \
  $(SOURCE)/test/generic.mkf \
  $(SOURCE)/test/symbol_kinds.c \

test/symkinds.test: \
  $(SOURCE)/test/generic.mkf \
  $(SOURCE)/test/symkinds.h \
  $(SOURCE)/test/symkinds1.cpp \
  $(SOURCE)/test/symkinds2.cpp \

test/virtual.test: \
  $(SOURCE)/test/virtual/source/main.cpp \
  $(SOURCE)/test/virtual/source/A.cpp \
  $(SOURCE)/test/virtual/source/A.h \

test/asm.test: \
  $(SOURCE)/test/asm/source/nordic.S \
  $(SOURCE)/test/asm/source/a.S \
  $(SOURCE)/test/asm/source/main.c \

test/pic32.test: \
  $(SOURCE)/test/pic32/makefile \
  $(SOURCE)/test/pic32/start.S \
  $(SOURCE)/test/pic32/main.c \

test/fa.test: \
  $(SOURCE)/test/fa/makefile \
  $(SOURCE)/test/fa/source/example.cpp \
  $(SOURCE)/test/fa/source/foo.cc \

SKIP =
TESTS = $(filter-out $(SKIP),$(patsubst $(SOURCE)/%.py,%.test,$(wildcard $(SOURCE)/test/*.py)))
SELFTESTS = \
  base/filesystem.selftest.test \
  base/os.selftest.test \
  base/platform.selftest.test \
  base/RefCounted.selftest.test \
  Process.selftest.test \
  LineOffsetTable.selftest.test \
  MakeCommandInfo.selftest.test \
  LinkCommandAnalyzer.selftest.test \
  compiler.selftest.test \

#  base/FileSystem.selftest.test \

.PHONY: selftest test
test: linktest $(TESTS)
selftest: $(SELFTESTS)
RUNS = $(patsubst %.test,%.run,$(TESTS)) $(patsubst %.test,%.py.run,$(TESTS))
run: $(RUNS)

base/filesystem.selftest: $(filter-out base/filesystem.o,$(BASE_OBJS))
base/filesystem.selftest.run: base/filesystem.selftest.prepare
base/filesystem.selftest.prepare: ; rm -rf cache
base/filesystem.selftest: LDFLAGS += -lpthread
base/RefCounted.selftest: $(filter-out base/RefCounted.o, $(BASE_OBJS))
base/RefCounted.selftest: environment.o
base/RefCounted.selftest: LDFLAGS += -lpthread
base/os.selftest: $(filter-out base/os.o,$(BASE_OBJS))
base/os.selftest: LDFLAGS += -lpthread
Process.selftest: Task.o $(BASE_OBJS)
Process.selftest: LDFLAGS += -lpthread
LineOffsetTable.selftest: $(filter-out base/RefCounted.o, $(BASE_OBJS))
LineOffsetTable.selftest: LDFLAGS += -lpthread
EditLog.selftest: $(filter-out base/RefCounted.o, $(BASE_OBJS))
EditLog.selftest: LDFLAGS += -lpthread
LinkerScriptAnalyzer.selftest: \
  $(filter-out LinkerScriptAnalyzer.o,$(SA_OBJECTS)) $(CLANG_LIBS)
LinkerScriptAnalyzer.selftest: LDFLAGS += -lpthread $(CLANG_LDLIBS)
LinkerScriptAnalyzer.selftest.test: \
  RUNFLAGS = $(SOURCE)/test/linkerscript/cmsdk_cm0_user_flash.ld
MakeCommandInfo.selftest: \
  $(filter-out base/RefCounted.o, $(BASE_OBJS)) FileKind.o environment.o
MakeCommandInfo.selftest: LDFLAGS += -lpthread
MakeCommandInfo.selftest.test: test/pic32.test
LinkCommandAnalyzer.selftest: \
  $(filter-out base/RefCounted.o, $(BASE_OBJS)) FileKind.o environment.o
LinkCommandAnalyzer.selftest: LDFLAGS += -lpthread
compiler.selftest: $(filter-out base/RefCounted.o, $(BASE_OBJS)) FileKind.o \
  environment.o
compiler.selftest: toolchains
compiler.selftest: LDFLAGS += -lpthread
compiler.selftest.run compiler.selftest.test: \
  RUNFLAGS = $(TOOLS_LOC)/$(ARM_TOOLCHAIN)

OTree.selftest: $(filter-out OTree.o,$(SA_OBJECTS)) $(CLANG_LIBS)
OTree.selftest: LDFLAGS += $(CLANG_LDLIBS)

TEST_PYTHON_DEPS =
$(TESTS) $(RUNS): $(SOURCE)/template.py sys

CXXFLAGS += -std=c++17


################################################################################
# Rules
################################################################################

# Shadow building: make sure subdirectories exist in build directory
# The . avoids errors when there are no subdirectories.
$(info $(shell echo mkdir -p . $(SUBDIRS)))
$(shell mkdir -p . $(SUBDIRS))
$(shell rm -rf trace-*.out)

# Auto-generate header dependencies
CPPDEPFLAGS = -MMD
include $(wildcard *.d $(patsubst %, %/*.d, $(SUBDIRS)))
%.h: ; @echo $@ missing
%.inc: ; @echo $@ missing

# Preprocessing
%.i: %.cpp
	$(info Changed: $?)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -E -o $@

# Compilation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -c -o $@

# Shared library linking
$(SHAREDLIBS): %.so:
	$(info Changed: $?)
	$(LD) -shared -Wl,-soname,$(notdir $@) -Wl,-rpath,"\$$ORIGIN" -o $@ $(filter %.o,$^) -Wl,--start-group $(filter %.a %.so,$^) -Wl,--end-group $(LDFLAGS)

# Program linking
$(PROGRAMS): %: %.o
	$(LD) -o $@ $^  -Wl,-rpath,"\$$ORIGIN/../lib" $(LDFLAGS)

# Program running within make
%.run: %
	LD_LIBRARY_PATH=$(SA_SYSLIB) ./$< $(RUNFLAGS)

%.selftest.test: %.selftest
	LD_LIBRARY_PATH=$(SA_SYSLIB) TOOLCHAIN_DIR="$(TOOLS_LOC)/$(ARM_TOOLCHAIN)" \
      ./$< $(RUNFLAGS)
	touch $@

%.test: %.py toolchains
	# Make $@ triggered by $?
	#echo PATH=$$PATH
	#ldd $(SA_SYS)/lib/libsource_analyzer.so
	#echo SA_DEBUG=$$SA_DEBUG
	#pwd
	#echo "---8x-----"
	#printenv
	#echo "---8x-----"
	LD_LIBRARY_PATH=$(SA_SYSLIB) $(PYTHON_ENV) python $<
	touch $@

%.test: test/%.test
	# $@ OK

%.run: %.py toolchains
	$(call PYTHON,$<)

%.py.run: %.py toolchains
	$(call PYTHON,$<)

%.run: test/%.run
	# $@ OK

%.run: test/%.py.run
	# $@ OK

%.out: %.py toolchains
	$(call PYTHON,$<) >$@

%.selftest: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DSELFTEST $< \
      $(filter %.o %.a,$^) $(LDFLAGS) -o$@

.PHONY: clang_asm_test
clang_asm_test: nordic.asm.test gcc_startup_nrf52840.asm.test

test/weak.test: $(SOURCE)/test/weak/weak.c
test/templ.test: $(SOURCE)/test/templ/templ.cpp
test/function_template.test: $(SOURCE)/test/function_template.cpp
test/inline_cpp.test: \
  $(SOURCE)/test/inline_cpp/inline_cpp.cpp \
  $(SOURCE)/test/inline_cpp/tip.cpp \
  $(SOURCE)/test/inline_cpp/makefile \

test/inline_c99.test: \
  $(SOURCE)/test/inline_c99/inline_c99.c \
  $(SOURCE)/test/inline_c99/makefile \

test/pic32.test: \
  $(SOURCE)/test/pic32/main.c \
  $(SOURCE)/test/pic32/start.S \
  $(SOURCE)/test/pic32/makefile \
  $(SOURCE)/test/pic32/linkerscript.ld \

%.c.test: $(SOURCE)/test/%.c $(SA_CLANG)
	rm -f $(notdir $(<:.c=.o))
	LD_LIBRARY_PATH=$(SA_SYSLIB) $(SA_CLANG) -fsyntax-only $<
	if [ -e $(notdir $(<:.c=.o)) ]; then \
        echo Error: $(notdir $(<:.c=.o)) created; \
        ls -l $(notdir $(<:.c=.o)); \
        false; \
    fi

%.cpp.test: $(SOURCE)/test/%.cpp $(SA_CLANG)
	rm -f $(notdir $(<:.cpp=.o))
	LD_LIBRARY_PATH=$(SA_SYSLIB) $(SA_CLANG) -fsyntax-only $<
	if [ -e $(notdir $(<:.cpp=.o)) ]; then \
        echo Error: $(notdir $(<:.cpp=.o)) created; \
        ls -l $(notdir $(<:.cpp=.o)); \
        false; \
    fi

# Cleaning
.PHONY: clean

clean:
	rm -f $(SOURCE)/.bld
	touch .bld
	test ! -e $(SOURCE)/.bld
	rm -rf * .[!.]* ..?*