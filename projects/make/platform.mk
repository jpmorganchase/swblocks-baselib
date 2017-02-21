ifeq ($(OS),Windows_NT)
  OS := win7
endif

# simple platform detection
ARCH           ?= x64
PLAT            = $(OS)-$(ARCH)-$(TOOLCHAIN)-$(VARIANT)
NONSTDARCH     := x86_64-linux-2.6-libc6
# separate variable for external dependencies
# to allow toolchains to rely on compatible platforms
# i.e. clang will use boost built with gcc
EXTPLAT    = $(PLAT)

ifeq (win, $(findstring win, $(OS)))
  NONSTDARCH   := x86_64-nt-6.0
  NONSTDARCH32 := ia32-nt-4.0

  ifneq ($(PROCESSOR_ARCHITEW6432),AMD64)
    ARCH := x86
  endif

  ifeq ($(ARCH),x86)
    NONSTDARCH := ia32-nt-4.0
  endif

  ifdef SANITIZE
     $(error SANITIZE can only be used with Linux builds)
  endif

  TRUNCATE_COMMAND := echo | set /p= >

  BL_PROP_PLAT := windows
else
  UNAME_R := $(shell uname -r)
  UNAME_S := $(shell uname -s)
  UNAME_MERGED=$(UNAME_S)-$(UNAME_R)

  ifeq (Darwin-15.6.0,$(findstring Darwin-15.6.0,$(UNAME_MERGED)))
    OS := d156
    BL_PROP_PLAT := darwin-d156
    BL_PLAT_IS_DARWIN := 1
  else ifeq (el5,$(findstring el5,$(UNAME_R)))
    OS := rhel5
    BL_PROP_PLAT := linux-rhel5
    BL_PLAT_IS_RHEL := 1
  else ifeq (el6,$(findstring el6,$(UNAME_R)))
    OS := rhel6
    BL_PROP_PLAT := linux-rhel6
    BL_PLAT_IS_RHEL := 1
  else ifeq (el7,$(findstring el7,$(UNAME_R)))
    OS := rhel7
    BL_PROP_PLAT := linux-rhel7
    BL_PLAT_IS_RHEL := 1
    EXTPLAT = rhel6-$(ARCH)-$(TOOLCHAIN)-$(VARIANT)
  else
    #
    # assume Ubuntu, but generally any other flavor which
    # will be detected via lsb_release command
    #
    LSB_RELEASE_DIST_ID := $(shell lsb_release -i)
    LSB_RELEASE_VERSION := $(shell lsb_release -r)

    ifeq (Ubuntu,$(findstring Ubuntu,$(LSB_RELEASE_DIST_ID)))
        ifeq (12.04,$(findstring 12.04,$(LSB_RELEASE_VERSION)))
            OS := ub12
        else ifeq (14.04,$(findstring 14.04,$(LSB_RELEASE_VERSION)))
            OS := ub14
        else ifeq (16.04,$(findstring 16.04,$(LSB_RELEASE_VERSION)))
            OS := ub16
        else
            $(error Unsupported Ubuntu Version)
        endif
        DPKGDEB = fakeroot dpkg-deb --build
        BL_PROP_PLAT := linux-$(OS)
        BL_PLAT_IS_UBUNTU := 1
    else
        $(error Unsupported Linux Release Distributor ID)
    endif
  endif

  # configure rpmbuild command on redhat machines
  ifeq (rhel, $(findstring rhel,$(PLAT)))
    RPMBUILD = rpmbuild -bb \
      --define "_builddir $(realpath $(BLDDIR))" \
      --define "_rpmdir %{_builddir}/rpms" \
      --define "_build_id $(BUILD_ID)" \
      --define "_release $(OS)"
  endif

  TRUNCATE_COMMAND := echo -n >
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

ifeq (win, $(findstring win, $(OS)))
  TOOLCHAIN_DEFAULT         := msvc-default
  TOOLCHAIN                 ?= vc12
else
  TOOLCHAIN_DEFAULT         := gcc-default
ifeq ($(OS),ub14)
  TOOLCHAIN                 ?= clang35
else ifeq ($(OS),ub16)
  TOOLCHAIN                 ?= clang391
else ifeq ($(OS),d156)
  TOOLCHAIN                 ?= clang730
else
  TOOLCHAIN                 ?= gcc492
endif
endif

DEVENV_VERSION_TAG := invalid

ifeq ($(TOOLCHAIN),gcc492)
DEVENV_VERSION_TAG := devenv2
endif

ifeq ($(TOOLCHAIN),gcc630)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),clang35)
DEVENV_VERSION_TAG := devenv2
endif

ifeq ($(TOOLCHAIN),clang391)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),clang730)
DEVENV_VERSION_TAG := devenv3
endif

ifeq ($(TOOLCHAIN),vc12)
DEVENV_VERSION_TAG := devenv2
endif

ifneq (devenv, $(findstring devenv, $(DEVENV_VERSION_TAG)))
$(error The value '$(TOOLCHAIN)' of the TOOLCHAIN parameter is either invalid or the toolchain specified is no longer supported; the supported toolchains are: vc12, gcc492, gcc630, clang35, clang391, clang730)
endif

BL_DEVENV_JSON_SPIRIT_VERSION=4.08
BL_DEVENV_BOOST_VERSION=1.58.0-devenv2
BL_DEVENV_OPENSSL_VERSION=1.0.2d

ifeq ($(DEVENV_VERSION_TAG),devenv3)
BL_DEVENV_BOOST_VERSION=1.63.0
BL_DEVENV_OPENSSL_VERSION=1.1.0d
endif
