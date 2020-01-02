ifndef JDK_COMMON_INCLUDED
JDK_COMMON_INCLUDED = 1

ifneq ("$(wildcard $(DIST_ROOT_DEPS3)/jdk/open-jdk/8/$(OS)-$(ARCH))","")

BL_JNI_ENABLED   := 1
ARCH_JDK          = $(OS)-$(ARCH)
JAVA_HOME        := $(DIST_ROOT_DEPS3)/jdk/open-jdk/8/$(ARCH_JDK)

$(info Building with BL_JNI_ENABLED = $(BL_JNI_ENABLED))
$(info Building with JAVA_HOME = $(JAVA_HOME))

JAR              = jar
JAVA	         = java
JAVAC            = javac

PATH            := $(JAVA_HOME)/bin:$(PATH)
INCLUDE         += $(JAVA_HOME)/include

ifeq (windows, $(findstring windows, $(BL_PROP_PLAT)))
INCLUDE		+= $(JAVA_HOME)/include/win32
else ifeq (linux, $(findstring linux, $(BL_PROP_PLAT)))
INCLUDE		+= $(JAVA_HOME)/include/linux
endif

export JAVA_HOME

endif # ifeq ($(BL_JNI_ENABLED),1)

endif # JDK_COMMON_INCLUDED
