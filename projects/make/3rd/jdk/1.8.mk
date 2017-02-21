ifndef ORACLE_JDK_1_8_INCLUDED
ORACLE_JDK_1_8_INCLUDED = 1

JAR	   		= jar
JAVA	   	= java
JAVAC	   	= javac

ifneq ($(BL_PLAT_IS_DARWIN),1)
JAVA_HOME 	:= $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8/$(NONSTDARCH)
PATH      	:= $(JAVA_HOME)/bin:$(PATH)
INCLUDE   	+= $(JAVA_HOME)/include
endif

ifeq (win, $(findstring win, $(OS)))
INCLUDE		+= $(JAVA_HOME)/include/win32
else ifneq ($(BL_PLAT_IS_DARWIN),1)
INCLUDE		+= $(JAVA_HOME)/include/linux
endif

export JAVA_HOME

endif # ORACLE_JDK_1_8_INCLUDED
