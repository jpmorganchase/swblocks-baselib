ifndef OPENSSL_1_1_0D_INCLUDED
OPENSSL_1_1_0D_INCLUDED = 1

ifeq ($(OS),rhel7)
OS_REMAPPED := rhel6
else
OS_REMAPPED := $(OS)
endif

ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_REMAPPED := gcc492
else ifeq ($(TOOLCHAIN),clang391)
TOOLCHAIN_REMAPPED := gcc630
else
TOOLCHAIN_REMAPPED := $(TOOLCHAIN)
endif

OPENSSLROOT = $(DIST_ROOT_DEPS3)/openssl/1.1.0d
OPENSSLDIR  = $(OPENSSLROOT)/$(OS_REMAPPED)-$(ARCH)-$(TOOLCHAIN_REMAPPED)-$(VARIANT)

# this is necessary because for newer openssl versions we need to
# include some private headers - e.g. <crypto/rsa/rsa_locl.h>
# you can search for this header in the code and find more details there
INCLUDE  += $(OPENSSLROOT)/source

INCLUDE  += $(OPENSSLDIR)/include
LIBPATH  += $(OPENSSLDIR)/lib

ifeq (win, $(findstring win, $(OS)))
  LDLIBSVERBATIM   += libeay32
  LDLIBSVERBATIM   += ssleay32
  LDLIBSVERBATIM   += gdi32
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

endif # OPENSSL_1_1_0D_INCLUDED
