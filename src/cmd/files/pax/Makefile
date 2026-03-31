.CURDIR=    src
PROG=       pax

# Modular CXX files + the original C archive library
SRCS=       main.cc tar.cc cpio.cc paxlogic.cc ../../../lib/archive.c

# Add include directory to header search path
# We use CFLAGS for the C archive.c and CXXFLAGS for the .cc files
CFLAGS+=    -I../../../include
CXXFLAGS+=  -I../../../include -std=c++17

# Ensure the linker knows we are using C++ (required for some BSD make versions)
LDADD+=     -lstdc++

# Create hard links for the multicall binary
# These allow argv[0] to change based on how the user invokes the program
LINKS=      ${BINDIR}/pax ${BINDIR}/tar \
            ${BINDIR}/pax ${BINDIR}/cpio

# Create symbolic links for the manual pages
MLINKS=     pax.1 tar.1 \
            pax.1 cpio.1

.include <bsd.prog.mk>
