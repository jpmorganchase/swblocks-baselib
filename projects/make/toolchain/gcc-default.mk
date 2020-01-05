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

ifeq ($(BL_PLAT_IS_DARWIN),1)

# the compiler is provided by the OS

CXX             := clang++

TOOLCHAIN_GCC_TOOLCHAIN_ID=$(TOOLCHAIN)

else

ifeq ($(DEVENV_VERSION_TAG),devenv4)
TOOLCHAIN_GCC_VERSION=8.1.0
TOOLCHAIN_GCC_TOOLCHAIN_ID=gcc810
TOOLCHAIN_GCC_TARGET_PLATFORM=pc
ifeq ($(BL_PROP_PLAT_IS_32BIT),1)
TOOLCHAIN_GCC_ARCH_TAG=x86
TOOLCHAIN_GCC_ARCH_TAG2=i686
TOOLCHAIN_GCC_ARCH_TAG3=i386
TOOLCHAIN_GCC_LIB_TAG=lib
else
TOOLCHAIN_GCC_ARCH_TAG=x64
TOOLCHAIN_GCC_ARCH_TAG2=x86_64
TOOLCHAIN_GCC_ARCH_TAG3=x86_64
TOOLCHAIN_GCC_LIB_TAG=lib64
endif
else
ifeq ($(DEVENV_VERSION_TAG),devenv3)
TOOLCHAIN_GCC_VERSION=6.3.0
TOOLCHAIN_GCC_TOOLCHAIN_ID=gcc630
TOOLCHAIN_GCC_TARGET_PLATFORM=pc
ifeq ($(BL_PROP_PLAT_IS_32BIT),1)
TOOLCHAIN_GCC_ARCH_TAG=x86
TOOLCHAIN_GCC_ARCH_TAG2=i686
TOOLCHAIN_GCC_ARCH_TAG3=i386
TOOLCHAIN_GCC_LIB_TAG=lib
else
TOOLCHAIN_GCC_ARCH_TAG=x64
TOOLCHAIN_GCC_ARCH_TAG2=x86_64
TOOLCHAIN_GCC_ARCH_TAG3=x86_64
TOOLCHAIN_GCC_LIB_TAG=lib64
endif
else
TOOLCHAIN_GCC_ARCH_TAG=x64
TOOLCHAIN_GCC_ARCH_TAG2=x86_64
TOOLCHAIN_GCC_ARCH_TAG3=x86_64
TOOLCHAIN_GCC_LIB_TAG=lib64
TOOLCHAIN_GCC_VERSION=4.9.2
TOOLCHAIN_GCC_TOOLCHAIN_ID=gcc492
TOOLCHAIN_GCC_TARGET_PLATFORM=unknown
endif
endif

ifeq ($(OS),rhel7)
ifeq ($(DEVENV_VERSION_TAG),devenv3)
TOOLCHAIN_ROOT_GCC := $(DIST_ROOT_DEPS3)/toolchain-gcc/$(TOOLCHAIN_GCC_VERSION)/rhel6-$(TOOLCHAIN_GCC_ARCH_TAG)-$(TOOLCHAIN_GCC_TOOLCHAIN_ID)-release
else
TOOLCHAIN_ROOT_GCC := $(DIST_ROOT_DEPS3)/toolchain-gcc/$(TOOLCHAIN_GCC_VERSION)/$(OS)-$(TOOLCHAIN_GCC_ARCH_TAG)-$(TOOLCHAIN_GCC_TOOLCHAIN_ID)-release
endif
else
TOOLCHAIN_ROOT_GCC := $(DIST_ROOT_DEPS3)/toolchain-gcc/$(TOOLCHAIN_GCC_VERSION)/$(OS)-$(TOOLCHAIN_GCC_ARCH_TAG)-$(TOOLCHAIN_GCC_TOOLCHAIN_ID)-release
endif

ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/3.9.1/$(OS)-x64-clang391-release
else ifeq ($(TOOLCHAIN),clang380)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/3.8.0/$(OS)-x64-clang380-release
else ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/3.5/ub14-x64-clang35-release
else ifeq ($(TOOLCHAIN),clang800)
TOOLCHAIN_ROOT := $(DIST_ROOT_DEPS3)/toolchain-clang/8.0.0/ub18-x64-clang800-release
else
TOOLCHAIN_ROOT := $(TOOLCHAIN_ROOT_GCC)
endif

TOOLCHAIN_STD_INCLUDES :=

CXX             := $(TOOLCHAIN_ROOT_GCC)/bin/g++
LD_LIBRARY_PATH := $(TOOLCHAIN_ROOT_GCC)/$(TOOLCHAIN_GCC_LIB_TAG):$(LD_LIBRARY_PATH)
LD_LIBRARY_PATH := $(TOOLCHAIN_ROOT_GCC)/libexec/gcc/$(TOOLCHAIN_GCC_ARCH_TAG2)-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION):$(LD_LIBRARY_PATH)

ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/3.5.0/include
endif

ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/3.9.1/include
endif

ifeq ($(TOOLCHAIN),clang380)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/3.8.0/include
endif

ifeq ($(TOOLCHAIN),clang800)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT)/lib/clang/8.0.0/include
endif

