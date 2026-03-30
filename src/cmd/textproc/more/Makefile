PROG=	more
.PATH:	${.CURDIR}/src

SRCS=	more.c

# Kayoko Header Integration
CFLAGS+= -I${.CURDIR}/../../../include -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	more.1
LINKS=	${BINDIR}/more ${BINDIR}/moar
MLINKS= more.1 moar.1

.include <bsd.prog.mk>
