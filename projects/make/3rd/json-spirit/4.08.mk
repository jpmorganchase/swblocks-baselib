ifndef JSON_SPIRIT_4_08_INCLUDED
JSON_SPIRIT_4_08_INCLUDED = 1

# use json-spirit source directory for header-only implementation
# try DIST_ROOT_DEPS3 root first and if not there then fallback to
# DIST_ROOT_DEPS2

ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/json-spirit/4.08)","")
INCLUDE  += $(DIST_ROOT_DEPS3)/json-spirit/4.08/source
else
INCLUDE  += $(DIST_ROOT_DEPS2)/json-spirit/4.08/source
endif

endif # JSON_SPIRIT_4_08_INCLUDED
