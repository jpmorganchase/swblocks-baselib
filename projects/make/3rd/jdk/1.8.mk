ifndef ORACLE_JDK_1_8_INCLUDED
ORACLE_JDK_1_8_INCLUDED = 1

ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8)","")
BL_JNI_ENABLED := 1

JAR	   		= jar
JAVA	   	= java
JAVAC	   	= javac

ARCH_JDK		:= $(NONSTDARCH)

ifeq ($(DEVENV_VERSION_TAG),devenv2)
ifeq (linux, $(findstring linux, $(BL_PROP_PLAT)))
ARCH_JDK		:= linux_x64
else
ARCH_JDK		:= windows_x64
endif
endif

JAVA_HOME 	:= $(DIST_ROOT_DEPS3)/oracle-jdk/latest-1.8/$(ARCH_JDK)

ifeq ($(DEVENV_VERSION_TAG),devenv2)
ifeq (windows, $(findstring windows, $(BL_PROP_PLAT)))
ifeq ($(ARCH),x86)
JAVA_HOME 	:= $(DIST_ROOT_DEPS3)/oracle-jdk/1.8.0.144/windows_x86
endif
endif
endif

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
