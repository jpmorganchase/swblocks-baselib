ifndef OPENSSL_1_0_2D_INCLUDED
OPENSSL_1_0_2D_INCLUDED = 1

ifeq ($(OS),rhel7)
OS_REMAPPED := rhel6
else
OS_REMAPPED := $(OS)
endif

ifeq ($(TOOLCHAIN),clang35)
TOOLCHAIN_REMAPPED := gcc492
else
TOOLCHAIN_REMAPPED := $(TOOLCHAIN)
endif

OPENSSLDIR  = $(DIST_ROOT_DEPS3)/openssl/1.0.2d-devenv2/$(OS_REMAPPED)-$(ARCH)-$(TOOLCHAIN_REMAPPED)-$(VARIANT)

INCLUDE  += $(OPENSSLDIR)/include
LIBPATH  += $(OPENSSLDIR)/lib

ifeq (win, $(findstring win, $(OS)))
  LDLIBSVERBATIM   += libeay32
  LDLIBSVERBATIM   += ssleay32
  LDLIBSVERBATIM   += gdi32
else
  LDLIBS   += ssl
  LDLIBS   += crypto
endif

endif # OPENSSL_1_0_2D_INCLUDED
