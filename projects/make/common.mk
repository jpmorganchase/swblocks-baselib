########################################################################################
# Initialize and bootstrap the CI environment before we do anything else
#

ifeq (, $(CI_ENV_ROOT))
ifeq ("$(wildcard $(MKDIR)/ci-init-env.mk)","")
$(error Either ci-init-env.mk or CI_ENV_ROOT environment variable must be defined and point to the CI env root)
else
CI_ENV_MKDIR := $(MKDIR)
endif
else ifeq ("$(wildcard $(CI_ENV_ROOT)/projects/make/ci-init-env.mk)","")
$(error The CI env file $(CI_ENV_ROOT)/projects/make/ci-init-env.mk pointed by CI_ENV_ROOT does not exist)
else
CI_ENV_MKDIR := $(CI_ENV_ROOT)/projects/make
endif

# platform detection, sets PLAT, OS, ARCH, and default TOOLCHAIN
# this must be included before the ci-init-env.mk makefile as it needs
# to depend on this to init the CI environment roots
include $(MKDIR)/platform.mk

include $(CI_ENV_MKDIR)/ci-init-env.mk

# ======================================================================================
# The CI environment is expected to initialize some common roots (e.g. dist root for
# 3rd party tools and 3rd party deps etc)
#

ifeq (, $(DIST_ROOT_DEPS1))
$(error DIST_ROOT_DEPS1 environment variable must be defined from the CI environment init file)
endif
ifeq ("$(wildcard $(DIST_ROOT_DEPS1))","")
$(error The The root path $(DIST_ROOT_DEPS1) pointed by DIST_ROOT_DEPS1 does not exist)
endif

ifeq (, $(DIST_ROOT_DEPS2))
$(error DIST_ROOT_DEPS2 environment variable must be defined from the CI environment init file)
endif
ifeq ("$(wildcard $(DIST_ROOT_DEPS2))","")
$(error The The root path $(DIST_ROOT_DEPS2) pointed by DIST_ROOT_DEPS2 does not exist)
endif

ifeq (, $(DIST_ROOT_DEPS3))
$(error DIST_ROOT_DEPS3 environment variable must be defined from the CI environment init file)
endif
ifeq ("$(wildcard $(DIST_ROOT_DEPS3))","")
$(error The The root path $(DIST_ROOT_DEPS3) pointed by DIST_ROOT_DEPS3 does not exist)
endif

$(info Building with DIST_ROOT_DEPS1 = $(DIST_ROOT_DEPS1))
$(info Building with DIST_ROOT_DEPS2 = $(DIST_ROOT_DEPS2))
$(info Building with DIST_ROOT_DEPS3 = $(DIST_ROOT_DEPS3))

########################################################################################
# The CI environment should be bootstrapped at this point
#

# disable built-in rules for performance
override MAKEFLAGS += -r

# clear out suffixes, use pattern rules instead
.SUFFIXES:

# let make delete files on error
# (msvc sometimes leaves empty .obj files around)
.DELETE_ON_ERROR:

# set shell explicitly, rh uses bash by default, ub does not
SHELL   := /bin/bash

VARIANT	:= debug

ifeq (release, $(VARIANT))
override SANITIZE:=
endif

# so we can use the proper jdk when invoking Java tests
include $(MKDIR)/3rd/jdk/1.8.mk

# for python script wrappers (windows compiler, debug harness)
include $(MKDIR)/3rd/python/2.7.5.mk

# add git to the path for systems where it's external
include $(MKDIR)/3rd/git/git.mk

# project related directories
SRCDIR	    := $(TOPDIR)src
SCRIPTDIR   := $(TOPDIR)scripts
SETTINGSDIR := $(TOPDIR)settings
CERTSDIR    := $(TOPDIR)certs
PYTHONDIR   := $(SRCDIR)/python
BLDDIR	    := $(TOPDIR)bld/$(PLAT)
LOCALTMPDIR := $(TOPDIR)bld/$(PLAT)/tmp
DESTDIR     := $(TOPDIR)bld/install/$(PLAT)

