create-install-root-dir:
	@mkdir -p $(DESTDIR)$(PREFIX)
	@mkdir -p $(DESTDIR)$(PREFIX)-tools
	
install-apps: apps create-install-root-dir
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)$(PREFIX)-tools/bin
	for f in $(APPS); do \
	  install -m0755 $(BLDDIR)/apps/$$f/$$f$(EXEEXT) $(DESTDIR)$(PREFIX)/bin; \
	  install -m0755 $(BLDDIR)/apps/$$f/$$f$(EXEEXT) $(DESTDIR)$(PREFIX)-tools/bin; \
	  install -m0755 $(BLDDIR)/apps/$$f/$$f$(DBGEXT) $(DESTDIR)$(PREFIX)/bin; \
	  if [[ -f $(BLDDIR)/apps/$$f/$$f$(EXEEXT).mf ]]; then \
	    install -m0755 $(BLDDIR)/apps/$$f/$$f$(EXEEXT).mf $(DESTDIR)$(PREFIX)/bin; \
	  fi; \
	done

install: all install-apps install-plugins install-modules create-install-root-dir
	if [ -f $(DESTDIR)$(PREFIX)/.revision ]; then \
		@rm -f $(DESTDIR)$(PREFIX)/.revision; \
	fi; \
	echo $(GITREV) > $(DESTDIR)$(PREFIX)/.revision
	@mkdir -p $(DESTDIR)$(PREFIX)/utests
	for f in $(BLDDIR)/utests/*; do \
	  t=$${f##*/}; \
	  if [[ $$t == "utf_baselib_plugin" ]]; then \
	    u=$${t}$(SOEXT); \
	  else  \
	    u=$${t}$(EXEEXT); \
	  fi; \
	  install -m0755 $$f/$${u//_/-} $(DESTDIR)$(PREFIX)/utests; \
	  install -m0644 $$f/$${t//_/-}*$(DBGEXT) $(DESTDIR)$(PREFIX)/utests; \
	  srcdata=$(BLDDIR)/utests/$${t}/$${t//_/-}-data; \
	  dstdata=$(DESTDIR)$(PREFIX)/utests/$${t//_/-}-data; \
	  if [[ -d $$srcdata ]]; then \
	    mkdir -p $$dstdata; \
	    cp -r $$srcdata/* $$dstdata/; \
	  fi; \
	  srclib=$(BLDDIR)/utests/$${t}/$${t//_/-}-lib; \
	  dstlib=$(DESTDIR)$(PREFIX)/utests/$${t//_/-}-lib; \
	  if [[ -d $$srclib ]]; then \
	    mkdir -p $$dstlib; \
	    cp -r $$srclib/* $$dstlib/; \
	  fi; \
	done

install-plugins: plugins create-install-root-dir
	for f in $(PLUGINS); do \
	  install -m0755 $(BLDDIR)/plugins/$$f/$$f$(SOEXT) $(DESTDIR)$(PREFIX)/lib/plugins; \
	  install -m0755 $(BLDDIR)/plugins/$$f/$$f$(DBGEXT) $(DESTDIR)$(PREFIX)/lib/plugins; \
	  if [[ -f $(BLDDIR)/plugins/$$f/$$f$(SOEXT).mf ]]; then \
	    install -m0755 $(BLDDIR)/plugins/$$f/$$f$(SOEXT).mf $(DESTDIR)$(PREFIX)/lib/plugins; \
	  fi; \
	done

install-modules: modules create-install-root-dir
	for f in $(MODULES); do \
		install -m0755 $(BLDDIR)/apps/$$f/$$f$(EXEEXT) $(DESTDIR)$(PREFIX)/lib/plugins; \
		install -m0644 $(BLDDIR)/apps/$$f/$$f$(DBGEXT) $(DESTDIR)$(PREFIX)/lib/plugins; \
		install -m0644 $(BLDDIR)/apps/$$f/$$f$(EXEEXT).mf $(DESTDIR)$(PREFIX)/lib/plugins; \
	done

.PHONY: install install-apps install-plugins install-modules create-install-root-dir
