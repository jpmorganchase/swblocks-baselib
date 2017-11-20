include $(MKDIR)/3rd/jdk/1.8.mk

ifeq ($(DEVENV_VERSION_TAG),devenv2)
include $(MKDIR)/3rd/gradle/1.6.mk
else
include $(MKDIR)/3rd/gradle/latest.mk
endif

ifneq (win, $(findstring win, $(OS)))
  $(utf_baselib_jni_ARTIFACT): LDADD += $(JAVA_HOME)/jre/lib/amd64/server/libjsig.so
endif

$(info Building with Gradle = $(GRADLE))

GRADLE_ARGS := --build-file $(JAVADIR)/build.gradle

utf_baselib_jni:
	$(GRADLE) $(GRADLE_ARGS) clean
	$(GRADLE) $(GRADLE_ARGS) jar
