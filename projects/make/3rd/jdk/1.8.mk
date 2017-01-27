ifndef ORACLE_JDK_1_8_INCLUDED
ORACLE_JDK_1_8_INCLUDED = 1

JAVA_HOME 	:= $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8/$(NONSTDARCH)
JAR	   		= jar
JAVA	   	= java
JAVAC	   	= javac
PATH      	:= $(JAVA_HOME)/bin:$(PATH)
INCLUDE   	+= $(JAVA_HOME)/include

ifeq (win, $(findstring win, $(OS)))
INCLUDE		+= $(JAVA_HOME)/include/win32
else
INCLUDE		+= $(JAVA_HOME)/include/linux
endif

export JAVA_HOME

endif # ORACLE_JDK_1_8_INCLUDED
