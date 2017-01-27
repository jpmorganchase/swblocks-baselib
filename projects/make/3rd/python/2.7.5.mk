ifndef PYTHON_2_7_5_INCLUDED
PYTHON_2_7_5_INCLUDED = 1

PYTHONDIR   = $(DIST_ROOT_DEPS2)/python/2.7.5
PYTHON      = python

ifeq (win, $(findstring win, $(OS)))
  ifeq ($(TOOLCHAIN),vc12)
  EXTPLAT_REMAPPED := $(OS)-$(ARCH)-vc11-$(VARIANT)
  PATH := $(PYTHONDIR)/$(EXTPLAT_REMAPPED:%-$(VARIANT)=%):$(PATH)
  else
  PATH := $(PYTHONDIR)/$(EXTPLAT:%-$(VARIANT)=%):$(PATH)
  endif
else
  EXTPLAT_REMAPPED := $(EXTPLAT)
  ifeq ($(TOOLCHAIN),gcc492)
  EXTPLAT_REMAPPED := $(OS)-$(ARCH)-gcc48-$(VARIANT)
  endif
  ifeq ($(OS),ub14)
  EXTPLAT_REMAPPED := ub12-$(ARCH)-gcc48-$(VARIANT)
  else ifeq ($(OS),rhel7)
  EXTPLAT_REMAPPED := rhel6-$(ARCH)-gcc48-$(VARIANT)
  endif
  PATH := $(PYTHONDIR)/$(EXTPLAT_REMAPPED:%-$(VARIANT)=%)/bin:$(PATH)
endif

endif # PYTHON_2_7_5_INCLUDED
