ifeq ($(TOOLCHAIN),vc14)
PATH     := $(MSVC)/VC/bin/amd64:$(PATH)
else
PATH     := $(MSVC)/VC/bin/x86_amd64:$(PATH)
endif
LIBPATH  += $(MSVC)/VC/lib/amd64
LDFLAGS  += -machine:x64
