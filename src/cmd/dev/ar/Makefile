PROG=	ar
.PATH:	${.CURDIR}/src

SRCS=	ar.c

# Kayoko Header Integration
CFLAGS+= -I${.CURDIR}/../../../include -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	ar.1
#NOMAN=	yes

.include <bsd.prog.mk>