# do not export temp directory paths when running from a virtual machine shared folder
# note that the temp dir environment variables must be pointing to an absolute path
ifeq (, $(findstring /media/sf_, $(realpath $(MKDIR))))
export TMP := $(realpath $(LOCALTMPDIR))
export TEMP := $(realpath $(LOCALTMPDIR))
export TMPDIR := $(realpath $(LOCALTMPDIR))
endif

# misc variables
HR = --------------------------------------------------------------------
EMPTY    :=
SPACE    := $(EMPTY) $(EMPTY)

# common variables and flags
LIBPATH  := $(subst $(if $(findstring ;,$(LIBPATH)),;,:), ,$(LIBPATH))
INCLUDE  := $(subst $(if $(findstring ;,$(INCLUDE)),;,:), ,$(INCLUDE))
INCLUDE  += $(SRCDIR)/versioning
INCLUDE  += $(SRCDIR)/include
INCLUDE  += $(SRCDIR)/utests/include

CPPFLAGS += -DBL_BUILD_PLATFORM=$(BL_PROP_PLAT)
CPPFLAGS += -DBL_BUILD_VARIANT=$(VARIANT)
CPPFLAGS += -DBL_BUILD_ARCH=$(BL_PROP_ARCH)
CPPFLAGS += -DBL_BUILD_OS=$(OS)
CPPFLAGS += -DBL_BUILD_TOOLCHAIN=$(TOOLCHAIN)

ifeq ($(ASIO_ENABLE_HANDLER_TRACKING),1)
CPPFLAGS += -DBOOST_ASIO_ENABLE_HANDLER_TRACKING
endif

# tools and variables for publishing releases
DATE           := $(shell date +%Y%m%d)
DATETIME	   := $(shell date +%Y%m%d-%H%M%S)
DATEBLDTIME    := $(shell date +%Y%m%d-build-%H%M%S)

BUILD_ID_FILE   = src/versioning/build/PluginBuildId.h
BUILD_ID        = $(shell awk '/^\#define *BL_PLUGINS_BUILD_ID/ \
                              { print $$3 }' $(BUILD_ID_FILE))
BUILD_RELEASE_DATE = $(shell awk '/^\#define *BL_PLUGINS_RELEASE_DATE/ \
                              { print $$3 }' $(BUILD_ID_FILE))
VERSION         = $(BUILD_RELEASE_DATE)-build-$(BUILD_ID)

# -u (unbuffered stdout/err) would help investigating hanging tests
DEBUG_HARNESS   = $(PYTHON) -u $(TOPDIR)scripts/debug_harness.py
ASAN_SYMBOLIZE  = $(TOPDIR)scripts/addr2line_symbolizer.sh
UTF_FLAGS        += --log_level=test_suite
UTF_FLAGS        += --catch_system_errors=no
UTF_FLAGS        += --build_info=yes

UTF_LOGS_DIR   ?= $(BLDDIR)/utflogs

