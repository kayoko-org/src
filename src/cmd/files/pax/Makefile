PROG=       pax

# Modular CXX files + the original C archive library
SRCS=       src/main.cc src/tar.cc src/cpio.cc src/pax.cc ../../../lib/archive.c

#LDFLAGS+=     -lstdc++

# Create hard links for the multicall binary
# These allow argv[0] to change based on how the user invokes the program
LINKS=      pax tar \
            pax cpio

# Create symbolic links for the manual pages
MLINKS=     pax.1 tar.1 \
            pax.1 cpio.1

include ../../build.mk
