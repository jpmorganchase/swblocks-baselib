include $(MKDIR)/3rd/jdk/1.8.mk

ifneq (win, $(findstring win, $(OS)))
  LDADD += $(JAVA_HOME)/jre/lib/amd64/server/libjsig.so
endif

TEST_JNI_SRC_DIR = $(SRCDIR)/utests/utf_baselib_jni
TEST_JNI_CLASS_DIR = $(abspath $(BLDDIR)/generated/test/jni/classes)
TEST_JNI_JAR_DIR = $(abspath $(BLDDIR)/utests/utf_baselib_jni/jars)
TEST_JNI_JAR = $(TEST_JNI_JAR_DIR)/java-bridge.jar

$(TEST_JNI_JAR): $(TEST_JNI_SRC_DIR)/java/src/com/jpmc/swblocks/baselib/test/*.java
	mkdir -p $(TEST_JNI_CLASS_DIR)
	$(JAVAC) -d $(TEST_JNI_CLASS_DIR) $^
	mkdir -p $(TEST_JNI_JAR_DIR)
	$(JAR) cvf $@ -C $(TEST_JNI_CLASS_DIR) com

utf_baselib_jni: $(TEST_JNI_JAR)
