CXXFLAGS += -DNDEBUG
# TODO: inlining is set to -Ob1 instead of -Ob2 to workaround a compiler bug in VS2013
# TODO: once the compiler is upgraded and / or the issue is understood we can revert back to -Ob2
CXXFLAGS += -O2 -Ob1 -Ot -Oi

#
# "Enhance Optimized Debugging" option helpful for debugging Release builds
#
CXXFLAGS += -Zo
