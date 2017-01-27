CXXFLAGS += -O0

ifdef SANITIZE

ifeq (, $(filter $(SANITIZE), address thread))
$(error Bad value of SANITIZE variable, allowed values are "address" and "thread")
endif

CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS  += -fsanitize=$(SANITIZE)

ifeq (address, $(SANITIZE))
ifndef NO_STATIC_ASAN
LDFLAGS  += -static-libasan
endif
endif

ifeq (thread, $(SANITIZE))
CXXFLAGS += -pie
LDFLAGS  += -static-libtsan -pie
endif

endif

