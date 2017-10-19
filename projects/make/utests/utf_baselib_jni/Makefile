include $(MKDIR)/3rd/jdk/1.8.mk

ifneq (win, $(findstring win, $(OS)))
  $(utf_baselib_jni_ARTIFACT): LDADD += $(JAVA_HOME)/jre/lib/amd64/server/libjsig.so
endif
