PROG=   ping
.PATH:  ${.CURDIR}/src

# Use .cpp for C++ sources
SRCS=   ping.cc

# CXXFLAGS for CXX compiler options
CXXFLAGS+= -I../../../include/ -Wall -Wextra -std=c++20
# POSIX.1-2008 compatibility
CXXFLAGS+= -D_POSIX_C_SOURCE=200809L


CXXFLAGS+= -D_LITTLE_ENDIAN
# if you want big endian:
#CXXFLAGS+= -D_BIG_ENDIAN

CXXFLAGS+= -D_BSD_SOURCE

# Ensure the BSD build system uses the C++ linker
LDADD+= -lstdc++ -lm

# Manual page configuration
MAN=    ping.1
#NOMAN=  yes

.include <bsd.prog.mk>