# targets
APPS        := $(patsubst $(SRCDIR)/apps/%, %, $(wildcard $(SRCDIR)/apps/*))
PLUGINS     := $(patsubst $(SRCDIR)/plugins/%, %, $(wildcard $(SRCDIR)/plugins/*))
TESTAPPS    := $(patsubst $(SRCDIR)/tests/%, %, $(wildcard $(SRCDIR)/tests/*))
UTESTS      := $(patsubst $(SRCDIR)/utests/%, %, $(wildcard $(SRCDIR)/utests/utf*))

# toolchain setup - toolchain default, toolchain, arch/variant specific includes
# all includes are optional
-include $(MKDIR)/toolchain/$(TOOLCHAIN_DEFAULT).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN_DEFAULT)-$(ARCH).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN_DEFAULT)-$(VARIANT).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN_DEFAULT)-$(ARCH)-$(VARIANT).mk

ifeq (clang, $(findstring clang, $(TOOLCHAIN)))
-include $(MKDIR)/toolchain/clang.mk
endif

-include $(MKDIR)/toolchain/$(TOOLCHAIN).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN)-$(ARCH).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN)-$(VARIANT).mk
-include $(MKDIR)/toolchain/$(TOOLCHAIN)-$(ARCH)-$(VARIANT).mk

# common dependencies
include $(MKDIR)/3rd/boost/$(BL_DEVENV_BOOST_VERSION).mk
include $(MKDIR)/3rd/openssl/$(BL_DEVENV_OPENSSL_VERSION).mk
include $(MKDIR)/3rd/json-spirit/$(BL_DEVENV_JSON_SPIRIT_VERSION).mk
include $(MKDIR)/3rd/gdb/7.6.mk

# include project private definitions (if such exists)
-include $(CI_ENV_MKDIR)/ci-private.mk
-include $(MKDIR)/project-private.mk

TARGETS := $(APPS) $(PLUGINS) $(TESTAPPS) $(UTESTS)

# publishable artifacts (plug-ins and application modules)
PUBLISHABLES := $(PLUGINS) $(MODULES)

# bl-tool (for path removal)
BL_TOOL = $(DIST_ROOT_DEPS3)/baselib-tools/prod/$(TOOLS_PLATFORM)/bin/bl-tool$(EXEEXT)
RMPATH = $(if $(findstring win, $(OS)),$(BL_TOOL) path remove --force --path,rm -rf path)

all: apps dotnet-apps plugins testapps modules apis jni java utests

apps: $(APPS)

dotnet-apps: $(DOTNET_APPS)

plugins: $(PLUGINS)

testapps: $(TESTAPPS)

modules: $(MODULES)

apis: $(APIS)

jni: $(JNITARGETS)

java: $(JAVATARGETS)

utils: $(UTILTARGETS)

utests: $(UTESTS)

testutf: $(UTESTS:%=test_%)

testjni: $(JNITARGETS:%=test_%)

testjava: $(JAVATARGETS:%=test_%)

test: testutf testjni testjava

help:
	$(info $(HR))
	$(info Targets for this project)
	$(info $(HR))
	$(info Global targets)
	$(info $(SPACE)all - build all targets)
	$(info $(SPACE)apps - build applications sub-targets)
	$(info $(SPACE)apis - build api targets)
	$(info $(SPACE)clean - clean all build artifacts)
	$(info $(SPACE)dm-gen - re-generate the datamodel, jni and java generated files)
	$(info $(SPACE)jni-dm-gen - re-generate the jni generated files)
	$(info $(SPACE)java-dm-gen - re-generate the java generated files)
	$(info $(SPACE)dotnet - build Windows .NET targets)
	$(info $(SPACE)install - prepare and install all release artifacts)
	$(info $(SPACE)jni - build JNI sub-targets)
	$(info $(SPACE)msi - create a Windows-msi-based installer)
	$(info $(SPACE)plugins - build plugins sub-targets)
	$(info $(SPACE)testapps - build testapps sub-targets)
	$(info $(SPACE)test - build and test all targets)
	$(info $(SPACE)testutf - build and run utf test sub-targets)
	$(info $(SPACE)testjni - build and run jni test sub-targets)
	$(info $(SPACE)testjava - build and run java test sub-targets)
	$(info $(SPACE)utests - build utf test sub-targets)
	$(info $(SPACE))
	$(info Normal targets)
	$(foreach t,$(APPS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(foreach t,$(DOTNET_APPS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(foreach t,$(APIS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(foreach t,$(PLUGINS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(foreach t,$(TESTAPPS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(foreach t,$(UTILTARGETS), \
		$(info $(SPACE)$(t) - build the $(t) utility project))
	$(foreach t,$(JNITARGETS), \
		$(info $(SPACE)$(t) - build the $(LIBPREFIX)$(t)$(SOEXT) jni/library-related project))
	$(foreach t,$(JAVATARGETS), \
		$(info $(SPACE)$(t) - build the $(t) java/library-related project))
	$(info $(SPACE))
	$(info Unit-test targets)
	$(foreach t,$(UTESTS), \
		$(info $(SPACE)$(t) - build the $(t) project))
	$(info $(SPACE))
	$(info Run Unit-test targets)
	$(foreach t,$(UTESTS), \
		$(info $(SPACE)test_$(t) - build and test the $(t) project))
	$(foreach t,$(JNITARGETS), \
		$(info $(SPACE)test_$(t) - build and test the $(LIBPREFIX)$(t)$(SOEXT) jni/library-related project))
	$(foreach t,$(JAVATARGETS), \
		$(info $(SPACE)test_$(t) - build and test the $(t) java/library-related project))
	$(info $(HR))
	@:

clean:
	@$(RMPATH) $(abspath $(BLDDIR))
	@$(RMPATH) $(abspath $(DESTDIR)$(PREFIX))

mktmppath:
	$(info Creating temporary path $(LOCALTMPDIR))
	@mkdir -p $(LOCALTMPDIR)

mkbuildpath:
	$(info Creating build path $(BLDDIR))
	@mkdir -p $(BLDDIR)

mkutflogspath:
	$(info Creating utf logs path $(UTF_LOGS_DIR))
	@mkdir -p $(UTF_LOGS_DIR)

build-plugins: $(PLUGINS)

# targets which are always made
.PHONY: all clean apps dotnet-apps plugins modules apis jni java utests testutf testjni testjava test help rpm msi $(TARGETS) build-plugins mktmppath mkbuildpath mkutflogspath

# generate jni-libs rules
define JNILIBTEMPLATE
  # include any project specific makefiles
  -include $$(wildcard $$(MKDIR)/*/$(1)/Makefile)

  # add generic target name for library and optional jar
  $(1): $(BLDDIR)/libs/$(1)/$(LIBPREFIX)$(1)$(SOEXT) $(BLDDIR)/jars/$(1).jar

  # add test target for building and running the tests
  test_$(1): $(1)_test
  deploy_$(1): $(1)_deploy
endef

# generate java project rules
define JAVATEMPLATE
  # include any project specific makefiles
  -include $$(wildcard $$(MKDIR)/libs/java/$(1)/Makefile)

  # add generic target name for the jar
  $(1): $(1)_package

  # add test target for building and running the tests
  test_$(1): $(1)_test
  deploy_$(1): $(1)_deploy
endef

# generate targets rules
define TEMPLATE
  # variables for program sources, objects, and executables
  # determined by conventions, can be overridden by project
  # "generated" dir is filtered-out to avoid depending on the
  # file system sort order
  $(1)_SRCDIR   = $$(firstword $$(filter-out $$(SRCDIR)/generated/$(1),$$(wildcard $$(SRCDIR)/*/$(1))))
  $(1)_BLDDIR   = $$(patsubst $$(SRCDIR)/%,$$(BLDDIR)/%, $$($(1)_SRCDIR))
  $(1)_SOURCES  = $$(wildcard $$($(1)_SRCDIR)/*.cpp)
  # all object files
  $(1)_OBJECTS  = $$($(1)_SOURCES:$$(SRCDIR)/%.cpp=$$(BLDDIR)/%$$(OBJEXT))
  # the primary artifact (program or library)
  $(1)_ARTIFACT = $$(addsuffix \
    $$(if $$(findstring ain.cpp, $$($(1)_SOURCES)),$$(EXEEXT),$$(SOEXT)), \
    $$($(1)_BLDDIR)/$$(subst _,-,$(1)))

  # include any project specific makefiles
  -include $$(wildcard $$(MKDIR)/*/$(1)/Makefile)

  ifneq ($$(MAKECMDGOALS),clean)
    # include compiler generated dependency info
    -include $$($(1)_OBJECTS:%$$(OBJEXT)=%.d)
    -include $$($(1)_ARTIFACT).d
  endif

  # add program-specific libraries to global defaults
  $$($(1)_ARTIFACT): LDLIBS += $$($(1)_LIBS)

  # setup program dependencies on objects and libs
  $$($(1)_ARTIFACT): $$($(1)_OBJECTS)
	@echo "Linking $(1)..."
	@mkdir -p $$(@D)
	$$(LINK.$$(TOOLCHAIN))

  # if test data directory exists create rule to copy files from it
  $(1)_SRC_DATADIR = $$($(1)_SRCDIR)/data
  $(1)_BLD_DATADIR = $$($(1)_BLDDIR)/$$(subst _,-,$(1))-data

  ifneq (,$$(wildcard $$($(1)_SRC_DATADIR)/*))
    $$($(1)_BLD_DATADIR): $$($(1)_SRC_DATADIR)/*
	@echo "Copying $(1) test data..."
	mkdir -p $$($(1)_BLD_DATADIR)
	cp -r $$($(1)_SRC_DATADIR)/* $$($(1)_BLD_DATADIR)/

    $(1): $$($(1)_BLD_DATADIR)
  endif

  # add generic target name for program
  $(1): $$($(1)_ARTIFACT)

  # add test target for building and running program
  ifneq (,$$(findstring utf,$(1)))
    $$($(1)_ARTIFACT): CPPFLAGS += -DBL_IS_UNIT_TEST_BINARY
    $$($(1)_ARTIFACT): CPPFLAGS += -DBL_ENABLE_EXCEPTION_HOOKS
    test_$(1)_begin: | mktmppath mkutflogspath $(1)
	$$(info $$(HR))
	@echo $$(HR) > $$(UTF_LOGS_DIR)/$(1).log
	$$(info Starting $(1) at $$(shell date))
	@echo Starting $(1) at $$(shell date) >>$$(UTF_LOGS_DIR)/$(1).log
	$$(info $$(HR))
	@echo $$(HR) >>$$(UTF_LOGS_DIR)/$(1).log

    test_$(1)_run: test_$(1)_begin
      ifeq (,$$(findstring $$(SOEXT), $$($(1)_ARTIFACT)))
        ifdef SANITIZE
	$(DEBUG_HARNESS) $$($(1)_ARTIFACT) 2>&1 | $(ASAN_SYMBOLIZE) 2> /dev/null > $(1)_asan.log
	@cat $(1)_asan.log >> $(1).log
	@if [ `grep -c AddressSanitizer $(1)_asan.log` -gt 0 ]; then \
		echo -e "\n*** Memory errors detected"; \
	fi
        else
            ifeq (utf_baselib,$$(findstring utf_baselib, $$($(1)_ARTIFACT)))
	@$$(DEBUG_HARNESS) $$($(1)_ARTIFACT) $$(UTF_FLAGS) >>$$(UTF_LOGS_DIR)/$(1).log
            else ifeq (utf_shared,$$(findstring utf_shared, $$($(1)_ARTIFACT)))
	@$$(DEBUG_HARNESS) $$($(1)_ARTIFACT) $$(UTF_FLAGS) >>$$(UTF_LOGS_DIR)/$(1).log
            else
	@$$(DEBUG_HARNESS) $$($(1)_ARTIFACT) $$(UTF_FLAGS) $$(UTF_FLAGS_EXTRA) >>$$(UTF_LOGS_DIR)/$(1).log
            endif
        endif
      endif

    test_$(1)_end: test_$(1)_run
	$$(info $$(HR))
	@echo $$(HR) >>$$(UTF_LOGS_DIR)/$(1).log
	$$(info Completed $(1) at $$(shell date))
	@echo Completed $(1) at $$(shell date) >>$$(UTF_LOGS_DIR)/$(1).log
	$$(info $$(HR))
	@echo $$(HR) >>$$(UTF_LOGS_DIR)/$(1).log

    test_$(1): test_$(1)_end
    .PHONY: test_$(1)_begin test_$(1)_run test_$(1)_end
  endif
endef

# generate util rules
define UTILTEMPLATE
  # include any project specific makefiles
  -include $$(wildcard $$(MKDIR)/utils/$(1)/Makefile)
endef

$(foreach T,$(JNITARGETS),$(eval $(call JNILIBTEMPLATE,$(T))))
$(foreach T,$(JAVATARGETS),$(eval $(call JAVATEMPLATE,$(T))))
$(foreach T,$(TARGETS),$(eval $(call TEMPLATE,$(T))))
$(foreach T,$(UTILTARGETS),$(eval $(call UTILTEMPLATE,$(T))))

ifneq "$(wildcard $(TOPDIR).git)" ""
include $(MKDIR)/git-support.mk
endif

# include project private special targets (if such exists)
-include $(MKDIR)/project-private-post.mk

# include files containing other targets
-include $(MKDIR)/doc.mk
-include $(MKDIR)/install.mk

# include CI env specific files containing other targets
-include $(CI_ENV_MKDIR)/prebuild.mk
-include $(CI_ENV_MKDIR)/publish.mk
-include $(CI_ENV_MKDIR)/rpm.mk
-include $(CI_ENV_MKDIR)/msi.mk
-include $(CI_ENV_MKDIR)/deb.mk
-include $(CI_ENV_MKDIR)/postbuild.mk
