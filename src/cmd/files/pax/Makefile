.CURDIR=	src
PROG=	pax
SRCS=	pax.c ../../../lib/archive.c

# Add include directory to header search path
CFLAGS+=-I../../../include

# Create hard links for the multicall binary
LINKS=	${BINDIR}/pax ${BINDIR}/tar \
	${BINDIR}/pax ${BINDIR}/cpio

# Create symbolic links for the manual pages
MLINKS=	pax.1 tar.1 \
	pax.1 cpio.1

.include <bsd.prog.mk>
