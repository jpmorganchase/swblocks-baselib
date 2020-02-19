WINSDK                    := $(DIST_ROOT_DEPS3)/winsdk/8.1/default
WINSDK10                  := $(DIST_ROOT_DEPS3)/winsdk/10/default

WINSDKLIBSROOT            := $(WINSDK)/lib/winv6.3/um

ifeq ($(TOOLCHAIN),vc141)
WINSDK10VERSIONTAG        := 10.0.17763.0
else
WINSDK10VERSIONTAG        := 10.0.10240.0
endif

WINSDK10UCRTLIBSROOT      := $(WINSDK10)/Lib/$(WINSDK10VERSIONTAG)

MSVC          :=
MSVCRTTAG     :=

ifeq ($(TOOLCHAIN),vc12)
MSVC                := $(DIST_ROOT_DEPS3)/toolchain-msvc/vc12-update4/default
MSVCRTTAG           := Microsoft.VC120.CRT
endif

ifeq ($(TOOLCHAIN),vc14)
MSVC                := $(DIST_ROOT_DEPS3)/toolchain-msvc/vc14-update3/default
MSVCRTTAG           := Microsoft.VC140.CRT
endif

ifeq ($(TOOLCHAIN),vc141)
MSVC                := $(DIST_ROOT_DEPS3)/toolchain-msvc/vc14.1/BuildTools
MSVCRTTAG           := Microsoft.VC141.CRT
MSVCVERSIONTAG      := 14.16.27023
ifeq ($(BL_WIN_ARCH_IS_X64),1)
MSVCHOSTARCHTAG     := Hostx64
else
MSVCHOSTARCHTAG     := Hostx86
endif
endif

ifeq ($(MSVC),)
$(error unknown toolchain was provided: $(TOOLCHAIN))
endif

##########################################################################
# verify that the req ARCH is available (e.g. x64 vs. x86) by checking if
# the artifacts of key dependencies (e.g. boost and openssl) are present
#

ifeq ("$(wildcard $(BL_EXPECTED_BOOSTDIR))","")
$(error Requested architecture '$(ARCH)' is not available due to missing dependency: $(BL_EXPECTED_BOOSTDIR))
endif
ifeq ("$(wildcard $(BL_EXPECTED_OPENSSLDIR))","")
$(error Requested architecture '$(ARCH)' is not available due to missing dependency: $(BL_EXPECTED_OPENSSLDIR))
endif

##########################################################################
# toolchain env setup
#

##########################################################################
# INCLUDE

ifeq ($(TOOLCHAIN),vc141)
INCLUDE  += $(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)/include
else
INCLUDE  += $(MSVC)/VC/include
endif

INCLUDE  += $(WINSDK)/include/shared
INCLUDE  += $(WINSDK)/include/um

ifeq ($(TOOLCHAIN),vc141)
INCLUDE  += $(WINSDK10)/Include/$(WINSDK10VERSIONTAG)/ucrt
else
ifeq ($(TOOLCHAIN),vc14)
INCLUDE  += $(WINSDK10)/Include/$(WINSDK10VERSIONTAG)/ucrt
endif
endif

##########################################################################
# LIBPATH

ifeq ($(TOOLCHAIN),vc141)
LIBPATH  += $(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)/lib/$(ARCH)
endif

ifeq ($(TOOLCHAIN),vc14)
ifeq ($(ARCH),x86)
LIBPATH  += $(MSVC)/VC/lib
else
LIBPATH  += $(MSVC)/VC/lib/amd64
endif
endif

LIBPATH  += $(WINSDKLIBSROOT)/$(ARCH)

ifeq ($(TOOLCHAIN),vc141)
# LIBPATH  += $(WINSDK10UCRTLIBSROOT)/um/$(ARCH)
LIBPATH  += $(WINSDK10UCRTLIBSROOT)/ucrt/$(ARCH)
endif

ifeq ($(TOOLCHAIN),vc14)
LIBPATH  += $(WINSDK10UCRTLIBSROOT)/ucrt/$(ARCH)
endif

##########################################################################
# PATH

ifeq ($(TOOLCHAIN),vc141)

ifeq ($(ARCH),x64)
PATH     := $(MSVC)/VC/Redist/MSVC/$(MSVCVERSIONTAG)\$(ARCH)\$(MSVCRTTAG):$(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)\bin\$(MSVCHOSTARCHTAG)\$(ARCH):$(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)\bin\$(MSVCHOSTARCHTAG)\x86:$(MSVC)/DIA SDK/bin/amd64:$(WINSDK)/bin/$(ARCH):$(WINSDK10)/bin/$(ARCH)/ucrt:$(PATH)
else
PATH     := $(MSVC)/VC/Redist/MSVC/$(MSVCVERSIONTAG)\$(ARCH)\$(MSVCRTTAG):$(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)\bin\$(MSVCHOSTARCHTAG)\$(ARCH):$(MSVC)/VC/Tools/MSVC/$(MSVCVERSIONTAG)\bin\$(MSVCHOSTARCHTAG)\x64:$(MSVC)/DIA SDK/bin:$(WINSDK)/bin/$(ARCH):$(WINSDK10)/bin/$(ARCH)/ucrt:$(PATH)
endif

else

ifeq ($(TOOLCHAIN),vc14)
PATH     := $(MSVC)/Common7/ide:$(WINSDK)/bin/$(ARCH):$(WINSDK10)/bin/$(ARCH)/ucrt:$(WINSDK10)/Debuggers/$(ARCH):$(WINSDK10)/Redist/ucrt/DLLs/$(ARCH):$(PATH)
else
PATH     := $(MSVC)/Common7/ide:$(WINSDK)/bin/$(ARCH):$(WINSDK)/Debuggers/$(ARCH):$(PATH)
endif

endif

# Apparently on later versions of Windows Microsoft puts python (and potentially other aliases) in front of the path
# and it opens the Microsoft store (see link below). Put Python in front of the path to workaround this.
# %HOME%\AppData\Local\Microsoft\WindowsApps\python.exe
# https://superuser.com/questions/1437590/typing-python-on-windows-10-version-1903-command-prompt-opens-microsoft-stor

ifeq ($(BL_WIN_ARCH_IS_X64),1)
PATH     := $(DIST_ROOT_DEPS3)/python/2.7-latest/default:$(PATH)
else
PATH     := $(DIST_ROOT_DEPS3)/python/2.7-latest/default-x86:$(PATH)
endif

##########################################################################
# Other common configuration

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

# force to use MSPDBSRV.EXE
CXXFLAGS += -FS

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

ifeq ($(ARCH),x64)
LDFLAGS  += -machine:x64
endif

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
