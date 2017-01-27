# doxygen variables
DOXYGEN  := doxygen
DOXCFG   := $(TOPDIR)doc/baselib.doxygen

doc: $(BLDDIR)/.doxygen

$(BLDDIR)/.doxygen: $(DOXCFG)
	@mkdir -p $(DESTDIR)$(PREFIX)/doc
	(cat $(DOXCFG); echo INPUT=$(SRCDIR); \
         echo OUTPUT_DIRECTORY=$(DESTDIR)$(PREFIX)/doc) | \
	  $(DOXYGEN) - 2>/dev/null
	@touch $@

.PHONY: doc
