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
BASELIB_JAR_SRC_FILE := $(BLDDIR)/../../java/utf_baselib_jni/build/libs/utf_baselib_jni.jar
BASELIB_JAR_DEST_DIR := $(BLDDIR)/utests/utf_baselib_jni/utf-baselib-jni-lib

utf_baselib_jni:
	$(GRADLE) $(GRADLE_ARGS) clean
	$(GRADLE) $(GRADLE_ARGS) jar
	@mkdir -p $(BASELIB_JAR_DEST_DIR)
	@cp $(BASELIB_JAR_SRC_FILE) $(BASELIB_JAR_DEST_DIR)
