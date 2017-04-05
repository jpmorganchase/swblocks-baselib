ifndef PYTHON_2_7_LATEST_INCLUDED
PYTHON_2_7_LATEST_INCLUDED = 1

PYTHONDIR   = $(DIST_ROOT_DEPS3)/python/2.7-latest/default
PYTHON      = python

ifeq (win, $(findstring win, $(OS)))
  PATH := $(PYTHONDIR):$(PATH)
endif

endif # PYTHON_2_7_LATEST_INCLUDED
