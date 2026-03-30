PROG=   vis
.PATH:  ${.CURDIR}/src

SRCS=   vis.c

CFLAGS+= -I../../../include/ -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	vis.1

LINKS=  ${BINDIR}/vis ${BINDIR}/unvis

MLINKS= vis.1 unvis.1

.include <bsd.prog.mk>
