PROG=   ping
SRCS=   src/ping.cc

CXXFLAGS+= -std=c++20

CXXFLAGS+= -D_LITTLE_ENDIAN
# if you want big endian:
#CXXFLAGS+= -D_BIG_ENDIAN
LDADD+= -lstdc++ -lm

MAN=    ping.1
include ../../build.mk
