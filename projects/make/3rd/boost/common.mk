ifndef BOOST_COMMON_INCLUDED
BOOST_COMMON_INCLUDED = 1

CPPFLAGS += -DBOOST_ALL_NO_LIB
INCLUDE  += $(BOOSTDIR)/include
LIBPATH  += $(BOOSTDIR)/lib

# ugly hack to get first character of $(VARIANT)
INITIALS := d
V        := $(strip $(foreach v,$(INITIALS),$(if $(VARIANT:$v%=),,$v)))

LIBTAG   := -mt-s$(V)
LDLIBS   += boost_date_time$(LIBTAG)
LDLIBS   += boost_system$(LIBTAG)
LDLIBS   += boost_thread$(LIBTAG)
LDLIBS   += boost_filesystem$(LIBTAG)
LDLIBS   += boost_locale$(LIBTAG)
LDLIBS   += boost_program_options$(LIBTAG)
LDLIBS   += boost_regex$(LIBTAG)
LDLIBS   += boost_unit_test_framework$(LIBTAG)

ifeq ($(BL_PLAT_IS_DARWIN),1)
# It looks like this is not automatically included in Darwin
LDLIBS   += iconv
endif

endif # BOOST_COMMON_INCLUDED
