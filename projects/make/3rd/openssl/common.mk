ifndef OPENSSL_COMMON_INCLUDED
OPENSSL_COMMON_INCLUDED = 1

OPENSSLROOT = $(DIST_ROOT_DEPS3)/openssl/$(BL_DEVENV_OPENSSL_VERSION)

ifeq (, $(OPENSSLDIR))
OS_REMAPPED := $(OS)

ifeq ($(OS),rhel7)
ifeq ($(DEVENV_VERSION_TAG),devenv3)
OS_REMAPPED := rhel6
endif
endif

ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_REMAPPED := gcc492
else ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_REMAPPED := gcc630
else ifeq ($(TOOLCHAIN),clang801)
TOOLCHAIN_REMAPPED := gcc830
else
TOOLCHAIN_REMAPPED := $(TOOLCHAIN)
endif

OPENSSLDIR  = $(OPENSSLROOT)/$(OS_REMAPPED)-$(ARCH)-$(TOOLCHAIN_REMAPPED)-$(VARIANT)
endif

$(info Building with OPENSSLDIR = $(OPENSSLDIR))

# this is necessary because for newer openssl versions we need to
# include some private headers - e.g. <crypto/rsa/rsa_locl.h>
# you can search for this header in the code and find more details there
INCLUDE  += $(OPENSSLROOT)/source
ifneq ($(DEVENV_VERSION_TAG),devenv3)
INCLUDE  += $(OPENSSLROOT)/source/include
endif

INCLUDE  += $(OPENSSLDIR)/include
LIBPATH  += $(OPENSSLDIR)/lib

ifeq (win, $(findstring win, $(OS)))
  LDLIBSVERBATIM   += libcrypto
  LDLIBSVERBATIM   += libssl
  LDLIBSVERBATIM   += crypt32
else ifeq ($(BL_PLAT_IS_DARWIN),1)
  # https://developer.apple.com/library/content/qa/qa1393/_index.html
  # according to the link above on OS X we can't force the linker to
  # prefer the static library over dynamic one if both exist, so we
  # have to use full paths
  LDLIBSVERBATIM   += $(OPENSSLDIR)/lib/libssl
  LDLIBSVERBATIM   += $(OPENSSLDIR)/lib/libcrypto
else
  LDLIBS   += ssl
  LDLIBS   += crypto
endif

endif # OPENSSL_COMMON_INCLUDED
