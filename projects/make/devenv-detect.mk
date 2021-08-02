ifeq ($(BL_PLAT_IS_RHEL),1)
# clang or gcc may or may not be available on platform, so check first
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-clang/8.0.1)","")
  TOOLCHAIN                 ?= clang801
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/8.3.0)","")
  TOOLCHAIN                 ?= gcc830
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/6.3.0)","")
  TOOLCHAIN                 ?= gcc630
else
  TOOLCHAIN                 ?= gcc492
endif
endif

ifeq ($(BL_PLAT_IS_UBUNTU),1)
# clang or gcc may or may not be available on platform, so check first
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-clang/12.0.1)","")
  TOOLCHAIN                 ?= clang1201
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/11.1.0)","")
  TOOLCHAIN                 ?= gcc1110
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-clang/8.0.1)","")
  TOOLCHAIN                 ?= clang801
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/8.3.0)","")
  TOOLCHAIN                 ?= gcc830
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-clang/3.9.1)","")
  TOOLCHAIN                 ?= clang391
endif
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/6.3.0)","")
  TOOLCHAIN                 ?= gcc630
else
  TOOLCHAIN                 ?= gcc492
endif
endif

ifeq (win, $(findstring win, $(OS)))
  TOOLCHAIN_DEFAULT         := msvc-default
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-msvc/vc14.1/BuildTools)","")
  TOOLCHAIN                 ?= vc141
else ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-msvc/vc14-update3/default)","")
  TOOLCHAIN                 ?= vc14
else
  TOOLCHAIN                 ?= vc12
endif
else
  TOOLCHAIN_DEFAULT         := gcc-default
ifeq ($(OS),ub14)
  TOOLCHAIN                 ?= clang35
else ifeq ($(OS),ub16)
  TOOLCHAIN                 ?= clang391
else ifeq ($(OS),ub18)
  TOOLCHAIN                 ?= clang801
else ifeq ($(OS),ub20)
  TOOLCHAIN                 ?= clang1201
else ifeq ($(OS),d156)
  TOOLCHAIN                 ?= clang730
else ifeq ($(OS),d17)
  TOOLCHAIN                 ?= clang1000
else ifeq ($(OS),d20)
  TOOLCHAIN                 ?= clang1205
else
  TOOLCHAIN                 ?= gcc492
endif
endif

#
# The TOOLS_PLATFORM macro is used to execute tools from the tree
# and since we don't dist the binaries for clang on ubXX platforms we need to force
# the toolchain to be gccXXX, so clang builds can work by default on ubXX platforms
#

ifeq ($(OS),ub12)
TOOLS_PLATFORM ?= $(OS)-$(ARCH)-gcc492-$(VARIANT)
else ifeq ($(OS),ub14)
TOOLS_PLATFORM ?= $(OS)-$(ARCH)-clang35-$(VARIANT)
# Due to a bug upload using platform = linux-ub14 fails. Putting temporary workaround
# till issue is fixed.
BL_PROP_PLAT := linux-ub12
else ifeq ($(OS),win7)
TOOLS_PLATFORM ?= $(OS)-$(ARCH)-vc12-$(VARIANT)
else
TOOLS_PLATFORM ?= $(PLAT)
endif

BL_PROP_ARCH := $(ARCH)

DEVENV_VERSION_TAG := invalid

ifeq ($(TOOLCHAIN),gcc492)
DEVENV_VERSION_TAG := devenv2
endif

ifeq ($(TOOLCHAIN),gcc630)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),gcc830)
DEVENV_VERSION_TAG := devenv4
endif

ifeq ($(TOOLCHAIN),gcc1110)
DEVENV_VERSION_TAG := devenv5
endif

ifeq ($(TOOLCHAIN),clang35)
DEVENV_VERSION_TAG := devenv2
endif

ifeq ($(TOOLCHAIN),clang391)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),clang801)
DEVENV_VERSION_TAG := devenv4
endif

ifeq ($(TOOLCHAIN),clang1201)
DEVENV_VERSION_TAG := devenv5
endif

ifeq ($(TOOLCHAIN),clang380)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),clang730)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),clang1000)
DEVENV_VERSION_TAG := devenv4
endif

ifeq ($(TOOLCHAIN),clang1205)
DEVENV_VERSION_TAG := devenv5
endif

ifeq ($(TOOLCHAIN),vc141)
DEVENV_VERSION_TAG := devenv4
endif

ifeq ($(TOOLCHAIN),vc14)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),vc12)
DEVENV_VERSION_TAG := devenv2
endif

ifneq (devenv, $(findstring devenv, $(DEVENV_VERSION_TAG)))
$(error The value '$(TOOLCHAIN)' of the TOOLCHAIN parameter is either invalid or the toolchain specified is no \
longer supported; the supported toolchains are: vc12, gcc492, gcc630, gcc830, gcc1110, \
clang35, clang391, clang380, clang801, clang730, clang1000, clang1201, clang1205)
endif

BL_DEVENV_JSON_SPIRIT_VERSION=4.08
BL_DEVENV_BOOST_VERSION=1.58.0-devenv2
BL_DEVENV_OPENSSL_VERSION=1.0.2d

ifeq ($(DEVENV_VERSION_TAG),devenv3)
BL_DEVENV_BOOST_VERSION=1.63.0
BL_DEVENV_OPENSSL_VERSION=1.1.0d
endif

ifeq ($(DEVENV_VERSION_TAG),devenv4)
BL_DEVENV_BOOST_VERSION=1.72.0
BL_DEVENV_OPENSSL_VERSION=1.1.1d
endif

ifeq ($(DEVENV_VERSION_TAG),devenv5)
BL_DEVENV_BOOST_VERSION=1.75.0
BL_DEVENV_OPENSSL_VERSION=1.1.1k
endif

ifeq ($(DEVENV_VERSION_TAG),devenv3)
CPPFLAGS += -DBL_DEVENV_VERSION=3
endif

ifeq ($(DEVENV_VERSION_TAG),devenv4)
CPPFLAGS += -DBL_DEVENV_VERSION=4
endif

ifeq ($(DEVENV_VERSION_TAG),devenv5)
CPPFLAGS += -DBL_DEVENV_VERSION=5
endif

BL_EXPECTED_BOOSTDIR = $(DIST_ROOT_DEPS3)/boost/$(BL_DEVENV_BOOST_VERSION)/$(PLAT:%-$(VARIANT)=%)
BL_EXPECTED_OPENSSLDIR = $(DIST_ROOT_DEPS3)/openssl/$(BL_DEVENV_OPENSSL_VERSION)/$(PLAT)
