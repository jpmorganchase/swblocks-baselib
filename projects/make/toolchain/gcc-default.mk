OBJEXT     := .o
EXEEXT     :=
LIBEXT     := .a
LIBPREFIX  := lib
SOEXT      := .so
SOSUFFIX   := so
DBGEXT     := .dbg

#
# We need to include our own includes before the toolchain includes
# because on rhel5 boost is installed in /usr/include and gets
# picked up instead of the one we want
#

CPPFLAGS += \
  $(patsubst %,-I%,$(filter-out $(DISTROOT)/%,$(INCLUDE))) \
  $(patsubst %,-isystem %,$(filter $(DISTROOT)/%,$(INCLUDE)))
  
ifeq ($(DEVENV_VERSION_TAG),devenv3)
TOOLCHAIN_GCC_VERSION=6.3.0
TOOLCHAIN_GCC_TOOLCHAIN_ID=gcc630
TOOLCHAIN_GCC_TARGET_PLATFORM=pc
else
TOOLCHAIN_GCC_VERSION=4.9.2
TOOLCHAIN_GCC_TOOLCHAIN_ID=gcc492
TOOLCHAIN_GCC_TARGET_PLATFORM=unknown
endif

ifeq ($(OS),rhel7)
TOOLCHAIN_ROOT_GCC := $(DIST_ROOT_DEPS3)/toolchain-gcc/$(TOOLCHAIN_GCC_VERSION)/rhel6-x64-$(TOOLCHAIN_GCC_TOOLCHAIN_ID)-release
else
TOOLCHAIN_ROOT_GCC := $(DIST_ROOT_DEPS3)/toolchain-gcc/$(TOOLCHAIN_GCC_VERSION)/$(OS)-x64-$(TOOLCHAIN_GCC_TOOLCHAIN_ID)-release
endif

ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/3.9.1/$(OS)-x64-clang391-release
else ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/3.5/ub14-x64-clang35-release
else
TOOLCHAIN_ROOT := $(TOOLCHAIN_ROOT_GCC)
endif

TOOLCHAIN_STD_INCLUDES :=

CXX             := $(TOOLCHAIN_ROOT_GCC)/bin/g++
LD_LIBRARY_PATH := $(TOOLCHAIN_ROOT_GCC)/lib64:$(LD_LIBRARY_PATH)
LD_LIBRARY_PATH := $(TOOLCHAIN_ROOT_GCC)/libexec/gcc/x86_64-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION):$(LD_LIBRARY_PATH)

ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/3.5.0/include
endif

ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/3.9.1/include
endif

ifeq (clang, $(findstring clang, $(TOOLCHAIN)))
# clang is linking against GCC stdc++ library, so the GCC default lib locations have to be added explicitly
LIBPATH += $(TOOLCHAIN_ROOT_GCC)/lib64
LIBPATH += $(TOOLCHAIN_ROOT_GCC)/libexec/gcc/x86_64-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)
endif

TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/lib/gcc/x86_64-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)/include
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/lib/gcc/x86_64-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)/include-fixed
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/include/c++/$(TOOLCHAIN_GCC_VERSION)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/include/c++/$(TOOLCHAIN_GCC_VERSION)/x86_64-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu

TOOLCHAIN_STD_INCLUDES += /usr/local/include
TOOLCHAIN_STD_INCLUDES += /usr/include/x86_64-linux-gnu
TOOLCHAIN_STD_INCLUDES += /usr/include

export LD_LIBRARY_PATH

CPPFLAGS += \
  $(patsubst %,-isystem %,$(TOOLCHAIN_STD_INCLUDES)) \

CXXFLAGS += -nostdinc

export CXX # needed by utf_loader

CXXFLAGS += -std=c++11 -fPIC -Wall -Wpedantic -Wextra
CXXFLAGS += -fno-strict-aliasing -fmessage-length=0
CXXFLAGS += -fvisibility=hidden

# Produces debugging information for use by GDB. 
# This means to use the most expressive format available, 
# including GDB extensions if at all possible. 
CXXFLAGS += -ggdb

#
# We need a way to externally disable strict warnings level, so we can build
# 3rd party code with our compiler flags
# GCC_DISABLE_STRICT=1 in the command line will do the magic
#

ifneq ($(GCC_DISABLE_STRICT),1)
CXXFLAGS += -Werror
endif

ifneq (clang, $(findstring clang, $(TOOLCHAIN)))
# options  not properly supported by clang
CXXFLAGS += -fno-omit-frame-pointer
CXXFLAGS += -ftrack-macro-expansion=0 --param ggc-min-expand=20
endif

CXXFLAGS += -MMD -MP # output dependency info for make
CPPFLAGS += -D_FILE_OFFSET_BITS=64

LD        = $(CXX)
export LD # needed by utf_loader

LDFLAGS  += -pthread
LDFLAGS  += -static-libgcc
LDFLAGS  += -static-libstdc++
LDFLAGS  += -Wl,-version-script=$(MKDIR)/versionscript.ld  # mark non-exported symbols as 'local'
LDFLAGS  += -Wl,-Bstatic
LDFLAGS  += $(LIBPATH:%=-L%)
LDADD    += -Wl,-Bdynamic # dynamic linking for os libs
LDADD    += -lrt          # librt and libdl
LDADD    += -ldl          # must be last

AR        = ar
OBJCOPY   = objcopy
KEEPSYMS := registerPlugin
KEEPSYMS += registerResolver
RANLIB    = ranlib

TOUCH	  = touch

OUTPUT_OPTION = -o $@

# echo gcc version, just fyi
ifneq (clang35, $(findstring clang35, $(TOOLCHAIN)))
ifneq ($(MAKECMDGOALS),clean)
  $(info )
  $(info $(HR))
  $(info Toolchain)
  $(info $(HR))
  $(info $(shell $(CXX) --version))
  $(info $(HR))
  $(info )
endif
endif

# create objects from source
COMPILE.cc = $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.pch = $(COMPILE.cc)
$(BLDDIR)/%$(OBJEXT): $(SRCDIR)/%.cpp | mktmppath
	@echo "Compiling $(<F)..."
	@mkdir -p $(@D)
	$(STATIC_ANALYZER_CMD) $(COMPILE.cc) $< $(OUTPUT_OPTION)

# link binaries from objects
LINK.o = $(LD) $(LDFLAGS) $(TARGET_ARCH)
LINK.$(TOOLCHAIN) = \
	$(LINK.o) $^ $(LOADLIBES) $(OUTPUT_OPTION) $(LDLIBS:%=-l%) $(LDADD) && \
	  $(OBJCOPY) --only-keep-debug $@ $@$(DBGEXT) && \
	  $(OBJCOPY) --strip-all $(KEEPSYMS:%=--keep-symbol=%) $@ && \
	  $(OBJCOPY) --add-gnu-debuglink=$@$(DBGEXT) $@
%$(EXEEXT): %$(OBJEXT) | mktmppath
	@echo "Linking $(@F)..."
	@mkdir -p $(@D)
	$(LINK.$(TOOLCHAIN))

# link libraries from objects
ARCHIVE.lib = $(AR) $(ARFLAGS)
%$(LIBEXT): | mktmppath
	@echo "Creating library $(@F)..."
	@mkdir -p $(@D)
	$(ARCHIVE.lib) $@ $^ $(LOADLIBES) && \
	  $(RANLIB) $@

# add shared flag when building shared objects
%$(SOEXT): LDFLAGS += -shared
