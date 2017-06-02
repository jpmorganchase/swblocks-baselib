ifndef ORACLE_JDK_1_8_INCLUDED
ORACLE_JDK_1_8_INCLUDED = 1

ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8)","")
BL_JNI_ENABLED := 1

JAR	   		= jar
JAVA	   	= java
JAVAC	   	= javac

JAVA_HOME 	:= $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8/$(NONSTDARCH)
PATH      	:= $(JAVA_HOME)/bin:$(PATH)
INCLUDE   	+= $(JAVA_HOME)/include

ifeq (windows, $(findstring windows, $(BL_PROP_PLAT)))
INCLUDE		+= $(JAVA_HOME)/include/win32
else ifeq (linux, $(findstring linux, $(BL_PROP_PLAT)))
INCLUDE		+= $(JAVA_HOME)/include/linux
endif

export JAVA_HOME
endif

endif # ORACLE_JDK_1_8_INCLUDED
