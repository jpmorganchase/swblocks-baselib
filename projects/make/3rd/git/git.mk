ifndef GIT_INCLUDED
GIT_INCLUDED = 1

# Note: git is already on the path for linux hosts

ifeq (rhel5, $(findstring rhel5, $(OS)))
  GIT := $(DIST_ROOT_DEPS1)/git/linux/rhel5/bin/git
endif

ifeq (rhel6, $(findstring rhel6, $(OS)))
ifneq ("$(wildcard $(DIST_ROOT_DEPS1)/git/linux/rhel6/bin/git)","")
  GIT := $(DIST_ROOT_DEPS1)/git/linux/rhel6/bin/git
else
  GIT ?= git
endif
endif

ifeq (rhel7, $(findstring rhel7, $(OS)))
ifneq ("$(wildcard $(DIST_ROOT_DEPS1)/git/linux/rhel6/bin/git)","")
  GIT := $(DIST_ROOT_DEPS1)/git/linux/rhel6/bin/git
else
  GIT ?= git
endif
endif

ifeq (ub12, $(findstring ub12, $(OS)))
  GIT := $(DIST_ROOT_DEPS1)/git/linux/ub12/bin/git
endif

ifeq (ub14, $(findstring ub14, $(OS)))
  GIT := $(DIST_ROOT_DEPS1)/git/linux/ub12/bin/git
endif

ifeq (win, $(findstring win, $(OS)))
ifeq ($(BL_WIN_ARCH_IS_X64),1)
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/git/latest/default/bin)","")
  GIT := $(DIST_ROOT_DEPS3)/git/latest/default/bin/git
else
  GIT := $(DIST_ROOT_DEPS1)/git/windows/win7-x64/Git/bin/git
endif
else
ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/git/latest/default-x86/bin)","")
  GIT := $(DIST_ROOT_DEPS3)/git/latest/default-x86/bin/git
else
  GIT := $(DIST_ROOT_DEPS1)/git/windows/win7-x64/Git/bin/git
endif
endif
endif

GIT ?= git

endif # GIT_INCLUDED
