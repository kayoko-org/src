PROG=   sed
.PATH:  ${.CURDIR}/src ${.CURDIR}/../ed/src

SRCS=   sed.c buffer.c

CFLAGS+= -I../../../include/ -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	sed.1

.include <bsd.prog.mk>
