MSVC     :=
WINSDK   :=

ifeq ($(TOOLCHAIN),vc12)
MSVC                := $(DIST_ROOT_DEPS3)/toolchain-msvc/vc12-update4/default
MSVCRTTAG           := Microsoft.VC120.CRT
WINSDK              := $(DIST_ROOT_DEPS3)/winsdk/8.1/default
WINSDKLIBSROOT      := $(WINSDK)/lib/winv6.3/um
endif

ifeq ($(MSVC),)
$(error unknown toolchain was provided: $(TOOLCHAIN))
endif

# toolchain env setup
INCLUDE  += $(MSVC)/VC/include
INCLUDE  += $(WINSDK)/include/shared
INCLUDE  += $(WINSDK)/include/um

PATH     := $(MSVC)/VC/redist/x86/$(MSVCRTTAG):$(MSVC)/Common7/ide:$(MSVC)/VC/bin:$(WINSDK)/bin/$(ARCH):$(WINSDK)/Debuggers/$(ARCH):$(PATH)
LIBPATH  += $(WINSDKLIBSROOT)/$(ARCH)

OBJEXT   := .obj
EXEEXT   := .exe
LIBPRE   := lib
LIBEXT   := .lib
SOEXT    := .dll
SOSUFFIX := dll
DBGEXT   := .pdb

CXX       = $(TOPDIR)scripts/cl.py -M
CXXFLAGS += -nologo
CXXFLAGS += -EHs
CXXFLAGS += -Zi
CXXFLAGS += -FD
CXXFLAGS += -bigobj
CXXFLAGS += -MT
CXXFLAGS += -GS
CXXFLAGS += -Oy-

#
# We need a way to externally disable strict warnings level, so we can build
# 3rd party code with our compiler flags
# MSVC_DISABLE_STRICT=1 in the command line will do the magic
#

ifneq ($(MSVC_DISABLE_STRICT),1)
CXXFLAGS += -W3
endif

CXXFLAGS += -WX

#
# TODO: we have to fix that at some point
#
# Once we remove the usage of unsafe APIs we can 
# remove _CRT_SECURE_NO_WARNINGS define
#
CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS

#
# TODO: we have to fix that at some point
#
# Disable C4535 - calling _set_se_translator() requires /EHa
# because _set_se_translator() is been called by Boost.Test
#
CPPFLAGS += -wd4535

#
# TODO: we have to fix that at some point
#
# Disable C4396 - 'boost::call_once' : the inline specifier cannot be used when a friend declaration refers to a specialization of a function template
# because thrift needs this to compile cleanly against recent versions of boost
#
CPPFLAGS += -wd4396

#
# TODO: we have to fix that at some point... when we upgrade the compiler,
# but nothing we can do about it now
#
# Disable C4503 - 'boost::detail::is_class_impl<T>::value' : decorated name length exceeded, name was truncated
#
CPPFLAGS += -wd4503

ifeq (winxp, $(findstring winxp, $(OS)))
  CPPFLAGS += -D_WIN32_WINNT=0x0501
else
  CPPFLAGS += -D_WIN32_WINNT=0x0601
endif
CPPFLAGS += -DWINDOWS_LEAN_AND_MEAN
CPPFLAGS += -DNOMINMAX
CPPFLAGS += -DSECURITY_WIN32
CPPFLAGS += -D_SECURE_SCL=0
CPPFLAGS += $(INCLUDE:%=-I%)

LD        = link
LDFLAGS  += -nologo
ifeq (winxp, $(findstring winxp, $(OS)))
  LDFLAGS  += -subsystem:console,5.01
else
  LDFLAGS  += -subsystem:console
endif
LDFLAGS  += -incremental:no
LDFLAGS  += -opt:icf
LDFLAGS  += -opt:ref
LDFLAGS  += -release
LDFLAGS  += -debug

AR        = lib
ARFLAGS   = -nologo

DEBUGGER  = cdb -G -g -o -cf $(TOPDIR)/scripts/cdb_script.txt

TOUCH	  = touch

# create objects from source
COMPILE.cc = $(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
%$(OBJEXT): OUTPUT_OPTION=-Fo$@ -Fd$(basename $@)$(DBGEXT)
$(BLDDIR)/%$(OBJEXT): $(SRCDIR)/%.cpp | mktmppath
	@mkdir -p $(@D)
	$(COMPILE.cc) $< $(OUTPUT_OPTION)

# msys converts most paths to windows, but not LIB
# this macro is used via `call' in the rules below
# to ensure the paths are properly formatted
PATHCONV = $(subst $(SPACE),\;,$(strip $(foreach d,$(1),\
    $(shell cd "$(d)" >/dev/null 2>&1 && pwd -W || echo "$(d)"))))

# link binaries from objects
LINK.o = LIB="$(call PATHCONV,$(LIBPATH))" $(LD) $(LDFLAGS) $(TARGET_ARCH)
LINK.$(TOOLCHAIN) = \
    $(LINK.o) $^ $(LOADLIBES) $(OUTPUT_OPTION) $(LDLIBS:%=$(LIBPRE)%$(LIBEXT)) $(LDLIBSVERBATIM:%=%$(LIBEXT))
%$(EXEEXT): OUTPUT_OPTION=-out:$@ -pdb:$(basename $@)$(DBGEXT)
%$(EXEEXT): | mktmppath
	@mkdir -p $(@D)
	$(LINK.$(TOOLCHAIN))

# link libraries from objects
ARCHIVE.lib = $(AR) $(ARFLAGS)
%$(LIBEXT): OUTPUT_OPTION=-out:$@
%$(LIBEXT): | mktmppath
	@mkdir -p $(@D)
	$(ARCHIVE.lib) $^ $(LOADLIBES) $(OUTPUT_OPTION)

# link shared libraries from objects
%$(SOEXT): OUTPUT_OPTION=-out:$@ -pdb:$(basename $@)$(DBGEXT)
%$(SOEXT): LDFLAGS += -dll
%$(SOEXT): | mktmppath
	@mkdir -p $(@D)
	$(LINK.o) $^ $(LOADLIBES) $(OUTPUT_OPTION) $(LDLIBS:%=lib%.lib)
