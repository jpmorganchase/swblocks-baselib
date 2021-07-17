ifndef BOOST_COMMON_INCLUDED
BOOST_COMMON_INCLUDED = 1

ifeq (, $(BOOSTDIR))
BOOSTDIR := $(DIST_ROOT_DEPS3)/boost/$(BL_DEVENV_BOOST_VERSION)/$(EXTPLAT:%-$(VARIANT)=%)
endif

CPPFLAGS += -DBOOST_ALL_NO_LIB
INCLUDE  += $(BOOSTDIR)/include
LIBPATH  += $(BOOSTDIR)/lib

# ugly hack to get first character of $(VARIANT)
INITIALS := d
V        := $(strip $(foreach v,$(INITIALS),$(if $(VARIANT:$v%=),,$v)))

LIBTAG   := -mt-s$(V)
ifneq ($(DEVENV_VERSION_TAG),devenv3)
ifeq (x86, $(ARCH))
ARCHTAG   := -x32
else
ARCHTAG   := -$(ARCH)
endif
endif
LDLIBS   += boost_date_time$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_system$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_thread$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_filesystem$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_locale$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_program_options$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_regex$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_random$(LIBTAG)$(ARCHTAG)
LDLIBS   += boost_unit_test_framework$(LIBTAG)$(ARCHTAG)

ifeq ($(BL_PLAT_IS_DARWIN),1)
# It looks like this is not automatically included in Darwin
LDLIBS   += iconv
endif

endif # BOOST_COMMON_INCLUDED
