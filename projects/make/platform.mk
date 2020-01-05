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

  ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
    BL_WIN_ARCH_IS_X64 := 1
  endif

	ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
    BL_WIN_ARCH_IS_X64 := 1
  endif

	ifneq ($(BL_WIN_ARCH_IS_X64),1)
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

  ifeq (Darwin-15.,$(findstring Darwin-15.,$(UNAME_MERGED)))
    OS := d156
    BL_PROP_PLAT := darwin-d156
    BL_PLAT_IS_DARWIN := 1
    $(info Detected OS is $(UNAME_MERGED) - i.e. OS X El Capitan)
  else ifeq (Darwin-16.,$(findstring Darwin-16.,$(UNAME_MERGED)))
    # for macOS Sierra we can safely fallback to the El Capitan binaries / devenv3
    OS := d156
    BL_PROP_PLAT := darwin-d156
    BL_PLAT_IS_DARWIN := 1
    $(info Detected OS is $(UNAME_MERGED) - i.e. mscOS Sierra)
  else ifeq (Darwin-17.,$(findstring Darwin-17.,$(UNAME_MERGED)))
    ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/boost/1.72.0)","")
      # for macOS High Sierra and devenv4
      OS := d17
      BL_PROP_PLAT := darwin-d17
      BL_PLAT_IS_DARWIN := 1
      $(info Detected OS is $(UNAME_MERGED) - i.e. mscOS High Sierra; devenv4)
    else
      # for macOS High Sierra we can safely fallback to the El Capitan binaries / devenv3
      OS := d156
      BL_PROP_PLAT := darwin-d156
      BL_PLAT_IS_DARWIN := 1
      $(info Detected OS is $(UNAME_MERGED) - i.e. mscOS High Sierra; devenv3)
    endif
  else ifeq (Darwin-18.,$(findstring Darwin-18.,$(UNAME_MERGED)))
    ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/boost/1.72.0)","")
      # for macOS Mojave and devenv4
      OS := d17
      BL_PROP_PLAT := darwin-d17
      BL_PLAT_IS_DARWIN := 1
      $(info Detected OS is $(UNAME_MERGED) - i.e. mscOS Mojave; devenv4)
    else
      # for macOS Mojave without devenv4 we can safely fallback to the El Capitan binaries / devenv3
      OS := d156
      BL_PROP_PLAT := darwin-d156
      BL_PLAT_IS_DARWIN := 1
      $(info Detected OS is $(UNAME_MERGED) - i.e. mscOS Mojave; devenv3)
    endif
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
        else ifeq (18.04,$(findstring 18.04,$(LSB_RELEASE_VERSION)))
            ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/toolchain-gcc/8.1.0)","")
                # This is devenv4
                OS := ub18
            else
                # TODO: temporary to make devenv3 work on Ubuntu 18.04
                OS := ub16
            endif
        else
            $(error Unsupported Ubuntu Version)
        endif
        DPKGDEB = fakeroot dpkg-deb --build
        UNAME_I := $(shell uname -i)
        ifeq ($(UNAME_I),i686)
        BL_PROP_PLAT_IS_32BIT := 1
        ARCH := x86
        endif
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
