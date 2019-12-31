# TODO: in devenv4 we need to exlude the boost regex library on clang when lining statically
# to a shared library due to a clang bug. Details here: https://bugs.llvm.org/show_bug.cgi?id=40484
#
# libboost_regex-mt-s-x64.a(regex.o): relocation R_X86_64_PC32 against undefined hidden symbol
# `_ZTCN5boost10wrapexceptISt13runtime_errorEE0_NS_16exception_detail10clone_implINS3_19error_info_injectorIS1_EEEE'
# can not be used when making a shared object

ifeq ($(TOOLCHAIN),clang800)
ifeq ($(VARIANT),release)
$(utf_baselib_plugin_ARTIFACT): LDLIBS_TMPVAR := $(LDLIBS)
$(utf_baselib_plugin_ARTIFACT): LDLIBS = $(filter-out boost_regex$(LIBTAG)$(ARCHTAG),$(LDLIBS_TMPVAR))
endif
endif