ifeq (clang, $(findstring clang, $(TOOLCHAIN)))
# clang is linking against GCC stdc++ library, so the GCC default lib locations have to be added explicitly
LIBPATH += $(TOOLCHAIN_ROOT_GCC)/$(TOOLCHAIN_GCC_LIB_TAG)
LIBPATH += $(TOOLCHAIN_ROOT_GCC)/libexec/gcc/$(TOOLCHAIN_GCC_ARCH_TAG2)-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)
endif

TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/lib/gcc/$(TOOLCHAIN_GCC_ARCH_TAG2)-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)/include
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/lib/gcc/$(TOOLCHAIN_GCC_ARCH_TAG2)-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu/$(TOOLCHAIN_GCC_VERSION)/include-fixed
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/include/c++/$(TOOLCHAIN_GCC_VERSION)
TOOLCHAIN_STD_INCLUDES += $(TOOLCHAIN_ROOT_GCC)/include/c++/$(TOOLCHAIN_GCC_VERSION)/$(TOOLCHAIN_GCC_ARCH_TAG2)-$(TOOLCHAIN_GCC_TARGET_PLATFORM)-linux-gnu

TOOLCHAIN_STD_INCLUDES += /usr/local/include
TOOLCHAIN_STD_INCLUDES += /usr/include/$(TOOLCHAIN_GCC_ARCH_TAG3)-linux-gnu
TOOLCHAIN_STD_INCLUDES += /usr/include

export LD_LIBRARY_PATH

CPPFLAGS += \
  $(patsubst %,-isystem %,$(TOOLCHAIN_STD_INCLUDES)) \

CXXFLAGS += -nostdinc

endif # ifeq ($(BL_PLAT_IS_DARWIN),1)

export CXX # needed by utf_loader

CXXFLAGS += -std=c++11 -fPIC -Wall -Wpedantic -Wextra
CXXFLAGS += -fno-strict-aliasing -fmessage-length=0
ifeq ($(BL_PLAT_IS_DARWIN),1)
#
# TODO: On Darwin there appears to be an annoying linker warning in the form:
#
# ld: warning: direct access in <some_symbol_name> means the weak symbol cannot be overridden
# at runtime. This was likely caused by different translation units being compiled with different visibility settings
#
# You can also see more details in the following Boost issue ticket:
# https://svn.boost.org/trac/boost/ticket/6998
#
# The solution is apparently to build all your code and external dependencies (!)
# with the same visibility settings (based on Google search for the above message)
# and since our external dependencies (boost and openssl) are built with visibility
# default then we also must use default here
#
# In the future we might change the instructions for building boost and openssl
# to also use -fvisibility=hidden
#
ifeq ($(DEVENV_VERSION_TAG),devenv3)
CXXFLAGS += -fvisibility=default
else
CXXFLAGS += -fvisibility=hidden
endif
# Produces debugging information for use by LLDB on Darwin
CXXFLAGS += -g
else
CXXFLAGS += -fvisibility=hidden
# Produces debugging information for use by GDB.
# This means to use the most expressive format available,
# including GDB extensions if at all possible.
CXXFLAGS += -ggdb
endif

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

# feq ($(TOOLCHAIN),clang800)
# CXXFLAGS += -Wno-address-of-temporary
# endif

CXXFLAGS += -MMD -MP # output dependency info for make
CPPFLAGS += -D_FILE_OFFSET_BITS=64

LD        = $(CXX)
export LD # needed by utf_loader

ifneq ($(BL_PLAT_IS_DARWIN),1)
LDFLAGS  += -pthread
LDFLAGS  += -static-libgcc
LDFLAGS  += -static-libstdc++
LDFLAGS  += -Wl,-version-script=$(MKDIR)/versionscript.ld  # mark non-exported symbols as 'local'
LDFLAGS  += -Wl,-Bstatic
LDADD    += -Wl,-Bdynamic # dynamic linking for os libs
LDADD    += -lrt          # librt and libdl
LDADD    += -ldl          # must be last
endif

LDFLAGS  += $(LIBPATH:%=-L%)

AR        = ar
OBJCOPY   = objcopy
KEEPSYMS := registerPlugin
KEEPSYMS += registerResolver
RANLIB    = ranlib

TOUCH	  = touch

OUTPUT_OPTION = -o $@

# echo gcc version, just fyi
ifneq (clang, $(findstring clang, $(TOOLCHAIN)))
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
ifeq ($(BL_PLAT_IS_DARWIN),1)
# TODO: on Darwin it appears objcopy is not present by default; we can stop using it for now
# and figure out how to split the debug symbols later on
LINK.$(TOOLCHAIN) = \
	$(LINK.o) $^ $(LOADLIBES) $(OUTPUT_OPTION) $(LDLIBS:%=-l%) $(LDADD) $(LDLIBSVERBATIM:%=%$(LIBEXT))
else
LINK.$(TOOLCHAIN) = \
	$(LINK.o) $^ $(LOADLIBES) $(OUTPUT_OPTION) $(LDLIBS:%=-l%) $(LDADD) $(LDLIBSVERBATIM:%=%$(LIBEXT)) && \
	  $(OBJCOPY) --only-keep-debug $@ $@$(DBGEXT) && \
	  $(OBJCOPY) --strip-all $(KEEPSYMS:%=--keep-symbol=%) $@ && \
	  $(OBJCOPY) --add-gnu-debuglink=$@$(DBGEXT) $@
endif
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
