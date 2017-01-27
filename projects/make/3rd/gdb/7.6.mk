ifndef GDB_7_6_INCLUDED
GDB_7_6_INCLUDED = 1

GDBDIR  := $(DIST_ROOT_DEPS2)/gdb/7.6/$(EXTPLAT:%-$(VARIANT)=%)-debug

ifneq (win, $(findstring win, $(OS)))
  PATH := $(GDBDIR)/bin:$(PATH)
endif

endif # GDB_7_6_INCLUDED
